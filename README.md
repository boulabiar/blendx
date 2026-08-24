# BlendX

BlendX is a proof-of-concept native React renderer that draws its entire UI on
the CPU with [Blend2D](../blend2d). It uses Blend2D's runtime-selected SIMD/JIT
pipelines and optional worker threads; SDL2 only creates the native window,
delivers input, and presents the CPU framebuffer.

```text
React -> react-reconciler -> Node-API mutations -> retained C++ tree
      -> flex layout -> Blend2D BLImage -> SDL window surface
```

## Run the example

Prerequisites are Bun 1.3+, CMake, a C++17 compiler, SDL2 development files,
Node-API headers, and the sibling `../blend2d` source checkout. Blend2D is built
statically into the native addon.

```bash
bun install
bun run example
```

Click **Increment** to exercise the full native event-to-React-state-to-native
mutation round trip.

To run the large animated workload:

```bash
bun run stress
```

It retains 20,000 rows, paints only the visible rows, changes 320 small elements
at 60 Hz, and prints renderer statistics once per second. Scroll the list with
the mouse wheel. For a repeatable run without window-presentation overhead:

```bash
bun run benchmark
```

The GPUix-inspired chat workload retains 5,000 messages and exercises images,
SVG icons, a changing canvas, buttons, badges, progress, Markdown, code, diffs,
an editable composer, absolute positioning, and an anchored model picker:

```bash
bun run chat
bun run chat:benchmark
# choose a different transcript size
bun examples/chat.tsx --messages=20000
```

Click **Model** to show the anchored overlay, type in the composer, use
Ctrl+Enter to submit, and scroll the transcript with the mouse wheel.

To create a standalone executable containing Bun, React, the application, the
N-API addon, and static Blend2D:

```bash
bun run compile:chat
./build/blendx-chat
```

The executable still uses the target operating system's SDL2 and GUI libraries.
See [`PACKAGES.md`](PACKAGES.md) for the full dependency inventory.

## API

```tsx
import { render } from "blendx"

const app = render(
  <div style={{ width: "100%", height: "100%", padding: 24 }}>
    <text style={{ color: "#ffffff", fontSize: 24 }}>Hello from the CPU</text>
  </div>,
  { title: "My app", width: 800, height: 600, threads: 4 }
)

app.stop()
```

Setting `threads` to zero uses Blend2D's synchronous context. A positive value
uses an asynchronous context; values above one request additional workers from
Blend2D's shared thread pool.

## Current surface

- Host elements: `div`, `text`, `button`, `badge`, `separator`, `progress`,
  `img`, `svg`, `canvas`, `markdown`, `code`, `diff`, `input`, `textarea`,
  `anchored`, and `virtual-list`
- Layout: row/column flex subset, grow/shrink, alignment, justification, gap,
  per-edge padding/margins, min/max/fixed/percentage dimensions, scrolling,
  clipping, and absolute/fixed positioning
- Paint: colors, opacity, borders, rounded rectangles, raster image fit modes,
  a useful SVG path/shape subset, retained canvas commands, Markdown blocks,
  line-numbered code, colored diffs, badges, separators, and progress bars
- Input: mouse down/up/click, wheel scrolling, deepest-node hit testing,
  keyboard focus, UTF-8 text input, Backspace, Enter, change/submit/key events
- Overlays: out-of-flow anchored nodes with top/right/bottom/left placement,
  start/center/end alignment, gaps, and x/y offsets
- Window: resize and close handling through SDL2
- Renderer: PRGB32 Blend2D framebuffer, coalesced damage rectangles, partial
  framebuffer presentation, runtime-selected Blend2D SIMD/JIT pipelines
- Protocol: one Node-API mutation batch per React commit; unchanged style
  objects are filtered before entering the batch
- Virtualization: uniform-height `virtual-list` with configurable overdraw;
  offscreen retained rows are neither laid out nor painted
- Extensibility: native element registry maps host names to behavior handlers
- Tests: a headless renderer mode that still executes Blend2D

The style and canvas-command types are exported from `src/types.ts`. Unsupported properties are
rejected by TypeScript instead of being silently treated like browser CSS.

## Architecture notes

React queues mutations such as `createElement`, `appendChild`, `setStyle`, and
`setText`, then sends one array through Node-API per commit. C++ retains those
nodes, so unchanged UI does not cross Node-API again. A mutation records the old
damage, layout records the new damage, and overlapping rectangles are merged.
Painting traverses only nodes intersecting each damage rectangle. The paced JS
poll loop lets Node continue processing promises and timers while SDL events
are pumped.

`getStats()` separates `layoutTimeMs`, `paintTimeMs`, and `presentTimeMs`, and
also reports dirty rectangles, painted pixels/nodes, mutations, and rolling
p50/p95/maximum frame times.

Blend2D chooses its SIMD implementation internally. BlendX deliberately does
not contain handwritten intrinsics.

## Current stress results

On the development machine, the headless 1000×760 workload retained 40,325
native nodes and produced the following representative result:

| Measurement | Result |
|---|---:|
| Initial mount | 231 ms |
| Steady frame p50 | 0.34 ms |
| Steady frame p95 | 0.79 ms |
| Changed mutations/frame | 38 |
| Painted pixels/frame | 7,260–7,854 of 760,000 |
| Painted nodes/frame | 66–70 of 40,325 |

These are renderer-development numbers, not a cross-machine guarantee.
Headless mode excludes SDL/window-server presentation. In a forwarded X11
session, presentation dominates and should be evaluated separately from CPU
layout and Blend2D paint time.

The 5,000-message chat benchmark retains 11,312 nodes. A representative
compiled-Bun headless run mounted in 88 ms and then updated its animated
canvas/progress at a 0.23 ms median renderer frame time while painting 50
intersecting nodes and 7,480 pixels per update.

## Adding another native element

Register its name and handler kind in `ElementRegistry`, add its custom props to
the React host types and `syncCustomProps()`, then implement its measurement,
paint and event behavior. The protocol transports numbers, booleans, strings,
points, and retained canvas command arrays. Images and SVG file contents are
cached on first use.

This is not yet a complete desktop toolkit. The next substantial pieces are a
full flexbox implementation, text shaping across fallback fonts and bidi,
selection and clipboard support, IME composition, accessible platform nodes,
and production-grade CommonMark/syntax highlighting. The current Markdown and
code painters intentionally cover the chat/demo subset rather than replacing a
complete parser.
