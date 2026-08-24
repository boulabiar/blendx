import React from "react"
import {
  Accordion,
  AccordionContent,
  AccordionItem,
  AccordionTrigger,
  Collapsible,
  CollapsibleContent,
  CollapsibleTrigger,
  Tabs,
  TabsContent,
  TabsList,
  TabsTrigger,
  render,
} from "../src/index.js"

const { useState } = React
const C = {
  canvas: "#090d14" as const,
  surface: "#121925" as const,
  raised: "#1a2433" as const,
  border: "#2d3a4e" as const,
  text: "#edf2fa" as const,
  muted: "#8d99ab" as const,
  faint: "#5e6c80" as const,
  violet: "#7868f7" as const,
  cyan: "#45c8dc" as const,
  green: "#48d39b" as const,
}

const tabStyle = ({ selected, focused, disabled }: { selected: boolean; focused: boolean; disabled: boolean }) => ({
  width: 132, height: 38, paddingHorizontal: 12, alignItems: "center" as const, justifyContent: "center" as const,
  opacity: disabled ? 0.35 : 1, backgroundColor: selected ? "#292654" as const : "#151d29" as const,
  borderWidth: 1, borderColor: focused ? "#a99fff" as const : selected ? "#6258bd" as const : C.border,
  borderRadius: 9, hover: { backgroundColor: "#222c3c" as const },
})

function PanelTitle({ eyebrow, title, detail }: { eyebrow: string; title: string; detail: string }) {
  return <div style={{ gap: 5 }}><text style={{ color: C.violet, fontSize: 9 }}>{eyebrow}</text><text style={{ color: C.text, fontSize: 16 }}>{title}</text><text style={{ color: C.muted, fontSize: 10 }}>{detail}</text></div>
}

