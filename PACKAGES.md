# Packages and native dependencies

BlendX uses a small JavaScript dependency set. Most rendering behavior is
implemented directly in `native/addon.cpp` and the sibling Blend2D checkout.

## Application runtime

| Package | Current resolved version | Purpose |
|---|---:|---|
| Hermes | pinned at `e9edc8b52832df9be82163fc17d2459aa456b0e2` | Lean embedded JavaScript VM and optimized bytecode runtime |
| `react` | 19.2.8 | Components, hooks, state, and JSX runtime |
| `react-reconciler` | 0.33.0 | Converts React commits into BlendX retained-tree mutations |

React, the reconciler, and application modules are bundled and compiled to
Hermes bytecode. The packaging step appends that bytecode to a native host
containing Hermes, the N-API renderer, and static Blend2D.

## Development packages

| Package | Current resolved version | Purpose |
|---|---:|---|
| `typescript` | 5.9.3 | Strict type checking and declaration/JavaScript output |
| `esbuild` | 0.28.2 | Bundles React and TS/TSX entries before Hermes compilation |
| `@types/node` | 26.2.0 | Types for build scripts and example host globals |
| `@types/react` | 19.2.18 | React and JSX types |
| `@types/react-reconciler` | 0.28.9 | Reconciler API types; its published host-config surface currently lags React 19 |

Node.js and npm are build-time tools only. The shipped application does not
contain Node.js or Bun and does not need `node_modules` at runtime.

## Native libraries

| Library | Source/version | Linkage | Purpose |
|---|---|---|---|
| Blend2D | sibling `../blend2d` checkout | Static, compiled into the Hermes host and test addon | SIMD/JIT rasterization, text shaping, paths, images, and the PRGB32 framebuffer |
| AsmJit | Blend2D's vendored `3rdparty/asmjit` | Static through Blend2D | Runtime generation of architecture-specific Blend2D pipelines |
| SDL2 | system 2.32.4 development package | Dynamic | Window creation, events, keyboard/text input, and framebuffer presentation |
| N-API | Hermes implementation plus system `node_api.h` | Static runtime ABI; headers at build time | Stable C ABI between Hermes and the native renderer; also builds a Node-compatible test addon |

Blend2D is built in-tree as `libblend2d.a`, eliminating a separate Blend2D
runtime file and the development-machine RPATH.

## Native build tools

| Tool | Observed version | Purpose |
|---|---:|---|
| CMake | 3.31.6 | Configures BlendX, Hermes, static Blend2D, the host, and the test addon |
| GCC/G++ | 15.2 | Compiles the C++17 host, renderer, Hermes, and Blend2D |
| `pkg-config` | system package | Locates SDL2 and its compiler/linker flags |
| Node.js/npm | 18.20.8 / 10.8.2 observed | Installs packages and runs TypeScript/bundling/test tools |

## Operating-system dependencies

The packed Hermes executable is one file, but it still uses the target
operating system's dynamic runtime and SDL stack. On Linux this includes
glibc, libstdc++, SDL2, and the X11/Wayland/input/audio libraries selected by
the distribution's SDL build. This is comparable to a native application using
system GUI libraries; those libraries are not JavaScript package dependencies.

The default font currently comes from:

```text
/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf
```

The chat's demonstration image currently comes from:

```text
/usr/share/pixmaps/debian-logo.png
```

For a completely asset-independent executable, the next step is to accept
font/image bytes in the native protocol and feed embedded assets directly
to Blend2D.

## Deliberately not external packages

BlendX does not currently depend on third-party packages for Markdown, syntax
highlighting, diffs, canvas commands, layout, virtualization, input widgets, or
scroll physics. Those implementations live in the renderer and are documented,
including their current limitations, in `CAPABILITIES.md`.
