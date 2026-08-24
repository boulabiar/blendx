# BlendX capabilities and performance

## Performance

Latest 5,000-message headless chat benchmark:

| Measurement | Result |
|---|---:|
| Retained native nodes | 97 |
| Initial React/native mount | ~5 ms |
| Steady renderer p50 | ~0.45 ms |
| Steady renderer p95 | ~0.92 ms |
| Worst sampled frame | ~2.3 ms |
| Layout per update | ~0.20–0.55 ms |
| Paint per update | ~0.10–0.52 ms |
| Nodes painted per animation | 50 |
| Pixels repainted | 7,480 of 955,200 |
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
- `motion` is declarative and frame-driven from React rather than a native
  compositor/timeline

### Layout

This is a practical flex subset, not Yoga or full CSS:

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
CommonMark/GFM, font fallback/bidi shaping, and native animation timelines.
