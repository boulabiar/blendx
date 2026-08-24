import React from "react"
import {
  Checkbox,
  RadioGroup,
  RadioGroupIndicator,
  RadioGroupItem,
  Slider,
  Switch,
  SwitchThumb,
  render,
} from "../src/index.js"

const { useState } = React
const C = {
  canvas: "#090d14" as const,
  surface: "#121925" as const,
  raised: "#1a2433" as const,
  border: "#2c3a4e" as const,
  text: "#eef3fb" as const,
  muted: "#8e9aad" as const,
  faint: "#5e6b7e" as const,
  violet: "#7767f7" as const,
  cyan: "#45c8db" as const,
  green: "#48d39b" as const,
}

function Card({ title, eyebrow, children }: { title: string; eyebrow: string; children: React.ReactNode }) {
  return (
    <div style={{ width: 0, flexGrow: 1, minWidth: 0, padding: 20, gap: 18, backgroundColor: C.surface, borderWidth: 1, borderColor: C.border, borderRadius: 14 }}>
      <div style={{ gap: 5 }}>
        <text style={{ color: C.violet, fontSize: 9 }}>{eyebrow}</text>
        <text style={{ color: C.text, fontSize: 16 }}>{title}</text>
      </div>
      {children}
    </div>
  )
}

function ControlsApp() {
  const [sync, setSync] = useState(true)
  const [telemetry, setTelemetry] = useState(false)
  const [runtime, setRuntime] = useState("balanced")
  const [alerts, setAlerts] = useState(true)
  const [volume, setVolume] = useState(68)
  const [window, setWindow] = useState<[number, number]>([24, 76])

  const checkboxStyle = ({ focused, disabled }: { checked: boolean | "indeterminate"; focused: boolean; disabled: boolean }) => ({
    width: "100%" as const, height: 46, paddingHorizontal: 10, flexDirection: "row" as const, alignItems: "center" as const, gap: 11,
    opacity: disabled ? 0.45 : 1, backgroundColor: focused ? "#202c3e" as const : C.raised,
    borderWidth: 1, borderColor: focused ? C.violet : C.border, borderRadius: 9,
    hover: { backgroundColor: "#202c3e" as const },
  })

  return (
    <div style={{ width: "100%", height: "100%", padding: 24, gap: 18, backgroundColor: C.canvas }}>
      <div style={{ height: 62, flexShrink: 0, flexDirection: "row", alignItems: "center", gap: 13 }}>
        <div style={{ width: 42, height: 42, alignItems: "center", justifyContent: "center", backgroundColor: C.violet, borderRadius: 12 }}>
          <text style={{ color: "#ffffff", fontSize: 18 }}>◇</text>
        </div>
        <div style={{ flexGrow: 1, gap: 4 }}>
          <text style={{ color: C.text, fontSize: 19 }}>Control surface</text>
          <text style={{ color: C.muted, fontSize: 10 }}>Native pointer, keyboard, focus and controlled-state primitives</text>
        </div>
        <badge style={{ paddingHorizontal: 10, paddingVertical: 6, backgroundColor: "#153329", borderRadius: 8 }}>
          <text style={{ color: C.green, fontSize: 9 }}>4 NEW CONTROLS</text>
        </badge>
      </div>

      <div style={{ flexGrow: 1, minHeight: 0, flexDirection: "row", gap: 18 }}>
        <Card eyebrow="SELECTION" title="Preferences">
          <div style={{ gap: 8 }}>
            <Checkbox checked={sync} onCheckedChange={(value) => setSync(value === true)} style={checkboxStyle}>
              {({ checked }) => <div style={{ width: "100%", flexDirection: "row", alignItems: "center", gap: 11 }}>
                <div style={{ width: 20, height: 20, alignItems: "center", justifyContent: "center", backgroundColor: checked ? C.violet : "#111824", borderWidth: 1, borderColor: checked ? C.violet : "#46546a", borderRadius: 6 }}>
                  {checked && <text style={{ color: "#ffffff", fontSize: 12 }}>✓</text>}
                </div>
                <div style={{ gap: 3 }}><text style={{ color: C.text, fontSize: 11 }}>Sync workspace</text><text style={{ color: C.muted, fontSize: 9 }}>Keep local state mirrored automatically</text></div>
              </div>}
            </Checkbox>
            <Checkbox checked={telemetry} onCheckedChange={(value) => setTelemetry(value === true)} style={checkboxStyle}>
              {({ checked }) => <div style={{ width: "100%", flexDirection: "row", alignItems: "center", gap: 11 }}>
                <div style={{ width: 20, height: 20, alignItems: "center", justifyContent: "center", backgroundColor: checked ? C.violet : "#111824", borderWidth: 1, borderColor: checked ? C.violet : "#46546a", borderRadius: 6 }}>
                  {checked && <text style={{ color: "#ffffff", fontSize: 12 }}>✓</text>}
                </div>
                <div style={{ gap: 3 }}><text style={{ color: C.text, fontSize: 11 }}>Share diagnostics</text><text style={{ color: C.muted, fontSize: 9 }}>Send anonymous rendering statistics</text></div>
              </div>}
            </Checkbox>
            <Checkbox disabled style={checkboxStyle}>
              <div style={{ width: 20, height: 20, backgroundColor: "#111824", borderWidth: 1, borderColor: "#46546a", borderRadius: 6 }} />
              <div style={{ gap: 3 }}><text style={{ color: C.text, fontSize: 11 }}>Cloud backups</text><text style={{ color: C.muted, fontSize: 9 }}>Unavailable in the local profile</text></div>
            </Checkbox>
          </div>

          <separator style={{ width: "100%", height: 1, color: C.border }} />
          <div style={{ gap: 10 }}>
            <text style={{ color: C.muted, fontSize: 10 }}>Execution profile</text>
            <RadioGroup value={runtime} onValueChange={setRuntime} orientation="horizontal" style={{ width: "100%", height: 68, gap: 8 }}>
              {[["quiet", "Quiet"], ["balanced", "Balanced"], ["burst", "Burst"]].map(([value, label]) => (
                <RadioGroupItem key={value} value={value!} style={({ checked, focused }) => ({ width: 0, flexGrow: 1, minWidth: 0, padding: 10, gap: 7, alignItems: "center", justifyContent: "center", backgroundColor: checked ? "#25264b" as const : C.raised, borderWidth: 1, borderColor: focused ? C.violet : checked ? "#5f58bd" as const : C.border, borderRadius: 9 })}>
                  <div style={{ width: 16, height: 16, alignItems: "center", justifyContent: "center", borderWidth: 1, borderColor: "#7b86a0", borderRadius: 8 }}>
                    <RadioGroupIndicator style={{ width: 8, height: 8, backgroundColor: C.violet, borderRadius: 4 }} />
                  </div>
                  <text style={{ color: C.text, fontSize: 10 }}>{label}</text>
                </RadioGroupItem>
              ))}
            </RadioGroup>
          </div>
        </Card>

        <Card eyebrow="CONTINUOUS" title="Output tuning">
          <div style={{ padding: 14, gap: 14, backgroundColor: C.raised, borderRadius: 10 }}>
            <div style={{ flexDirection: "row", alignItems: "center" }}>
              <div style={{ flexGrow: 1, gap: 3 }}><text style={{ color: C.text, fontSize: 11 }}>Live alerts</text><text style={{ color: C.muted, fontSize: 9 }}>Notify when a render exceeds its budget</text></div>
              <Switch checked={alerts} onCheckedChange={setAlerts} style={({ checked, focused }) => ({ width: 48, height: 26, padding: 3, flexDirection: "row", alignItems: "center", justifyContent: checked ? "end" : "start", backgroundColor: checked ? C.cyan : "#303a49", borderWidth: focused ? 2 : 1, borderColor: focused ? "#a6edf6" : checked ? C.cyan : "#46546a", borderRadius: 13 })}>
                <SwitchThumb style={({ checked }) => ({ width: 20, height: 20, backgroundColor: checked ? "#071217" : "#dce4ef", borderRadius: 10 })} />
              </Switch>
            </div>
          </div>

          <div style={{ padding: 14, gap: 13, backgroundColor: C.raised, borderRadius: 10 }}>
            <div style={{ flexDirection: "row" }}><text style={{ flexGrow: 1, color: C.text, fontSize: 11 }}>Output level</text><text style={{ color: C.cyan, fontSize: 11 }}>{volume}%</text></div>
            <Slider value={volume} onValueChange={(value) => setVolume(value as number)} step={1} style={({ focused }) => ({ width: "100%", height: 24, borderWidth: focused ? 1 : 0, borderColor: C.cyan, borderRadius: 12 })} rangeStyle={{ backgroundColor: C.cyan }} thumbStyle={{ borderColor: C.cyan }} />
            <div style={{ flexDirection: "row" }}><text style={{ flexGrow: 1, color: C.faint, fontSize: 8 }}>0</text><text style={{ color: C.faint, fontSize: 8 }}>100</text></div>
          </div>

          <div style={{ padding: 14, gap: 13, backgroundColor: C.raised, borderRadius: 10 }}>
            <div style={{ flexDirection: "row" }}><text style={{ flexGrow: 1, color: C.text, fontSize: 11 }}>Adaptive window</text><text style={{ color: C.violet, fontSize: 11 }}>{window[0]}–{window[1]} ms</text></div>
            <Slider value={window} onValueChange={(value) => setWindow(value as [number, number])} min={0} max={100} step={2} style={{ width: "100%", height: 24 }} rangeStyle={{ backgroundColor: C.violet }} thumbStyle={{ borderColor: C.violet }} />
            <text style={{ color: C.muted, fontSize: 9 }}>Drag either thumb or use arrow keys for precise steps.</text>
          </div>
        </Card>
      </div>
    </div>
  )
}

const headless = process.argv.includes("--headless")
const screenshot = process.argv.find((argument) => argument.startsWith("--screenshot="))?.slice("--screenshot=".length)
const mountedAt = performance.now()
const app = render(<ControlsApp />, { title: "BlendX · Control Surface", width: 1180, height: 700, threads: 4, headless })
const mountTimeMs = performance.now() - mountedAt
if (headless) setTimeout(() => {
  if (screenshot) app.renderer.captureScreenshot(screenshot)
  console.log(JSON.stringify({ mountTimeMs: Number(mountTimeMs.toFixed(2)), ...app.renderer.getStats() }))
  app.stop()
}, 300)
process.on("SIGINT", () => app.stop())
