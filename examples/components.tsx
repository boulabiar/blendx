import React from "react"
import {
  Combobox,
  ComboboxContent,
  ComboboxEmpty,
  ComboboxInput,
  ComboboxItem,
  ComboboxList,
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
  Tooltip,
  TooltipContent,
  TooltipProvider,
  TooltipTrigger,
  render,
} from "../src/index.js"

const { useState } = React
const openTooltipForScreenshot = process.argv.includes("--open-tooltip")
const initialFramework = process.argv.find((argument) => argument.startsWith("--framework="))?.slice("--framework=".length) ?? null

const C = {
  canvas: "#0a0d13" as const,
  surface: "#131923" as const,
  raised: "#1b2330" as const,
  border: "#2d3949" as const,
  text: "#e8edf5" as const,
  muted: "#929dad" as const,
  faint: "#626f81" as const,
  blue: "#69a9ff" as const,
  violet: "#a18cf8" as const,
  green: "#4bd39d" as const,
}

const triggerStyle = {
  width: "100%" as const,
  height: 42,
  paddingHorizontal: 12,
  flexDirection: "row" as const,
  alignItems: "center" as const,
  backgroundColor: C.raised,
  borderWidth: 1,
  borderColor: C.border,
  borderRadius: 9,
  hover: { backgroundColor: "#232e3d" as const, borderColor: "#42536a" as const },
  active: { backgroundColor: "#111722" as const },
}

const contentStyle = {
  width: 300,
  maxHeight: 230,
  padding: 6,
  gap: 4,
  overflow: "scroll" as const,
  backgroundColor: "#1a222e" as const,
  borderWidth: 1,
  borderColor: "#3a485c" as const,
  borderRadius: 10,
}

const itemStyle = ({ highlighted, selected, disabled }: { highlighted: boolean; selected: boolean; disabled: boolean }) => ({
  width: "100%" as const,
  height: 36,
  paddingHorizontal: 10,
  flexDirection: "row" as const,
  alignItems: "center" as const,
  opacity: disabled ? 0.4 : 1,
  backgroundColor: highlighted ? "#29384d" as const : selected ? "#202f48" as const : "#1a222e" as const,
  borderRadius: 7,
})

function Card({ title, description, children }: { title: string; description: string; children: React.ReactNode }) {
  return (
    <div style={{ width: 0, flexGrow: 1, minWidth: 0, height: 245, padding: 16, gap: 12, backgroundColor: C.surface, borderWidth: 1, borderColor: C.border, borderRadius: 12 }}>
      <div style={{ gap: 4 }}>
        <text style={{ color: C.text, fontSize: 14 }}>{title}</text>
        <text style={{ color: C.muted, fontSize: 10 }}>{description}</text>
      </div>
      {children}
    </div>
  )
}

