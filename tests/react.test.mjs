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
  Tabs,
  TabsList,
  TabsTrigger,
  TabsContent,
  Accordion,
  AccordionItem,
  AccordionTrigger,
  AccordionContent,
  Collapsible,
  CollapsibleTrigger,
  CollapsibleContent,
  DropdownMenu,
  DropdownMenuTrigger,
  DropdownMenuContent,
  DropdownMenuItem,
  DropdownMenuCheckboxItem,
  DropdownMenuSub,
  DropdownMenuSubTrigger,
  DropdownMenuSubContent,
  ContextMenu,
  ContextMenuTrigger,
  ContextMenuContent,
  ContextMenuItem,
  VirtualList,
  Dialog,
  DialogTrigger,
  DialogContent,
  DialogClose,
  ToastProvider,
  ToastViewport,
  useToast,
  motion,
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

  app.flush()
  const stats = app.renderer.getStats()
  assert.equal(stats.nodeCount, 2)
  assert.ok(stats.frameCount >= 1)
  assert.ok(stats.mutationsLastCommit > 0)

  const frameCount = stats.frameCount
  app.render(tree())
  app.flush()
  assert.equal(app.renderer.getStats().frameCount, frameCount)
  app.stop()
})

test("rejects a second live root without disturbing the first", async () => {
  const h = React.createElement
  const app = render(h("div", { style: { width: "100%", height: "100%" } }), {
    width: 80,
    height: 60,
    headless: true,
  })
  try {
    assert.throws(
      () => render(h("div", null), { width: 20, height: 20, headless: true }),
      /one live root per process/,
    )
    assert.equal(app.renderer.getStats().width, 80)
  } finally {
    app.stop()
  }
})

test("VirtualList memory-windows variable-height rows and scrolls by index", async () => {
  const h = React.createElement
  const items = Array.from({ length: 10_000 }, (_, index) => ({ id: index, height: index % 2 ? 30 : 18 }))
  const list = React.createRef()
  let visible = [0, 0]
  const app = render(
    h(VirtualList, {
      ref: list,
      items,
      estimatedItemHeight: 24,
      getItemHeight: (item) => item.height,
      getItemKey: (item) => item.id,
      overdraw: 2,
      style: { width: 240, height: 120 },
      onVisibleRangeChange: (start, end) => { visible = [start, end] },
      renderItem: (item) => h("text", { style: { width: "100%", height: item.height } }, `Row ${item.id}`),
    }),
    { width: 240, height: 120, headless: true },
  )
  app.flush()
  try {
    assert.ok(app.renderer.getStats().nodeCount < 30, JSON.stringify(app.renderer.getStats()))
    assert.ok(visible[1] < 12, JSON.stringify(visible))
    list.current.scrollToIndex(9_000, "start")
    app.flush()
    assert.ok(visible[0] <= 9_000 && visible[1] > 9_000, JSON.stringify(visible))
    assert.ok(app.renderer.getStats().nodeCount < 30, JSON.stringify(app.renderer.getStats()))
  } finally {
    app.stop()
  }
})

test("Dialog traps focus, dismisses outside, and restores its trigger", async () => {
  const h = React.createElement
  let open = false
  let outsideFocused = false
  let triggerFocused = 0
  const app = render(
    h("div", { style: { width: "100%", height: "100%", position: "relative" } },
      h(Dialog, { onOpenChange: (value) => { open = value } },
        h(DialogTrigger, { onFocus: () => { triggerFocused += 1 }, style: { width: 80, height: 30 } }, "Open"),
        h(DialogContent, { style: { width: 160, height: 80, padding: 8, backgroundColor: "#202838" } },
          h(DialogClose, { style: { width: 60, height: 24 } }, "Close"))),
      h("button", { onFocus: () => { outsideFocused = true }, style: { width: 80, height: 30, position: "absolute", top: 160 } }, "Outside")),
    { width: 300, height: 200, headless: true },
  )
  app.flush()
  try {
    click(globalThis.__blendxNative, 20, 15)
    app.flush()
    assert.equal(open, true)
    const modal = app.renderer.getAccessibilityTree().find((node) => node.role === "dialog")
    assert.deepEqual(
      modal && { x: modal.x, y: modal.y, width: modal.width, height: modal.height },
      { x: 0, y: 0, width: 300, height: 200 },
    )
    globalThis.__blendxNative.dispatchKey("Tab")
    app.flush()
    assert.equal(outsideFocused, false)
    click(globalThis.__blendxNative, 10, 100)
    app.flush()
    assert.equal(open, false)
    assert.ok(triggerFocused >= 1)
  } finally {
    app.stop()
  }
})

