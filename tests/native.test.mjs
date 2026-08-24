import assert from "node:assert/strict"
import { createRequire } from "node:module"
import test from "node:test"

const require = createRequire(import.meta.url)
const native = require("../dist/native/blendx_native.node")

test("renders a retained tree into a headless Blend2D framebuffer", () => {
  native.init({ width: 320, height: 200, threads: 2, headless: true })
  native.createElement(1, "div")
  native.setStyle(1, {
    width: "100%",
    height: "100%",
    padding: 12,
    backgroundColor: "#102030",
  })
  native.createElement(2, "text")
  native.setStyle(2, { fontSize: 20, color: "#ffffff" })
  native.setText(2, "BlendX")
  native.appendChild(1, 2)
  native.setRoot(1)
  native.commitMutations()
  native.renderFrame()

  const stats = native.getStats()
  assert.equal(stats.width, 320)
  assert.equal(stats.height, 200)
  assert.equal(stats.nodeCount, 2)
  assert.equal(stats.frameCount, 1)
  assert.equal(stats.threads, 2)
  assert.ok(stats.renderTimeMs >= 0)
  native.shutdown()
})

test("destroys retained subtrees", () => {
  native.init({ width: 32, height: 32, headless: true })
  native.createElement(10, "div")
  native.createElement(11, "text")
  native.appendChild(10, 11)
  assert.deepEqual(native.destroyElement(10).sort(), [10, 11])
  assert.equal(native.getStats().nodeCount, 0)
  native.shutdown()
})

test("applies one batch and repaints only changed pixels", () => {
  native.init({ width: 320, height: 200, headless: true })
  const initial = [
    ["create", 1, "div"],
    ["style", 1, { width: "100%", height: "100%", padding: 12, backgroundColor: "#101820" }],
    ["create", 2, "text"],
    ["style", 2, { fontSize: 20, color: "#ffffff" }],
    ["text", 2, "damage"],
    ["append", 1, 2],
    ["root", 1],
  ]
  native.applyBatch(initial)
  native.commitMutations()
  native.renderFrame()
  assert.equal(native.getStats().mutationsLastCommit, initial.length)

  native.applyBatch([["style", 2, { fontSize: 20, color: "#ff8090", backgroundColor: "#202838" }]])
  native.commitMutations()
  native.renderFrame()
  const stats = native.getStats()
  assert.ok(stats.paintedPixels > 0)
  assert.ok(stats.paintedPixels < 320 * 200)
  assert.equal(stats.dirtyRectCount, 1)
  native.shutdown()
})

test("virtual-list lays out and paints only viewport rows", () => {
  native.init({ width: 320, height: 100, headless: true })
  const batch = [
    ["create", 1, "virtual-list"],
    ["style", 1, { width: "100%", height: "100%", overflow: "scroll" }],
    ["prop", 1, "itemHeight", 20],
  ]
  for (let id = 2; id < 102; id++) {
    batch.push(["create", id, "div"])
    batch.push(["style", id, { width: "100%", height: 20, backgroundColor: "#202838" }])
    batch.push(["append", 1, id])
  }
  batch.push(["root", 1])
  native.applyBatch(batch)
  native.commitMutations()
  native.renderFrame()
  const stats = native.getStats()
  assert.equal(stats.nodeCount, 101)
  assert.ok(stats.paintedNodes < 15, JSON.stringify(stats))
  native.shutdown()
})
