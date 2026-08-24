import assert from "node:assert/strict"
import { createRequire } from "node:module"
import test from "node:test"
import React from "react"

const require = createRequire(import.meta.url)
globalThis.__blendxNative = require("../dist/native/blendx_native.node")
const {
  render,
  Tooltip,
  TooltipContent,
  TooltipTrigger,
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
  Combobox,
  ComboboxContent,
  ComboboxInput,
  ComboboxItem,
  ComboboxList,
  Checkbox,
  RadioGroup,
  RadioGroupItem,
  Switch,
  Slider,
} = await import("../dist/src/index.js")

function pointer(native, kind, x, y) {
  native.dispatchPointer(kind, x, y, 1)
}

function click(native, x, y) {
  pointer(native, "mouseDown", x, y)
  pointer(native, "mouseUp", x, y)
}

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

test("Tooltip opens on native hover and closes after leaving", async () => {
  let open = false
  const app = render(
    React.createElement("div", { style: { width: "100%", height: "100%", padding: 12 } },
      React.createElement(Tooltip, { delayDuration: 0, onOpenChange: (value) => { open = value } },
        React.createElement(TooltipTrigger, { style: { width: 120, height: 36, backgroundColor: "#223344" } },
          React.createElement("text", null, "Hover")),
        React.createElement(TooltipContent, { side: "bottom", style: { width: 120, height: 30, backgroundColor: "#111827" } },
          React.createElement("text", null, "Tooltip")))),
    { width: 360, height: 220, headless: true },
  )
  await new Promise((resolve) => setTimeout(resolve, 20))
  pointer(globalThis.__blendxNative, "mouseMove", 20, 20)
  await new Promise((resolve) => setTimeout(resolve, 20))
  assert.equal(open, true)
  pointer(globalThis.__blendxNative, "mouseMove", 320, 200)
  await new Promise((resolve) => setTimeout(resolve, 120))
  assert.equal(open, false)
  app.stop()
})

test("Select opens, anchors content, and chooses an item", async () => {
  let selected = "alpha"
  const app = render(
    React.createElement("div", { style: { width: "100%", height: "100%", padding: 12 } },
      React.createElement(Select, { defaultValue: "alpha", onValueChange: (value) => { selected = value } },
        React.createElement(SelectTrigger, { style: { width: 180, height: 36, backgroundColor: "#27324a" } },
          React.createElement(SelectValue, null)),
        React.createElement(SelectContent, { side: "bottom", style: { width: 180, padding: 4, backgroundColor: "#111827" } },
          React.createElement(SelectItem, { value: "alpha", style: { width: "100%", height: 32 } }, "Alpha"),
          React.createElement(SelectItem, { value: "beta", style: { width: "100%", height: 32 } }, "Beta")))),
    { width: 400, height: 260, headless: true },
  )
  await new Promise((resolve) => setTimeout(resolve, 20))
  click(globalThis.__blendxNative, 30, 25)
  await new Promise((resolve) => setTimeout(resolve, 20))
  assert.ok(app.renderer.getStats().nodeCount >= 8)
  globalThis.__blendxNative.dispatchKey("ArrowDown")
  globalThis.__blendxNative.dispatchKey("Enter")
  await new Promise((resolve) => setTimeout(resolve, 20))
  assert.equal(selected, "beta")
  app.stop()
})

test("Combobox opens from its native input and selects a filtered-list item", async () => {
  let selected = null
  const frameworks = ["Astro", "Next.js", "SvelteKit"]
  const app = render(
    React.createElement("div", { style: { width: "100%", height: "100%", padding: 12 } },
      React.createElement(Combobox, { items: frameworks, onValueChange: (value) => { selected = value } },
        React.createElement(ComboboxInput, { placeholder: "Framework", style: { width: 180, height: 36, backgroundColor: "#27324a" } }),
        React.createElement(ComboboxContent, { side: "bottom", style: { width: 180, padding: 4, backgroundColor: "#111827" } },
          React.createElement(ComboboxList, null, (item) =>
            React.createElement(ComboboxItem, { key: item, value: item, style: { width: "100%", height: 32 } }, item))))),
    { width: 400, height: 280, headless: true },
  )
  await new Promise((resolve) => setTimeout(resolve, 20))
  click(globalThis.__blendxNative, 30, 25)
  await new Promise((resolve) => setTimeout(resolve, 20))
  click(globalThis.__blendxNative, 30, 105)
  await new Promise((resolve) => setTimeout(resolve, 20))
  assert.equal(selected, "Next.js")
  app.stop()
})

test("selection controls respond to pointer and keyboard interaction", async () => {
  let checked = false
  let switched = false
  let radio = "alpha"
  let slider = 0
  const h = React.createElement
  const app = render(
    h("div", { style: { width: "100%", height: "100%", padding: 10, gap: 10 } },
      h(Checkbox, { onCheckedChange: (value) => { checked = value }, style: { width: 40, height: 30 } }),
      h(Switch, { onCheckedChange: (value) => { switched = value }, style: { width: 50, height: 30 } }),
      h(RadioGroup, { defaultValue: "alpha", onValueChange: (value) => { radio = value }, style: { width: 100, height: 60, gap: 4 } },
        h(RadioGroupItem, { value: "alpha", style: { width: 100, height: 28 } }),
        h(RadioGroupItem, { value: "beta", style: { width: 100, height: 28 } })),
      h(Slider, { onValueChange: (value) => { slider = value }, style: { width: 200, height: 24 } })),
    { width: 320, height: 240, headless: true },
  )
  await new Promise((resolve) => setTimeout(resolve, 25))
  try {
    click(globalThis.__blendxNative, 20, 20)
    await new Promise((resolve) => setTimeout(resolve, 20))
    assert.equal(checked, true)
    globalThis.__blendxNative.dispatchKey("Space")
    await new Promise((resolve) => setTimeout(resolve, 20))
    assert.equal(checked, false)

    click(globalThis.__blendxNative, 20, 60)
    await new Promise((resolve) => setTimeout(resolve, 20))
    assert.equal(switched, true)

    click(globalThis.__blendxNative, 20, 132)
    await new Promise((resolve) => setTimeout(resolve, 20))
    assert.equal(radio, "beta")
    globalThis.__blendxNative.dispatchKey("ArrowLeft")
    await new Promise((resolve) => setTimeout(resolve, 20))
    assert.equal(radio, "alpha")

    pointer(globalThis.__blendxNative, "mouseDown", 110, 182)
    await new Promise((resolve) => setTimeout(resolve, 20))
    pointer(globalThis.__blendxNative, "mouseMove", 300, 182)
    pointer(globalThis.__blendxNative, "mouseUp", 300, 182)
    await new Promise((resolve) => setTimeout(resolve, 20))
    assert.equal(slider, 100)
    globalThis.__blendxNative.dispatchKey("ArrowLeft")
    await new Promise((resolve) => setTimeout(resolve, 20))
    assert.equal(slider, 99)
  } finally {
    app.stop()
  }
})
