import React from "react"
import {
  ContextMenu,
  ContextMenuCheckboxItem,
  ContextMenuContent,
  ContextMenuItem,
  ContextMenuLabel,
  ContextMenuSeparator,
  ContextMenuTrigger,
  DropdownMenu,
  DropdownMenuCheckboxItem,
  DropdownMenuContent,
  DropdownMenuItem,
  DropdownMenuLabel,
  DropdownMenuRadioGroup,
  DropdownMenuRadioItem,
  DropdownMenuSeparator,
  DropdownMenuTrigger,
  render,
} from "../src/index.js"

const { useState } = React
const openDropdownForScreenshot = process.argv.includes("--open-dropdown")
const openContextForScreenshot = process.argv.includes("--open-context")
const C = {
  canvas: "#090d14" as const,
  surface: "#121925" as const,
  raised: "#1a2433" as const,
  border: "#2d3a4e" as const,
  text: "#edf2fa" as const,
  muted: "#8e9aac" as const,
  faint: "#5f6d80" as const,
  violet: "#7868f7" as const,
  cyan: "#45c8dc" as const,
  green: "#48d39b" as const,
}

const menuStyle = { width: 260, padding: 6, gap: 3, backgroundColor: "#171f2b" as const, borderWidth: 1, borderColor: "#3a4960" as const, borderRadius: 11, zIndex: 20 }
const itemStyle = ({ highlighted, disabled }: { highlighted: boolean; disabled: boolean }) => ({
  width: "100%" as const, height: 36, paddingHorizontal: 10, flexDirection: "row" as const, alignItems: "center" as const,
  opacity: disabled ? 0.35 : 1, backgroundColor: highlighted ? "#29384d" as const : "#171f2b" as const, borderRadius: 7,
})
const checkedItemStyle = ({ highlighted, disabled }: { highlighted: boolean; disabled: boolean; checked: boolean }) => itemStyle({ highlighted, disabled })

function MenuRow({ icon, label, hint, checked }: { icon: string; label: string; hint?: string; checked?: boolean }) {
  return <div style={{ width: "100%", flexDirection: "row", alignItems: "center" }}><text style={{ width: 22, color: checked ? C.violet : C.muted, fontSize: 11 }}>{checked ? "✓" : icon}</text><text style={{ flexGrow: 1, color: C.text, fontSize: 10 }}>{label}</text>{hint && <text style={{ color: C.faint, fontSize: 8 }}>{hint}</text>}</div>
}

