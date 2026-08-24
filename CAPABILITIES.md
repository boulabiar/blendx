# BlendX capabilities and performance

## Performance

Latest 5,000-message headless chat benchmark:

| Measurement | Result |
|---|---:|
| Retained native nodes | 11,312 |
| Initial React/native mount | ~168 ms |
| Steady renderer p50 | ~0.30 ms |
| Steady renderer p95 | ~0.52 ms |
| Worst sampled frame | ~3.1 ms |
| Layout per update | ~0.14–0.16 ms |
| Paint per update | ~0.10–0.34 ms |
| Nodes painted per animation | 50 |
| Pixels repainted | 7,480 of 955,200 |
| Native mutations per animation | 2 |

The transcript retains all 5,000 messages, but the virtual list only lays out
and paints visible rows plus overdraw. Animated canvas/progress changes produce
small dirty regions instead of repainting the entire window.

These are renderer-only headless numbers. In the forwarded X11 window test,
SDL presentation took roughly 15–50 ms and dominated the frame; BlendX layout
and painting remained below approximately 1 ms. On a local desktop,
presentation should be considerably cheaper, but it needs benchmarking there.

The animation currently runs every 32 ms, about 31 FPS. It can be changed to
16 ms for a 60 FPS workload.

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
- `virtual-list`: uniform-height virtualization with overdraw
- `button`: clickable styled container
- `badge`: styled container
- `separator`: horizontal rule

`button` and `badge` currently share the generic container implementation.
They do not yet provide automatic default styling, keyboard activation, or
accessibility semantics.

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
- `code`
  - Header/language label
  - Line numbers
  - Multiline rendering
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
- Backspace at the end
- Multiline Enter
- Ctrl+Enter submit
- Change, submit, and key-down events
- Placeholder and read-only state
- End-position caret

Anchored overlays support:

- Explicit screen anchor point
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

- No word wrapping for ordinary `text`
- No font fallback
- No bidi or complex multi-font shaping
- No selection, clipboard, or links
- Markdown is not a CommonMark parser
- Inline `**bold**`, emphasis, and inline code are currently displayed literally
- No Markdown tables, images, or nested block layout
- Code has no true syntax highlighting
- `wordDiff` is accepted but does not calculate word-level changes

### Virtual lists

- Rows must use a uniform `itemHeight`
- `estimatedItemHeight` currently aliases the uniform height
- No variable-height row measurement
- No scroll-to-index public React API
- No visible scrollbar or dragging
- No overscroll/bounce physics

The original GPUix chat uses estimated variable-height rows, so the BlendX port
uses fixed 112 px message slots instead.

### Input

- Caret is always at the end
- No mouse text selection
- No arrow, Home, or End navigation
- No Delete or selection replacement
- No undo/redo or clipboard
- No IME composition
- No password mode
- No platform accessibility node

### Images and SVG

- Images load synchronously on first use
- No HTTP/URL fetching
- No animated images
- `alt` is stored but not visually or accessibly exposed
- No rounded image clipping yet
- SVG lacks gradients, patterns, filters, masks, `<text>`, CSS, and transforms
- SVG fill/stroke inheritance is intentionally simplified

### Canvas

- Command arrays cross the JavaScript/native boundary when changed
- No imperative drawing context
- No paths, gradients, compositing modes, or image commands yet
- No cached display-list diffing inside an updated command array

### Interaction and components

- No hover/active pseudo-styles
- No keyboard Tab traversal
- Buttons do not respond to Space/Enter automatically
- No pointer capture, drag, double-click, or context menu
- Anchors use explicit `{x, y}` positions; anchoring to an element/ref is not implemented
- The `anchor` named-point property is accepted but not yet used
- No click-outside overlay event
- No modal focus trapping

### Layout

This is a practical flex subset, not Yoga or full CSS:

- No wrapping
- No baseline alignment
- No `flexBasis`
- No per-item `alignSelf`
- Simplified min-content sizing
- No grid
- No transforms
- No `zIndex`; stacking follows retained child order
- No percentage positioning offsets

The implemented chat is therefore an adapted GPUix-style workload, not a
source-compatible execution of the original GPUix chat. The highest-value next
steps for closer parity are variable-height virtualization, wrapped/rich text,
element-ref anchoring, hover/active states, and complete editor input behavior.
