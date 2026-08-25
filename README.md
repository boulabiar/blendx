# BlendX

BlendX is a proof-of-concept native React renderer that draws its entire UI on
the CPU with [Blend2D](https://blend2d.com/). It uses Blend2D's runtime-selected
SIMD/JIT pipelines and optional worker threads; SDL2 only creates the native
window, delivers input, and presents the CPU framebuffer.

```text
React/Hermes bytecode -> react-reconciler -> N-API mutations -> retained C++ tree
      -> flex layout -> Blend2D BLImage -> SDL window surface
```

## Run the example

Prerequisites are Node.js 18+ and npm for the build tools, CMake 3.20+, a C++17
compiler, and SDL2 development files. CMake downloads checksummed, commit-pinned
Hermes, Blend2D, and AsmJit source archives on the first build. No sibling
checkout or `/tmp` dependency is required. This initial native build is
substantially longer than subsequent incremental builds. Hermes, Blend2D, and
AsmJit are statically linked into the application host.

```bash
npm install
npm run example
```

On Debian/Ubuntu, the native prerequisites can be installed with:

```bash
sudo apt-get install build-essential cmake libsdl2-dev ninja-build
```

Developers who already have dependency checkouts can bypass downloads without
changing the reproducible defaults:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DHERMES_ROOT=/path/to/hermes \
  -DBLEND2D_ROOT=/path/to/blend2d
cmake --build build --target blendx_hermes hermesc -j
```

`npm test` additionally enables and builds the Node-compatible addon used by
the native protocol tests. It is not part of the shipped Hermes application.

Click **Increment** to exercise the full native event-to-React-state-to-native
mutation round trip.

To run the large animated workload:

```bash
npm run stress
```

It models 20,000 rows while mounting only the visible window, changes 320 small
elements at 60 Hz, and prints renderer statistics once per second. Scroll the
list with the mouse wheel or drag the visible scrollbar. For a repeatable run
without window-presentation overhead:

```bash
npm run benchmark
```

For a visual scaling benchmark, `visual-stress` retains 250 to 5,000 compound
metric widgets (roughly ten native nodes each) and can pressure sparse updates,
dense updates, layout, painting, mount/unmount churn, or scrolling independently:

```bash
npm run visual-stress
npm run visual-stress:benchmark
npm run visual-stress:matrix

# Override the deterministic workload:
npm run visual-stress -- --components=2500 --profile=dense --update=100 --fps=60
npm run visual-stress:matrix -- --components=1000 --duration=4000
```

Its telemetry reports actual application FPS alongside native layout, paint,
presentation, p50/p95/p99/maximum frame time, rolling 60 Hz budget misses,
mutations, dirty rectangles, and painted node visits. Headless native frame
times intentionally exclude desktop-compositor cost and React/Hermes work
outside the native frame; actual FPS makes that distinction visible.

The GPUix-inspired chat workload retains 5,000 messages and exercises images,
SVG icons, a changing canvas, buttons, badges, progress, Markdown, code, diffs,
an editable composer, absolute positioning, and an anchored model picker:

```bash
npm run chat
npm run chat:benchmark
# choose a different transcript size
npm run chat -- --messages=20000
```

Click **Model** to show the anchored overlay, type in the composer, use
Ctrl+Enter to submit, and scroll the transcript with the mouse wheel.

The native-text showcase ports GPUix's Markdown, highlighted-code, and diff
composition into a polished BlendX release-review screen:

```bash
npm run showcase
# repeatable headless smoke/performance run
npm run showcase:benchmark
```

Use **Change sample** to update the code and diff in one React commit.

The GPUix-inspired pull-request review example expands the dedicated diff demo
with selectable files, large scrollable patches, review progress, status
actions, and an anchored quick-file picker:

```bash
npm run diff
npm run diff:benchmark
```

Use the changed-file sidebar or **Jump to file**, then approve the review or
request changes from the header.

The real-time operations dashboard combines animated canvas charts, changing
metrics, service-health progress, and a 160-row virtualized process table:

```bash
npm run dashboard
npm run dashboard:benchmark
```

Use **Pause updates** to freeze or resume the live telemetry.

The live log and trace explorer retains 2,500 events, inserts new entries at
the top, virtualizes visible rows, and provides severity/service filters,
search, pause/resume, row selection, and trace details:

```bash
npm run logs
npm run logs:benchmark
```

The floating-controls gallery demonstrates the reusable `Tooltip`, `Select`,
and `Combobox` components together with hover/active styles, element-relative
anchoring, outside-click dismissal, and keyboard focus:

```bash
npm run components
npm run components:benchmark
```

See [`COMPONENTS.md`](COMPONENTS.md) for the compound APIs and interaction
foundation.

The application-foundation gallery brings the newer primitives together in one
interactive screen: dialogs, toast notifications, declarative motion,
multiline/password editing, wrapped selectable text, accessibility inspection,
and a variable-height memory-windowed activity feed:

```bash
npm run foundation
npm run foundation:benchmark
```

Try the modal's Tab/Escape behavior, generate notifications, toggle the motion
target, edit/select text, inspect semantic nodes, jump to row 300, and append a
follow-tail activity record.

To create a standalone executable containing the lean Hermes VM, React,
react-reconciler, the application bytecode, the native renderer, and static
Blend2D:

```bash
npm run compile:chat
./build/blendx-chat

# The showcase can be packed in the same way:
npm run compile:showcase
./build/blendx-showcase

# And the pull-request review app:
npm run compile:diff
./build/blendx-diff

# And the operations dashboard:
npm run compile:dashboard
./build/blendx-dashboard

# And the live log explorer:
npm run compile:logs
./build/blendx-logs

# And the floating-controls gallery:
npm run compile:components
./build/blendx-components

# And the application-foundation gallery:
npm run compile:foundation
./build/blendx-foundation

# And the visual component benchmark:
npm run compile:visual-stress
./build/blendx-visual-stress --components=2500 --profile=paint
```

The executable still uses the target operating system's SDL2 and GUI libraries.
On the development machine the packed chat is about 6.8 MB (6.5 MiB), including
optimized Hermes bytecode and the embedded Roboto font. `tools/hermes-app.mjs`
accepts any TS/TSX entry, so additional examples do not require a new native
host or external application assets.
See [`PACKAGES.md`](PACKAGES.md) for the full dependency inventory.

## Reproducibility and CI

Native dependencies are pinned by commit and SHA-256 in `CMakeLists.txt`.
GitHub Actions starts from a clean checkout, builds the runtime and test addon,
runs the native/React tests, executes the 5,000-message headless chat under
Hermes, and verifies the packed executable. Local source overrides are an
optimization only and are never used by CI.

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
- Input: mouse down/up/click, wheel and draggable-scrollbar scrolling,
  deepest-node hit testing, hover/active states, outside presses, modal Tab
  focus trapping, UTF-8 selection/editing, clipboard, undo/redo, password
  masking, IME pre-edit display, and keyboard activation
- Overlays: out-of-flow anchored nodes with top/right/bottom/left placement,
  start/center/end alignment, element-ref or point anchors, viewport clamping,
  gaps, and x/y offsets
- Components: `Tooltip`, `Select`, `Combobox`, checkbox/radio/switch/slider,
  tabs, accordion/collapsible, dropdown/context/submenus, `Dialog`, toasts,
  memory-windowed `VirtualList`, and declarative `motion`
- Window: resize and close handling through SDL2
- Renderer: PRGB32 Blend2D framebuffer, coalesced damage rectangles, partial
  framebuffer presentation, runtime-selected Blend2D SIMD/JIT pipelines
- Protocol: one Node-API mutation batch per React commit; unchanged style
  objects are filtered before entering the batch
- Virtualization: the generic `VirtualList` mounts only the visible React/native
  rows, supports known variable heights, overdraw, follow-tail, range reporting,
  scroll anchoring, and imperative index/offset scrolling
- Extensibility: native element registry maps host names to behavior handlers
- Tests: a headless renderer mode that still executes Blend2D

The style and canvas-command types are exported from `src/types.ts`. Unsupported properties are
rejected by TypeScript instead of being silently treated like browser CSS.

## Architecture notes

React queues mutations such as `createElement`, `appendChild`, `setStyle`, and
`setText`, then sends one array through N-API per commit. C++ retains those
nodes, so unchanged UI does not cross N-API again. A mutation records the old
damage, layout records the new damage, and overlapping rectangles are merged.
Painting traverses only nodes intersecting each damage rectangle. The Hermes
host supplies timers, microtask draining, console, `performance.now`, arguments,
and SIGINT handling. Its event loop blocks in `SDL_WaitEventTimeout` while idle,
wakes immediately for native input, and uses the next JavaScript timer as its
deadline rather than polling every 8 ms.

The native renderer is organized into `renderer_model.h`,
`renderer_layout.inc`, `renderer_paint.inc`, the input/event core in
`addon.cpp`, and `napi_protocol.inc`. They remain one optimized translation
unit while keeping feature ownership and review boundaries explicit.

`getStats()` separates `layoutTimeMs`, `paintTimeMs`, and `presentTimeMs`, and
also reports dirty rectangles, painted pixels/nodes, mutations, rolling
p50/p95/p99/maximum frame times, and the rolling count above the 16.67 ms
60 Hz frame budget.

Blend2D chooses its SIMD implementation internally. BlendX deliberately does
not contain handwritten intrinsics.

## Current stress results

Before memory-windowing, the headless 1000×760 stress workload retained 40,325
native nodes and produced the following historical representative result:

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

The current 5,000-message chat benchmark retains 97 native nodes. A
representative packed Hermes headless run mounted in about 3.9 ms and updated its
animated canvas/progress at about a 0.44 ms median and 0.73 ms p95 renderer
frame time while painting 50 intersecting nodes and 7,480 pixels per update.
See [`CAPABILITIES.md`](CAPABILITIES.md) for the measurement scope and memory
comparison.

## Adding another native element

Register its name and handler kind in `ElementRegistry`, add its custom props to
the React host types and `syncCustomProps()`, then implement its measurement,
paint and event behavior. The protocol transports numbers, booleans, strings,
points, and retained canvas command arrays. Images and SVG file contents are
cached on first use.

This is not yet a complete desktop toolkit. The next substantial pieces are a
full flexbox implementation, automatic variable-row measurement, text shaping
across fallback fonts and bidi, OS accessibility adapters, candidate-window
IME integration, native animation timelines, and production-grade
CommonMark/syntax highlighting. The current Markdown and code painters
intentionally cover the demo subset rather than replacing complete parsers.
