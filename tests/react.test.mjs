import assert from "node:assert/strict"
import test from "node:test"
import React from "react"
import { render } from "../dist/src/index.js"

test("React mounts through the reconciler and renders with Blend2D", async () => {
  const tree = () => React.createElement(
      "div",
      { style: { width: "100%", height: "100%", backgroundColor: "#123456" } },
      React.createElement("text", { style: { fontSize: 20, color: "#ffffff" } }, "React works")
    )
  const app = render(
    tree(),
    { width: 320, height: 200, threads: 2, headless: true }
  )

  await new Promise((resolve) => setTimeout(resolve, 25))
  const stats = app.renderer.getStats()
  assert.equal(stats.nodeCount, 2)
  assert.ok(stats.frameCount >= 1)
  assert.ok(stats.mutationsLastCommit > 0)

  const frameCount = stats.frameCount
  app.render(tree())
  await new Promise((resolve) => setTimeout(resolve, 25))
  assert.equal(app.renderer.getStats().frameCount, frameCount)
  app.stop()
})

test("React forwards rich element properties in one native batch", async () => {
  const h = React.createElement
  const app = render(
    h("div", { style: { width: "100%", height: "100%", position: "relative" } },
      h("canvas", { commands: [{ kind: "circle", x: 20, y: 20, radius: 10, color: "#38bdf8" }], style: { width: 80, height: 50 } }),
      h("progress", { value: 65, max: 100, style: { width: 100, height: 8 } }),
      h("markdown", { source: "# Hello\nBlendX", style: { width: 200, height: 50 } }),
      h("textarea", { value: "compose", placeholder: "message", style: { width: 200, height: 40 } }),
      h("anchored", { position: { x: 200, y: 20 }, side: "bottom", style: { width: 100, height: 30, position: "absolute" } },
        h("text", null, "overlay")))
    , { width: 400, height: 240, headless: true })
  await new Promise((resolve) => setTimeout(resolve, 25))
  const stats = app.renderer.getStats()
  assert.equal(stats.nodeCount, 7)
  assert.ok(stats.frameCount >= 1)
  app.stop()
})