function ComponentsApp() {
  const [model, setModel] = useState("hermes")
  const [framework, setFramework] = useState<string | string[] | null>(initialFramework)
  const frameworks = ["React", "Preact", "Solid", "Vue", "Svelte", "Lit"]

  return (
    <TooltipProvider delayDuration={180}>
      <div style={{ width: "100%", height: "100%", padding: 24, gap: 20, backgroundColor: C.canvas }}>
        <div style={{ height: 58, flexShrink: 0, flexDirection: "row", alignItems: "center", gap: 12 }}>
          <div style={{ width: 40, height: 40, alignItems: "center", justifyContent: "center", backgroundColor: "#604fe0", borderRadius: 11 }}>
            <text style={{ color: "#ffffff", fontSize: 18 }}>⌘</text>
          </div>
          <div style={{ flexGrow: 1, gap: 3 }}>
            <text style={{ color: C.text, fontSize: 18 }}>Floating controls</text>
            <text style={{ color: C.muted, fontSize: 10 }}>Native hover, focus, anchored placement and outside-click dismissal</text>
          </div>
          <badge style={{ paddingHorizontal: 9, paddingVertical: 5, backgroundColor: "#173128", borderRadius: 7 }}>
            <text style={{ color: C.green, fontSize: 9 }}>INTERACTIVE</text>
          </badge>
        </div>

        <separator style={{ width: "100%", height: 1, flexShrink: 0, color: C.border }} />

        <div style={{ height: 245, flexShrink: 0, flexDirection: "row", gap: 16 }}>
          <Card title="Tooltip" description="Hover or focus the action to reveal context.">
            <div style={{ flexGrow: 1, alignItems: "center", justifyContent: "center" }}>
              <Tooltip defaultOpen={openTooltipForScreenshot}>
                <TooltipTrigger style={{ width: 160, height: 42, alignItems: "center", justifyContent: "center", backgroundColor: "#26344a", borderWidth: 1, borderColor: "#405575", borderRadius: 9, hover: { backgroundColor: "#30435f" }, active: { backgroundColor: "#182232" } }}>
                  <text style={{ color: C.text, fontSize: 11 }}>Hover for details</text>
                </TooltipTrigger>
                <TooltipContent side="top" align="center" sideOffset={8} style={{ width: 210, height: 48, padding: 10, alignItems: "center", justifyContent: "center", backgroundColor: "#242d3b", borderWidth: 1, borderColor: "#435168", borderRadius: 8 }}>
                  <text style={{ color: C.text, fontSize: 10 }}>Painted in a native anchored layer</text>
                </TooltipContent>
              </Tooltip>
            </div>
          </Card>

          <Card title="Select" description="Mouse and keyboard-friendly single selection.">
            <Select value={model} onValueChange={setModel}>
              <SelectTrigger style={triggerStyle}>
                <SelectValue placeholder="Choose a runtime" style={{ flexGrow: 1 }} />
                <text style={{ color: C.faint, fontSize: 11 }}>▾</text>
              </SelectTrigger>
              <SelectContent side="bottom" align="start" sideOffset={6} style={contentStyle}>
                <SelectItem value="hermes" style={itemStyle}><text style={{ color: C.text, fontSize: 11 }}>Hermes bytecode</text></SelectItem>
                <SelectItem value="javascriptcore" style={itemStyle}><text style={{ color: C.text, fontSize: 11 }}>JavaScriptCore</text></SelectItem>
                <SelectItem value="v8" style={itemStyle}><text style={{ color: C.text, fontSize: 11 }}>V8 isolate</text></SelectItem>
                <SelectItem value="disabled" disabled style={itemStyle}><text style={{ color: C.muted, fontSize: 11 }}>Unavailable runtime</text></SelectItem>
              </SelectContent>
            </Select>
            <div style={{ flexGrow: 1 }} />
            <text style={{ color: C.faint, fontSize: 9 }}>Selected: {model}</text>
          </Card>

          <Card title="Combobox" description="Type to filter, then choose a framework.">
            <Combobox items={frameworks} value={framework} defaultInputValue={initialFramework ?? ""} onValueChange={setFramework}>
              <ComboboxInput placeholder="Search frameworks…" style={{ ...triggerStyle, color: C.text }} />
              <ComboboxContent side="bottom" align="start" sideOffset={6} style={contentStyle}>
                <ComboboxEmpty style={{ height: 38, padding: 10 }}><text style={{ color: C.muted, fontSize: 10 }}>No framework found</text></ComboboxEmpty>
                <ComboboxList>
                  {(item) => <ComboboxItem key={item} value={item} style={itemStyle}><text style={{ color: C.text, fontSize: 11 }}>{item}</text></ComboboxItem>}
                </ComboboxList>
              </ComboboxContent>
            </Combobox>
            <div style={{ flexGrow: 1 }} />
            <text style={{ color: C.faint, fontSize: 9 }}>Selected: {Array.isArray(framework) ? framework.join(", ") : framework ?? "none"}</text>
          </Card>
        </div>

        <div style={{ flexGrow: 1, minHeight: 0, padding: 18, gap: 12, backgroundColor: C.surface, borderWidth: 1, borderColor: C.border, borderRadius: 12 }}>
          <text style={{ color: C.text, fontSize: 13 }}>Interaction foundation</text>
          <text style={{ color: C.muted, fontSize: 10 }}>These primitives share the same retained renderer features:</text>
          {[
            ["Pointer states", "mouse enter/leave plus reusable hover and active style overrides"],
            ["Element refs", "stable native IDs exposed through React refs"],
            ["Floating placement", "trigger-relative top/right/bottom/left placement with viewport clamping"],
            ["Dismissal", "native mouse-down-outside events for layered controls"],
            ["Keyboard", "Tab focus traversal and Enter/Space button activation"],
          ].map(([title, detail]) => (
            <div key={title} style={{ height: 34, paddingHorizontal: 10, flexDirection: "row", alignItems: "center", backgroundColor: C.raised, borderRadius: 7 }}>
              <div style={{ width: 6, height: 6, backgroundColor: C.violet, borderRadius: 3 }} />
              <text style={{ marginLeft: 9, width: 145, color: C.text, fontSize: 10 }}>{title}</text>
              <text style={{ color: C.muted, fontSize: 10 }}>{detail}</text>
            </div>
          ))}
        </div>
      </div>
    </TooltipProvider>
  )
}

const headless = process.argv.includes("--headless")
const screenshot = process.argv.find((argument) => argument.startsWith("--screenshot="))?.slice("--screenshot=".length)
const mountedAt = performance.now()
const app = render(<ComponentsApp />, { title: "BlendX · Floating Controls", width: 1180, height: 700, threads: 4, headless })
const mountTimeMs = performance.now() - mountedAt
if (headless) setTimeout(() => {
  if (screenshot) app.renderer.captureScreenshot(screenshot)
  console.log(JSON.stringify({ mountTimeMs: Number(mountTimeMs.toFixed(2)), ...app.renderer.getStats() }))
  app.stop()
}, 300)
process.on("SIGINT", () => app.stop())
