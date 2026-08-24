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
