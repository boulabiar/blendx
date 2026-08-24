import { createRequire } from "node:module"
import type { NativeRenderer } from "./types.js"

const require = createRequire(import.meta.url)

export function loadNativeRenderer(): NativeRenderer {
  return require("../native/blendx_native.node") as NativeRenderer
}