function MenusApp() {
  const [showHidden, setShowHidden] = useState(false)
  const [compact, setCompact] = useState(true)
  const [sort, setSort] = useState("name")
  const [status, setStatus] = useState("Ready")
  const files = [
    ["◇", "src", "8 items", true], ["◇", "examples", "12 items", true], ["TS", "package.json", "3.2 KB", false],
    ["MD", "README.md", "6.8 KB", false], ["C+", "native", "4 items", true], ["CM", "CMakeLists.txt", "9.1 KB", false], ["·", ".cache", "1 item", true],
  ] as const
  return (
    <div style={{ width: "100%", height: "100%", padding: 24, gap: 18, backgroundColor: C.canvas }}>
      <div style={{ height: 62, flexShrink: 0, flexDirection: "row", alignItems: "center", gap: 13 }}>
        <div style={{ width: 42, height: 42, alignItems: "center", justifyContent: "center", backgroundColor: C.violet, borderRadius: 12 }}><text style={{ color: "#ffffff", fontSize: 17 }}>⌘</text></div>
        <div style={{ flexGrow: 1, gap: 4 }}><text style={{ color: C.text, fontSize: 19 }}>File workspace</text><text style={{ color: C.muted, fontSize: 10 }}>Anchored actions and pointer-positioned context menus</text></div>
        <badge style={{ paddingHorizontal: 10, paddingVertical: 6, backgroundColor: "#153329", borderRadius: 8 }}><text style={{ color: C.green, fontSize: 9 }}>{status.toUpperCase()}</text></badge>
      </div>

      <div style={{ flexGrow: 1, minHeight: 0, flexDirection: "row", gap: 18 }}>
        <ContextMenu>
          <ContextMenuTrigger style={{ width: 0, flexGrow: 1, minWidth: 0, padding: 18, gap: 14, backgroundColor: C.surface, borderWidth: 1, borderColor: C.border, borderRadius: 14 }}>
            <div style={{ height: 38, flexShrink: 0, flexDirection: "row", alignItems: "center" }}><div style={{ flexGrow: 1, gap: 4 }}><text style={{ color: C.text, fontSize: 14 }}>blendx / master</text><text style={{ color: C.muted, fontSize: 9 }}>Right-click anywhere in this panel</text></div><text style={{ color: C.faint, fontSize: 9 }}>6 entries</text></div>
            <separator style={{ width: "100%", height: 1, color: C.border }} />
            <div style={{ gap: 6 }}>
              {files.filter(([, name]) => showHidden || name !== ".cache").map(([icon, name, meta, folder]) => <div key={name} style={{ height: compact ? 46 : 56, paddingHorizontal: 12, flexDirection: "row", alignItems: "center", backgroundColor: C.raised, borderRadius: 8 }}><div style={{ width: 32, height: 28, alignItems: "center", justifyContent: "center", backgroundColor: folder ? "#282653" : "#202b3c", borderRadius: 7 }}><text style={{ color: folder ? C.violet : C.cyan, fontSize: 9 }}>{icon}</text></div><text style={{ marginLeft: 11, flexGrow: 1, color: C.text, fontSize: 10 }}>{name}</text><text style={{ color: C.faint, fontSize: 8 }}>{meta}</text></div>)}
            </div>
          </ContextMenuTrigger>
          <ContextMenuContent style={menuStyle}>
            <ContextMenuLabel style={{ height: 27, paddingHorizontal: 10, justifyContent: "center" }}><text style={{ color: C.faint, fontSize: 8 }}>FILE ACTIONS</text></ContextMenuLabel>
            <ContextMenuItem value="open" style={itemStyle} onSelect={() => setStatus("Opened selection")}><MenuRow icon="↗" label="Open" hint="Enter" /></ContextMenuItem>
            <ContextMenuItem value="rename" style={itemStyle} onSelect={() => setStatus("Rename requested")}><MenuRow icon="✎" label="Rename" hint="F2" /></ContextMenuItem>
            <ContextMenuItem value="duplicate" style={itemStyle} onSelect={() => setStatus("Duplicated")}><MenuRow icon="□" label="Duplicate" /></ContextMenuItem>
            <ContextMenuSeparator style={{ width: "100%", height: 1, marginTop: 3, marginBottom: 3, color: C.border }} />
            <ContextMenuCheckboxItem value="hidden" checked={showHidden} onCheckedChange={setShowHidden} style={checkedItemStyle}><MenuRow icon="·" label="Show hidden files" checked={showHidden} /></ContextMenuCheckboxItem>
            <ContextMenuItem value="delete" style={itemStyle} onSelect={() => setStatus("Moved to trash")}><MenuRow icon="×" label="Move to trash" /></ContextMenuItem>
          </ContextMenuContent>
        </ContextMenu>

        <div style={{ width: 342, flexShrink: 0, gap: 14 }}>
          <div style={{ zIndex: 10, padding: 18, gap: 15, backgroundColor: C.surface, borderWidth: 1, borderColor: C.border, borderRadius: 14 }}>
            <div style={{ gap: 5 }}><text style={{ color: C.violet, fontSize: 9 }}>DROPDOWN</text><text style={{ color: C.text, fontSize: 15 }}>View options</text><text style={{ color: C.muted, fontSize: 9 }}>Click or use arrow keys to navigate.</text></div>
            <DropdownMenu defaultOpen={openDropdownForScreenshot}>
              <DropdownMenuTrigger style={{ width: "100%", height: 42, paddingHorizontal: 12, flexDirection: "row", alignItems: "center", backgroundColor: C.raised, borderWidth: 1, borderColor: C.border, borderRadius: 9 }}><text style={{ flexGrow: 1, color: C.text, fontSize: 10 }}>Configure view</text><text style={{ color: C.violet, fontSize: 11 }}>▾</text></DropdownMenuTrigger>
              <DropdownMenuContent side="bottom" align="end" sideOffset={7} style={menuStyle}>
                <DropdownMenuLabel style={{ height: 27, paddingHorizontal: 10, justifyContent: "center" }}><text style={{ color: C.faint, fontSize: 8 }}>APPEARANCE</text></DropdownMenuLabel>
                <DropdownMenuCheckboxItem value="compact" checked={compact} onCheckedChange={setCompact} style={checkedItemStyle}><MenuRow icon="·" label="Compact rows" checked={compact} /></DropdownMenuCheckboxItem>
                <DropdownMenuCheckboxItem value="hidden" checked={showHidden} onCheckedChange={setShowHidden} style={checkedItemStyle}><MenuRow icon="·" label="Show hidden files" checked={showHidden} /></DropdownMenuCheckboxItem>
                <DropdownMenuSeparator style={{ width: "100%", height: 1, marginTop: 3, marginBottom: 3, color: C.border }} />
                <DropdownMenuLabel style={{ height: 27, paddingHorizontal: 10, justifyContent: "center" }}><text style={{ color: C.faint, fontSize: 8 }}>SORT BY</text></DropdownMenuLabel>
                <DropdownMenuRadioGroup value={sort} onValueChange={setSort}>
                  {["name", "modified", "size"].map((value) => <DropdownMenuRadioItem key={value} value={value} style={({ highlighted, disabled, checked }) => itemStyle({ highlighted, disabled })}><MenuRow icon="·" label={value[0]!.toUpperCase() + value.slice(1)} checked={sort === value} /></DropdownMenuRadioItem>)}
                </DropdownMenuRadioGroup>
                <DropdownMenuSeparator style={{ width: "100%", height: 1, marginTop: 3, marginBottom: 3, color: C.border }} />
                <DropdownMenuItem value="refresh" style={itemStyle} onSelect={() => setStatus("View refreshed")}><MenuRow icon="↻" label="Refresh" hint="Ctrl R" /></DropdownMenuItem>
              </DropdownMenuContent>
            </DropdownMenu>
          </div>
          <div style={{ flexGrow: 1, padding: 18, gap: 12, backgroundColor: "#111a24", borderWidth: 1, borderColor: C.border, borderRadius: 14 }}><text style={{ color: C.text, fontSize: 12 }}>Interaction model</text>{["Pointer hover highlights items", "Outside click dismisses layers", "Arrows, Home and End navigate", "Type a letter to jump", "Checkbox and radio states persist"].map((line) => <div key={line} style={{ height: 34, paddingHorizontal: 10, flexDirection: "row", alignItems: "center", backgroundColor: C.raised, borderRadius: 7 }}><div style={{ width: 5, height: 5, marginRight: 9, backgroundColor: C.violet, borderRadius: 3 }} /><text style={{ color: C.muted, fontSize: 9 }}>{line}</text></div>)}</div>
        </div>
      </div>
    </div>
  )
}

const headless = process.argv.includes("--headless")
const screenshot = process.argv.find((argument) => argument.startsWith("--screenshot="))?.slice("--screenshot=".length)
const mountedAt = performance.now()
const app = render(<MenusApp />, { title: "BlendX · File Workspace", width: 1180, height: 700, threads: 4, headless })
const mountTimeMs = performance.now() - mountedAt
if (headless) {
  if (openContextForScreenshot) setTimeout(() => {
    app.renderer.dispatchPointer("mouseDown", 390, 330, 3)
    app.renderer.dispatchPointer("mouseUp", 390, 330, 3)
  }, 80)
  setTimeout(() => {
    if (screenshot) app.renderer.captureScreenshot(screenshot)
    console.log(JSON.stringify({ mountTimeMs: Number(mountTimeMs.toFixed(2)), ...app.renderer.getStats() }))
    app.stop()
  }, 350)
}
process.on("SIGINT", () => app.stop())