function DisclosureApp() {
  const [tab, setTab] = useState("workspace")
  const [section, setSection] = useState<string | string[]>("rendering")
  const [advanced, setAdvanced] = useState(true)
  const activeSection = Array.isArray(section) ? section[0] : section
  const itemStyle = ({ open }: { open: boolean; disabled: boolean }) => ({
    width: "100%" as const, backgroundColor: open ? "#192334" as const : "#151d29" as const,
    borderWidth: 1, borderColor: open ? "#3c4d67" as const : C.border, borderRadius: 10,
  })
  const triggerStyle = ({ focused }: { open: boolean; disabled: boolean; focused: boolean }) => ({
    width: "100%" as const, height: 48, paddingHorizontal: 13, flexDirection: "row" as const, alignItems: "center" as const,
    borderWidth: focused ? 1 : 0, borderColor: C.violet, borderRadius: 9,
  })

  return (
    <div style={{ width: "100%", height: "100%", padding: 24, gap: 18, backgroundColor: C.canvas }}>
      <div style={{ height: 62, flexShrink: 0, flexDirection: "row", alignItems: "center", gap: 13 }}>
        <div style={{ width: 42, height: 42, alignItems: "center", justifyContent: "center", backgroundColor: C.violet, borderRadius: 12 }}><text style={{ color: "#ffffff", fontSize: 17 }}>▦</text></div>
        <div style={{ flexGrow: 1, gap: 4 }}><text style={{ color: C.text, fontSize: 19 }}>Workspace settings</text><text style={{ color: C.muted, fontSize: 10 }}>Keyboard-first navigation and disclosure primitives</text></div>
        <badge style={{ paddingHorizontal: 10, paddingVertical: 6, backgroundColor: "#153329", borderRadius: 8 }}><text style={{ color: C.green, fontSize: 9 }}>STATE: {tab.toUpperCase()}</text></badge>
      </div>

      <div style={{ flexGrow: 1, minHeight: 0, flexDirection: "row", gap: 18 }}>
        <div style={{ width: 0, flexGrow: 1, minWidth: 0, padding: 18, backgroundColor: C.surface, borderWidth: 1, borderColor: C.border, borderRadius: 14 }}>
          <Tabs value={tab} onValueChange={setTab} style={{ width: "100%", height: "100%", gap: 18 }}>
            <TabsList style={{ width: "100%", height: 38, flexShrink: 0, flexDirection: "row", gap: 8 }}>
              <TabsTrigger value="workspace" style={tabStyle}><text style={{ color: C.text, fontSize: 10 }}>Workspace</text></TabsTrigger>
              <TabsTrigger value="runtime" style={tabStyle}><text style={{ color: C.text, fontSize: 10 }}>Runtime</text></TabsTrigger>
              <TabsTrigger value="network" style={tabStyle}><text style={{ color: C.text, fontSize: 10 }}>Network</text></TabsTrigger>
              <TabsTrigger value="locked" disabled style={tabStyle}><text style={{ color: C.muted, fontSize: 10 }}>Enterprise</text></TabsTrigger>
            </TabsList>

            <TabsContent value="workspace" style={{ flexGrow: 1, minHeight: 0, gap: 16 }}>
              <PanelTitle eyebrow="GENERAL" title="Workspace behavior" detail="Choose a section to inspect or change its native settings." />
              <Accordion value={section} onValueChange={setSection} collapsible style={{ width: "100%", gap: 8 }}>
                {[
                  ["rendering", "Rendering engine", "Blend2D raster workers, dirty regions and frame pacing", "4 worker threads · partial repaint enabled"],
                  ["storage", "Local storage", "Hermes bytecode, cached assets and session recovery", "42 MB cached · recovery snapshots on"],
                  ["shortcuts", "Keyboard shortcuts", "Focus traversal and component-specific navigation", "Tab, arrows, Home, End and Escape"],
                ].map(([value, title, detail, content]) => (
                  <AccordionItem key={value} value={value!} style={itemStyle}>
                    <AccordionTrigger style={triggerStyle}>
                      <div style={{ flexGrow: 1, gap: 4 }}><text style={{ color: C.text, fontSize: 11 }}>{title}</text><text style={{ color: C.muted, fontSize: 9 }}>{detail}</text></div>
                      <text style={{ color: activeSection === value ? C.violet : C.faint, fontSize: 12 }}>{activeSection === value ? "−" : "+"}</text>
                    </AccordionTrigger>
                    <AccordionContent style={{ paddingHorizontal: 13, paddingBottom: 13 }}>
                      <div style={{ height: 34, paddingHorizontal: 10, flexDirection: "row", alignItems: "center", backgroundColor: "#202b3c", borderRadius: 7 }}><div style={{ width: 6, height: 6, marginRight: 9, backgroundColor: C.cyan, borderRadius: 3 }} /><text style={{ color: C.muted, fontSize: 9 }}>{content}</text></div>
                    </AccordionContent>
                  </AccordionItem>
                ))}
              </Accordion>
            </TabsContent>

            <TabsContent value="runtime" style={{ flexGrow: 1, gap: 16 }}><PanelTitle eyebrow="RUNTIME" title="Hermes execution" detail="Compiled bytecode is embedded directly into the native executable." /><div style={{ height: 84, padding: 16, gap: 8, backgroundColor: C.raised, borderRadius: 10 }}><text style={{ color: C.green, fontSize: 11 }}>Runtime healthy</text><text style={{ color: C.muted, fontSize: 9 }}>Bytecode loaded · React reconciler active · native bridge connected</text></div></TabsContent>
            <TabsContent value="network" style={{ flexGrow: 1, gap: 16 }}><PanelTitle eyebrow="NETWORK" title="Offline by default" detail="BlendX rendering does not require a browser or network service." /><div style={{ height: 84, padding: 16, gap: 8, backgroundColor: C.raised, borderRadius: 10 }}><text style={{ color: C.cyan, fontSize: 11 }}>No active connections</text><text style={{ color: C.muted, fontSize: 9 }}>External resource loading remains application-controlled.</text></div></TabsContent>
          </Tabs>
        </div>

        <div style={{ width: 342, flexShrink: 0, gap: 14 }}>
          <div style={{ padding: 18, gap: 15, backgroundColor: C.surface, borderWidth: 1, borderColor: C.border, borderRadius: 14 }}>
            <PanelTitle eyebrow="DISCLOSURE" title="Advanced routing" detail="A standalone collapsible keeps optional controls out of the way." />
            <Collapsible open={advanced} onOpenChange={setAdvanced} style={{ width: "100%", gap: 8 }}>
              <CollapsibleTrigger style={({ open, focused }) => ({ width: "100%", height: 42, paddingHorizontal: 12, flexDirection: "row", alignItems: "center", backgroundColor: open ? "#272550" as const : C.raised, borderWidth: 1, borderColor: focused ? "#a99fff" as const : open ? "#6258bd" as const : C.border, borderRadius: 9 })}>
                <text style={{ flexGrow: 1, color: C.text, fontSize: 10 }}>Route configuration</text><text style={{ color: C.violet, fontSize: 12 }}>{advanced ? "−" : "+"}</text>
              </CollapsibleTrigger>
              <CollapsibleContent style={{ padding: 12, gap: 10, backgroundColor: C.raised, borderRadius: 9 }}>
                {[["Strategy", "Local first"], ["Fallback", "Disabled"], ["Retry budget", "120 ms"]].map(([label, value]) => <div key={label} style={{ height: 28, flexDirection: "row", alignItems: "center" }}><text style={{ flexGrow: 1, color: C.muted, fontSize: 9 }}>{label}</text><text style={{ color: C.text, fontSize: 9 }}>{value}</text></div>)}
              </CollapsibleContent>
            </Collapsible>
          </div>
          <div style={{ flexGrow: 1, padding: 18, gap: 12, backgroundColor: "#111a24", borderWidth: 1, borderColor: C.border, borderRadius: 14 }}>
            <text style={{ color: C.text, fontSize: 12 }}>Keyboard map</text>
            {["Tab · move between controls", "Arrows · switch tabs or headers", "Home / End · jump to edges", "Enter / Space · activate"].map((line) => <div key={line} style={{ height: 32, paddingHorizontal: 9, justifyContent: "center", backgroundColor: C.raised, borderRadius: 7 }}><text style={{ color: C.muted, fontSize: 9 }}>{line}</text></div>)}
          </div>
        </div>
      </div>
    </div>
  )
}

const headless = process.argv.includes("--headless")
const screenshot = process.argv.find((argument) => argument.startsWith("--screenshot="))?.slice("--screenshot=".length)
const mountedAt = performance.now()
const app = render(<DisclosureApp />, { title: "BlendX · Workspace Settings", width: 1180, height: 700, threads: 4, headless })
const mountTimeMs = performance.now() - mountedAt
if (headless) setTimeout(() => {
  if (screenshot) app.renderer.captureScreenshot(screenshot)
  console.log(JSON.stringify({ mountTimeMs: Number(mountTimeMs.toFixed(2)), ...app.renderer.getStats() }))
  app.stop()
}, 300)
process.on("SIGINT", () => app.stop())
