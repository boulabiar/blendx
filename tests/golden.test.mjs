import assert from "node:assert/strict"
import { mkdtemp, readFile, copyFile, mkdir } from "node:fs/promises"
import { tmpdir } from "node:os"
import { join, dirname } from "node:path"
import { fileURLToPath } from "node:url"
import { createRequire } from "node:module"
import test from "node:test"

const require = createRequire(import.meta.url)
const native = require("../dist/native/blendx_native.node")
const reference = fileURLToPath(new URL("./golden/basic-components.png", import.meta.url))

test("basic components match the checked-in golden image", async () => {
  const outputDirectory = await mkdtemp(join(tmpdir(), "blendx-golden-"))
  const actual = join(outputDirectory, "basic-components.png")

  native.init({ width: 320, height: 180, threads: 1, headless: true })
  try {
    native.applyBatch([
      ["create", 1, "div"],
      ["style", 1, { width: "100%", height: "100%", padding: 16, gap: 12, backgroundColor: "#0b1018" }],
      ["create", 2, "text"],
      ["style", 2, { width: "100%", height: 28, fontSize: 22, color: "#f1f5f9" }],
      ["text", 2, "BlendX golden scene"],
      ["append", 1, 2],
      ["create", 3, "button"],
      ["style", 3, { width: 132, height: 38, padding: 9, borderRadius: 8, backgroundColor: "#6655ee", color: "#ffffff" }],
      ["create", 7, "text"],
      ["style", 7, { width: "100%", height: 20, fontSize: 14, color: "#ffffff" }],
      ["text", 7, "CPU rendered"],
      ["append", 3, 7],
      ["append", 1, 3],
      ["create", 4, "progress"],
      ["style", 4, { width: 240, height: 10, borderRadius: 5, backgroundColor: "#253044", color: "#8b7cff" }],
      ["prop", 4, "value", 68],
      ["prop", 4, "max", 100],
      ["append", 1, 4],
      ["create", 5, "separator"],
      ["style", 5, { width: "100%", height: 1, backgroundColor: "#334155" }],
      ["append", 1, 5],
      ["create", 6, "text"],
      ["style", 6, { width: "100%", height: 22, fontSize: 14, color: "#94a3b8" }],
      ["text", 6, "Deterministic Blend2D pixels"],
      ["append", 1, 6],
      ["root", 1],
    ])
    native.commitMutations()
    native.renderFrame()
    native.captureScreenshot(actual)
  } finally {
    native.shutdown()
  }

  if (process.env.UPDATE_GOLDENS === "1") {
    await mkdir(dirname(reference), { recursive: true })
    await copyFile(actual, reference)
  }
  const [expectedBytes, actualBytes] = await Promise.all([readFile(reference), readFile(actual)])
  assert.deepEqual(actualBytes, expectedBytes, `render changed; inspect ${actual}`)
})
