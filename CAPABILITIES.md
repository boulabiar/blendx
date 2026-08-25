# BlendX capabilities and performance

## Performance

Latest 5,000-message headless chat benchmark:

| Measurement | Result |
|---|---:|
| Retained native nodes | 97 |
| Initial React/native mount | ~5.0 ms |
| Steady renderer p50 | ~0.13 ms |
| Steady renderer p95 | ~0.67 ms |
| Worst sampled frame | ~2.6 ms |
| Yoga per paint-only update | 0 ms |
| Paint per update | ~0.32 ms final sample |
| Nodes painted per animation | 8 |
| Pixels repainted | 7,378 of 955,200 |
| Native mutations per animation | 2 |

The transcript remains lightweight JavaScript data. `VirtualList` mounts only
visible rows plus overdraw into React and the native tree. Animated
canvas/progress changes still produce small dirty regions instead of repainting
the entire window.

These are packed-Hermes renderer-only headless numbers. In the forwarded X11 window test,
SDL presentation took roughly 15–50 ms and dominated the frame; BlendX layout
and painting remained below approximately 1 ms. On a local desktop,
presentation should be considerably cheaper, but it needs benchmarking there.

The animation currently runs every 32 ms, about 31 FPS. It can be changed to
16 ms for a 60 FPS workload.

### Visual component scaling

The `visual-stress` benchmark deliberately retains compound cards containing
containers, text, badges, progress bars, and canvas charts. A representative
headless run on the same development machine measured:

| Profile | Widgets | Native nodes | Actual FPS | Native p50 | Native p95 |
|---|---:|---:|---:|---:|---:|
| Sparse 5%, 120 Hz target | 250 | 2,657 | ~117 | ~0.63 ms | ~1.04 ms |
| Layout 5%, 120 Hz target | 250 | 2,657 | ~120 | ~1.63 ms | ~1.98 ms |
| Scroll, 120 Hz target | 250 | 2,657 | ~120 | ~1.44 ms | ~1.84 ms |
| Dense 100%, 120 Hz target | 250 | 2,666 | ~23 | ~1.86 ms | ~2.51 ms |
| Native progress, 120 Hz target | 250 | 2,666 | ~123* | ~1.11 ms | ~1.45 ms |
| Sparse 1%, 120 Hz target | 1,000 | 10,244 | ~120 | ~2.01 ms | ~3.00 ms |
| Sparse 5%, 120 Hz target | 1,000 | 10,253 | ~115 | ~0.18 ms | ~0.73 ms |

These results use the persistent Yoga layout tree. The earlier custom recursive
layout pass measured roughly 15 ms at 250 sparse widgets and 54–59 ms at 1,000.
Yoga is now skipped for paint-only commits, box propagation follows Yoga's
changed-layout flags, and virtual lists/overlays are tracked directly instead
of discovered with a whole-tree walk. Subtree paint bounds prune unrelated
rows, while fragmented damage is consolidated when repeated traversal would be
more expensive. Stable-shaped style updates use numeric patches, and
`layoutContain` keeps content-only changes inside fixed card geometry. Dense
updates now spend substantially more time producing React/Hermes subtrees than
in native layout or painting.

The native profile animates 250 progress values without React commits. Its
reported ~123 FPS comprises the 120 Hz native timeline plus occasional
telemetry commits; it has 250 active animations and zero rolling 8.33 ms misses.

`actualFps` includes the application update cadence and React/Hermes work;
native percentiles cover layout, Blend2D paint, and presentation only. Headless
presentation excludes the desktop compositor. Use identical profile, widget
count, update share, target rate, and duration when comparing commits.
`getStats()` additionally separates batch decoding, Yoga calculation, box
synchronization, specialized virtual/overlay layout, synchronous bridge and
React commit duration, active animations, and both 60/120 Hz budget misses.

### Memory use

The earlier paint-only virtualization retained every row and measured:

| Messages | Retained native nodes | Maximum RSS |
|---:|---:|---:|
| 100 | 287 | 72,912 KiB (~71 MiB) |
| 1,000 | 2,312 | 90,448 KiB (~88 MiB) |
| 5,000 | 11,312 | 160,516 KiB (~157 MiB) |

After memory-windowing, the packed 5,000-message chat retains 97 native nodes,
mounts in about 5 ms, and peaks at 22,664 KiB (~22 MiB). Row offsets preserve
the complete scroll extent while only the active window is mounted. Known
variable heights, follow-tail behavior, imperative index/offset scrolling,
range reporting, and draggable native scrollbars are supported.

Scrolling now:

- Accumulates repeated wheel events
- Uses 120 px per conventional mouse notch
- Preserves proportional fractional trackpad deltas
- Interpolates toward the accumulated target
- Only processes actively scrolling nodes

This is BlendX-specific smoothing. GPUix delegates scrolling to GPUI's native
list implementation.

## Implemented elements

### Core

- `div`: retained flex container
- `text`: shaped UTF-8 text
- `virtual-list`: low-level retained uniform-row viewport
- `VirtualList`: React/native memory-windowed list with variable row sizes
- `button`: clickable styled container
- `badge`: styled container
- `separator`: horizontal rule