test("Toast queues notifications and supports dismissal", async () => {
  const h = React.createElement
  function ToastDemo() {
    const { toast } = useToast()
    return h("div", { style: { width: "100%", height: "100%", position: "relative" } },
      h("button", { onClick: () => toast({ title: "Saved", duration: 0 }), style: { width: 80, height: 30 } }, "Show"),
      h(ToastViewport, null))
  }
  const app = render(h(ToastProvider, null, h(ToastDemo)), { width: 400, height: 240, headless: true })
  app.flush()
  try {
    const before = app.renderer.getStats().nodeCount
    click(globalThis.__blendxNative, 20, 15)
    app.flush()
    assert.ok(app.renderer.getStats().nodeCount > before)
    click(globalThis.__blendxNative, 240, 35)
    app.flush()
    assert.equal(app.renderer.getStats().nodeCount, before)
  } finally {
    app.stop()
  }
})

test("motion interpolates native styles and accessibility metadata is inspectable", async () => {
  const h = React.createElement
  const animated = React.createRef()
  let completed = false
  const app = render(
    h("div", { style: { width: "100%", height: "100%" } },
      h(motion.div, {
        ref: animated,
        initial: { width: 10, opacity: 0 },
        animate: { width: 80, opacity: 1 },
        transition: { duration: 30, easing: "linear" },
        onAnimationComplete: () => { completed = true },
        style: { height: 20, backgroundColor: "#ffffff" },
      }),
      h("button", { accessibilityLabel: "Save document", style: { width: 100, height: 30 } }, "Save")),
    { width: 240, height: 120, headless: true },
  )
  await new Promise((resolve) => setTimeout(resolve, 90))
  try {
    assert.equal(completed, true)
    assert.ok(app.renderer.getElementBox(animated.current.id).width >= 79)
    const tree = app.renderer.getAccessibilityTree()
    assert.ok(tree.some((node) => node.role === "button" && node.label === "Save document"), JSON.stringify(tree))
  } finally {
    app.stop()
  }
})

test("semantic controls expose accessibility state", async () => {
  const h = React.createElement
  const app = render(
    h("div", null,
      h(Checkbox, { checked: true, accessibilityLabel: "Enable sync" }, "Sync"),
      h(Slider, { value: 42, accessibilityLabel: "Volume" }),
      h(Tabs, { value: "one" },
        h(TabsList, null, h(TabsTrigger, { value: "one" }, "One")))),
    { width: 320, height: 180, headless: true },
  )
  app.flush()
  try {
    const tree = app.renderer.getAccessibilityTree()
    assert.equal(tree.find((node) => node.label === "Enable sync")?.checked, "true")
    assert.equal(tree.find((node) => node.label === "Volume")?.value, "42")
    assert.equal(tree.find((node) => node.role === "tab")?.selected, true)
  } finally {
    app.stop()
  }
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
  app.flush()
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
  app.flush()
  pointer(globalThis.__blendxNative, "mouseMove", 20, 20)
  app.flush()
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
  app.flush()
  click(globalThis.__blendxNative, 30, 25)
  app.flush()
  assert.ok(app.renderer.getStats().nodeCount >= 8)
  globalThis.__blendxNative.dispatchKey("ArrowDown")
  globalThis.__blendxNative.dispatchKey("Enter")
  app.flush()
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
  app.flush()
  click(globalThis.__blendxNative, 30, 25)
  app.flush()
  click(globalThis.__blendxNative, 30, 105)
  app.flush()
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
  app.flush()
  try {
    click(globalThis.__blendxNative, 20, 20)
    app.flush()
    assert.equal(checked, true)
    globalThis.__blendxNative.dispatchKey("Space")
    app.flush()
    assert.equal(checked, false)

    click(globalThis.__blendxNative, 20, 60)
    app.flush()
    assert.equal(switched, true)

    click(globalThis.__blendxNative, 20, 132)
    app.flush()
    assert.equal(radio, "beta")
    globalThis.__blendxNative.dispatchKey("ArrowLeft")
    app.flush()
    assert.equal(radio, "alpha")

    pointer(globalThis.__blendxNative, "mouseDown", 110, 182)
    app.flush()
    pointer(globalThis.__blendxNative, "mouseMove", 300, 182)
    pointer(globalThis.__blendxNative, "mouseUp", 300, 182)
    app.flush()
    assert.equal(slider, 100)
    globalThis.__blendxNative.dispatchKey("ArrowLeft")
    app.flush()
    assert.equal(slider, 99)
  } finally {
    app.stop()
  }
})

