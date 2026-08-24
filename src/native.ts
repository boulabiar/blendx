import type { NativeRenderer } from "./types.js"

declare global {
  // The native host installs this before evaluating the application bytecode.
  // Keeping the bridge on globalThis makes the React bundle independent of a
  // Node/Bun module loader and works with Hermes's N-API implementation.
  var __blendxNative: NativeRenderer | undefined
}

export function loadNativeRenderer(): NativeRenderer {
  if (!globalThis.__blendxNative) {
    throw new Error("BlendX native renderer was not installed by the runtime host")
  }
  return globalThis.__blendxNative
}
