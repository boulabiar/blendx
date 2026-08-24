# Packages and native dependencies

BlendX uses a small JavaScript dependency set. Most rendering behavior is
implemented directly in `native/addon.cpp` and the sibling Blend2D checkout.

## Application runtime

| Package | Current resolved version | Purpose |
|---|---:|---|
| Bun | 1.4.0 used for migration verification | JavaScript/TypeScript runtime, package manager, test runner, bundler, and single-executable compiler |
| `react` | 19.2.8 | Components, hooks, state, and JSX runtime |
| `react-reconciler` | 0.33.0 | Converts React commits into BlendX retained-tree mutations |

Bun embeds its runtime, React, the reconciler, application modules, and the
BlendX N-API addon when `bun build --compile` creates an executable.

## Development packages

| Package | Current resolved version | Purpose |
|---|---:|---|
| `typescript` | 5.9.3 | Strict type checking and declaration/JavaScript output |
| `@types/bun` | 1.4.0 | Bun globals and runtime APIs |
| `@types/react` | 19.2.18 | React and JSX types |
| `@types/react-reconciler` | 0.28.9 | Reconciler API types; its published host-config surface currently lags React 19 |

`tsx` and the direct `@types/node` dependency were removed during the Bun
migration. Bun executes the `.ts`/`.tsx` examples directly.

## Native libraries

| Library | Source/version | Linkage | Purpose |
|---|---|---|---|
| Blend2D | sibling `../blend2d` checkout | Static, compiled into `blendx_native.node` | SIMD/JIT rasterization, text shaping, paths, images, and the PRGB32 framebuffer |
| AsmJit | Blend2D's vendored `3rdparty/asmjit` | Static through Blend2D | Runtime generation of architecture-specific Blend2D pipelines |
| SDL2 | system 2.32.4 development package | Dynamic | Window creation, events, keyboard/text input, and framebuffer presentation |
| Node-API headers | system `node_api.h` | Build-time ABI only | Stable C ABI used by Bun to call the native renderer addon |

Blend2D was previously loaded from the sibling checkout as `libblend2d.so`.
The Bun migration builds it in-tree as `libblend2d.a`, eliminating that runtime
file and the development-machine RPATH.

## Native build tools

| Tool | Observed version | Purpose |
|---|---:|---|
| CMake | 3.31.6 | Configures BlendX, static Blend2D, and the N-API addon |
| GCC/G++ | 15.2 | Compiles the C++17 renderer and Blend2D |
| `pkg-config` | system package | Locates SDL2 and its compiler/linker flags |

## Operating-system dependencies

The compiled Bun executable is one file, but its embedded addon still uses the
target operating system's dynamic runtime and SDL stack. On Linux this includes
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
font/image bytes in the native protocol and feed Bun-embedded assets directly
to Blend2D.

## Deliberately not external packages

BlendX does not currently depend on third-party packages for Markdown, syntax
highlighting, diffs, canvas commands, layout, virtualization, input widgets, or
scroll physics. Those implementations live in the renderer and are documented,
including their current limitations, in `CAPABILITIES.md`.