test("tabs, accordion, and collapsible implement pointer and keyboard state", async () => {
  let tab = "overview"
  let accordion = "first"
  let focusedAccordion = ""
  let collapsedOpen = false
  const h = React.createElement
  const app = render(
    h("div", { style: { width: "100%", height: "100%", position: "relative" } },
      h(Tabs, { defaultValue: "overview", onValueChange: (value) => { tab = value }, style: { width: 300, height: 82, position: "absolute", left: 10, top: 10, gap: 4 } },
        h(TabsList, { style: { width: 300, height: 32, flexDirection: "row" } },
          h(TabsTrigger, { value: "overview", style: { width: 100, height: 32 } }, "Overview"),
          h(TabsTrigger, { value: "activity", style: { width: 100, height: 32 } }, "Activity"),
          h(TabsTrigger, { value: "disabled", disabled: true, style: { width: 100, height: 32 } }, "Disabled")),
        h(TabsContent, { value: "overview", style: { width: 300, height: 40 } }, "Overview panel"),
        h(TabsContent, { value: "activity", style: { width: 300, height: 40 } }, "Activity panel")),
      h(Accordion, { defaultValue: "first", collapsible: true, onValueChange: (value) => { accordion = value }, style: { width: 300, position: "absolute", left: 10, top: 110, gap: 4 } },
        h(AccordionItem, { value: "first", style: { width: 300 } },
          h(AccordionTrigger, { onFocus: () => { focusedAccordion = "first" }, style: { width: 300, height: 30 } }, "First"),
          h(AccordionContent, { style: { width: 300, height: 24 } }, "First content")),
        h(AccordionItem, { value: "second", style: { width: 300 } },
          h(AccordionTrigger, { onFocus: () => { focusedAccordion = "second" }, style: { width: 300, height: 30 } }, "Second"),
          h(AccordionContent, { style: { width: 300, height: 24 } }, "Second content"))),
      h(Collapsible, { onOpenChange: (value) => { collapsedOpen = value }, style: { width: 300, position: "absolute", left: 10, top: 250 } },
        h(CollapsibleTrigger, { style: { width: 300, height: 32 } }, "Toggle"),
        h(CollapsibleContent, { style: { width: 300, height: 30 } }, "Details"))),
    { width: 360, height: 340, headless: true },
  )
  app.flush()
  try {
    click(globalThis.__blendxNative, 130, 25)
    app.flush()
    assert.equal(tab, "activity")
    globalThis.__blendxNative.dispatchKey("ArrowLeft")
    app.flush()
    assert.equal(tab, "overview")

    click(globalThis.__blendxNative, 20, 125)
    app.flush()
    assert.equal(accordion, "")
    globalThis.__blendxNative.dispatchKey("ArrowDown")
    app.flush()
    assert.equal(focusedAccordion, "second")

    click(globalThis.__blendxNative, 20, 265)
    app.flush()
    assert.equal(collapsedOpen, true)
    globalThis.__blendxNative.dispatchKey("Space")
    app.flush()
    assert.equal(collapsedOpen, false)
  } finally {
    app.stop()
  }
})

