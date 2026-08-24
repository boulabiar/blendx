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

test("flex growth overrides a zero-width basis with equal computed widths", () => {
  native.init({ width: 600, height: 100, headless: true })
  const batch = [
    ["create", 1, "div"],
    ["style", 1, { width: "100%", height: "100%", flexDirection: "row", gap: 12 }],
  ]
  for (let id = 2; id <= 4; id++) {
    batch.push(["create", id, "div"])
    batch.push(["style", id, { width: 0, height: 80, flexGrow: 1 }])
    batch.push(["append", 1, id])
  }
  batch.push(["root", 1])
  native.applyBatch(batch)
  native.commitMutations()
  native.renderFrame()

  const boxes = [2, 3, 4].map((id) => native.getElementBox(id))
  assert.deepEqual(boxes.map((box) => box.width), [192, 192, 192])
  assert.deepEqual(boxes.map((box) => box.x), [0, 204, 408])
  native.shutdown()
})

test("renders rich elements, structured props, and absolute overlays", () => {
  native.init({ width: 640, height: 480, headless: true })
  const elements = ["img", "svg", "canvas", "button", "separator", "badge", "progress", "markdown", "code", "diff", "textarea", "anchored"]
  const batch = [
    ["create", 1, "div"],
    ["style", 1, { width: "100%", height: "100%", padding: 8, gap: 4, position: "relative" }],
  ]
  elements.forEach((type, index) => {
    const id = index + 2
    batch.push(["create", id, type])
    batch.push(["style", id, { width: 180, height: type === "separator" ? 1 : 28, color: "#ffffff", backgroundColor: "#202838" }])
    batch.push(["append", 1, id])
  })
  batch.push(["prop", 3, "src", '<svg viewBox="0 0 24 24" fill="none" stroke="#000"><path d="M2 12h20"/></svg>'])
  batch.push(["prop", 4, "commands", [{ kind: "fillRect", x: 2, y: 2, width: 20, height: 10, color: "#ff0000", radius: 2 }]])
  batch.push(["prop", 8, "value", 42], ["prop", 8, "max", 100])
  batch.push(["prop", 9, "source", "# Markdown\n- retained"])
  batch.push(["prop", 10, "code", "const x = 1"], ["prop", 10, "showLineNumbers", true])
  batch.push(["prop", 11, "patch", "-old\n+new"])
  batch.push(["prop", 12, "value", "editable"], ["prop", 12, "placeholder", "type"])
  batch.push(["style", 13, { width: 120, height: 40, position: "absolute" }])
  batch.push(["prop", 13, "position", { x: 300, y: 40 }], ["prop", 13, "side", "bottom"])
  batch.push(["root", 1])
  native.applyBatch(batch)
  native.commitMutations()
  native.renderFrame()
  const stats = native.getStats()
  assert.equal(stats.nodeCount, 13)
  assert.ok(stats.paintedNodes >= 10)
  assert.ok(stats.renderTimeMs >= 0)
  native.shutdown()
})