`button` and `badge` share the generic container painter. Buttons support
Enter/Space activation and appear in the inspectable accessibility tree;
neither element applies automatic visual styling.

### Graphics

- `img`
  - Cached file decoding
  - `fill`, `contain`, `cover`, `scaleDown`, and `none` fit modes
  - Uses Blend2D's available raster codecs
- `svg`
  - Inline SVG strings or cached files
  - `viewBox`
  - Paths with `M/L/H/V/C/S/Q/T/A/Z`
  - Rectangles, circles, lines, polylines, and polygons
  - Basic fill and stroke handling
- `canvas`
  - Retained command arrays
  - Filled and stroked rectangles
  - Lines and circles
  - Text commands
  - Rounded rectangles

### Content

- `markdown`
  - Headings
  - Lists
  - Quotes
  - Multiline blocks
  - Bold runs, inline code, and link labels
- `code`
  - Header/language label
  - Line numbers
  - Multiline rendering
  - Lightweight token-based syntax coloring
- `diff`
  - Added, removed, and hunk line coloring
  - Line backgrounds

### Controls and overlays

- `progress`
- `input`
- `textarea`
- `anchored`
- Absolute and fixed positioning

Input supports:

- Focus and blur
- UTF-8 text entry
- Mouse-positioned caret and selection replacement
- UTF-8 Backspace/Delete and arrow/Home/End navigation
- Multiline Up/Down navigation
- Clipboard cut/copy/paste and select-all
- Undo/redo
- Multiline Enter
- Ctrl+Enter submit
- Change, submit, and key-down events
- Placeholder and read-only state
- Password masking
- SDL IME composition display

Anchored overlays support:

- Explicit screen anchor point
- Element-ref (`anchorId`) anchoring
- Top, right, bottom, and left placement
- Start, center, and end alignment
- Gap and x/y offset

## Layout and rendering support

Implemented:

- Row and column flex layout
- `flexGrow` and `flexShrink`
- Alignment and justification
- Gaps
- Per-edge padding and margins
- Fixed and percentage dimensions
- Min/max dimensions
- Borders, radius, and opacity
- Clipping and vertical scrolling
- Absolute positioning
- Viewport-relative fixed positioning
- Dirty-region coalescing
- Partial painting and partial SDL presentation
- Blend2D multithreaded contexts and runtime SIMD/JIT selection

## Important limitations

### Text and Markdown

- Ordinary text supports `normal`, `pre`, `preWrap`, and `nowrap` layout
- No multi-font fallback chain
- No bidi or complex multi-font shaping
- Selectable text supports mouse ranges and clipboard copy, but not rich spans
- Markdown is not a CommonMark parser
- No Markdown tables, images, or nested block layout
- Code highlighting is a lightweight lexer rather than a grammar parser
- `wordDiff` is accepted but does not calculate word-level changes

### Virtual lists

- Variable sizes currently come from `getItemHeight`; changes to that height
  model preserve the visible scroll anchor, but automatic post-layout
  measurement is not yet implemented
- No overscroll/bounce physics
- The low-level `virtual-list` host element remains uniform-height; applications
  should normally use the `VirtualList` component

### Input and accessibility

- Selection painting is single-font and does not expose platform-native handles
- IME pre-edit text is displayed, but candidate-window positioning is not exposed
- Accessibility roles, labels, values, checked/selected/disabled state, and
  bounds are available through `getAccessibilityTree()`; native Windows UI
  Automation, macOS NSAccessibility, and Linux AT-SPI adapters remain
  platform-specific follow-up work

### Images and SVG

- Images load synchronously on first use
- No HTTP/URL fetching
- No animated images
- `alt` is exposed through the accessibility tree
- No rounded image clipping yet
- SVG lacks gradients, patterns, filters, masks, `<text>`, CSS, and transforms
- SVG fill/stroke inheritance is intentionally simplified

### Canvas

- Command arrays cross the JavaScript/native boundary when changed
- No imperative drawing context
- No paths, gradients, compositing modes, or image commands yet
- No cached display-list diffing inside an updated command array

### Interaction and components

- Hover/active styles, Tab traversal, button activation, outside presses,
  element refs, pointer and element anchoring, menus, dialogs, toasts, tabs,
  disclosure controls, selection controls, and floating controls are implemented
- Pointer capture exists for sliders and scrollbars but has no public generic API
- No double-click event or general drag-and-drop primitive
- The `anchor` named-point property is accepted but not yet used
- Progress values and opacity can use the native 120 Hz timeline through
  `animateValue`/`animateOpacity`; general `motion` transforms remain
  frame-driven from React

### Layout

Ordinary flex layout is backed by Yoga, but BlendX currently exposes only a
practical subset of Yoga/CSS:

- No wrapping
- No baseline alignment
- No `flexBasis`
- No per-item `alignSelf`
- Simplified min-content sizing
- No grid
- No transforms
- `zIndex` is supported within a retained parent
- No percentage positioning offsets

The implemented examples remain GPUix-inspired adaptations rather than
source-compatible execution of GPUix applications. The deepest remaining work
is platform accessibility, automatic variable-row measurement, full
CommonMark/GFM, font fallback/bidi shaping, and a general transform/compositor
animation timeline.