test("dropdown and context menus support keyboard, selection, and right click", async () => {
  let selected = ""
  let pinned = false
  let contextSelected = ""
  let contextOpen = false
  const h = React.createElement
  const itemStyle = { width: 180, height: 32 }
  const app = render(
    h("div", { style: { width: "100%", height: "100%", position: "relative" } },
      h("div", { style: { width: 160, height: 36, position: "absolute", left: 10, top: 10 } },
        h(DropdownMenu, null,
          h(DropdownMenuTrigger, { style: { width: 160, height: 36 } }, "Actions"),
          h(DropdownMenuContent, { style: { width: 188, padding: 4, gap: 2, backgroundColor: "#111827" } },
            h(DropdownMenuItem, { value: "alpha", onSelect: () => { selected = "alpha" }, style: itemStyle }, "Alpha"),
            h(DropdownMenuItem, { value: "beta", onSelect: () => { selected = "beta" }, style: itemStyle }, "Beta"),
            h(DropdownMenuCheckboxItem, { value: "pinned", checked: pinned, onCheckedChange: (value) => { pinned = value }, style: itemStyle }, "Pinned")))),
      h(ContextMenu, { onOpenChange: (value) => { contextOpen = value } },
        h(ContextMenuTrigger, { style: { width: 400, height: 160, position: "absolute", left: 10, top: 150, backgroundColor: "#202838" } }, "Right click"),
        h(ContextMenuContent, { style: { width: 168, padding: 4, backgroundColor: "#111827" } },
          h(ContextMenuItem, { value: "inspect", onSelect: () => { contextSelected = "inspect" }, style: { width: 160, height: 32 } }, "Inspect")))),
    { width: 420, height: 320, headless: true },
  )
  app.flush()
  try {
    click(globalThis.__blendxNative, 30, 25)
    globalThis.__blendxNative.dispatchKey("ArrowDown")
    globalThis.__blendxNative.dispatchKey("Enter")
    app.flush()
    assert.equal(selected, "beta")

    click(globalThis.__blendxNative, 30, 25)
    app.flush()
    click(globalThis.__blendxNative, 30, 126)
    app.flush()
    assert.equal(pinned, true)

    globalThis.__blendxNative.dispatchPointer("mouseDown", 400, 300, 3)
    app.flush()
    assert.equal(contextOpen, true)
    globalThis.__blendxNative.dispatchPointer("mouseUp", 400, 300, 3)
    app.flush()
    assert.equal(contextSelected, "")
    assert.equal(contextOpen, true)
    click(globalThis.__blendxNative, 300, 292)
    app.flush()
    assert.equal(contextSelected, "inspect")
  } finally {
    app.stop()
  }
})

test("dropdown submenus open from the parent keyboard model", async () => {
  let selected = ""
  const h = React.createElement
  const app = render(
    h(DropdownMenu, null,
      h(DropdownMenuTrigger, { style: { width: 140, height: 34 } }, "Open"),
      h(DropdownMenuContent, { style: { width: 160, padding: 4, backgroundColor: "#111827" } },
        h(DropdownMenuItem, { value: "first", style: { width: 152, height: 30 } }, "First"),
        h(DropdownMenuSub, null,
          h(DropdownMenuSubTrigger, { value: "more", style: { width: 152, height: 30 } }, "More"),
          h(DropdownMenuSubContent, { style: { width: 150, padding: 4, backgroundColor: "#182235" } },
            h(DropdownMenuItem, { value: "nested", onSelect: () => { selected = "nested" }, style: { width: 142, height: 30 } }, "Nested"))))),
    { width: 420, height: 220, headless: true },
  )
  app.flush()
  try {
    click(globalThis.__blendxNative, 30, 20)
    globalThis.__blendxNative.dispatchKey("ArrowDown")
    globalThis.__blendxNative.dispatchKey("ArrowRight")
    app.flush()
    globalThis.__blendxNative.dispatchKey("Enter")
    app.flush()
    assert.equal(selected, "nested")
  } finally {
    app.stop()
  }
})
