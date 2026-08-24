import type { NativeRenderer } from "./types.js"

// Bun's executable compiler recognizes direct N-API requires and embeds the
// addon in the generated binary. Bun also requires require()/process.dlopen()
// rather than ESM import for Node-API modules.
const nativeRenderer = require("../native/blendx_native.node") as NativeRenderer

export function loadNativeRenderer(): NativeRenderer {
  return nativeRenderer
}
