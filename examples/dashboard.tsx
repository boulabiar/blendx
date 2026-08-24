/**
 * Real-time operations dashboard for BlendX.
 *
 * This example combines frequently changing text, progress bars, retained
 * canvas charts, status badges, and a virtualized process table.
 */

import React from "react"
import { render } from "../src/index.js"
import type { CanvasCommand, Color } from "../src/types.js"

const { memo, useEffect, useMemo, useState } = React

const C = {
  canvas: "#0a0d12" as const,
  sidebar: "#0e1219" as const,
  surface: "#141923" as const,
  raised: "#1a202b" as const,
  border: "#293241" as const,
  text: "#e7edf5" as const,
  muted: "#8e9aab" as const,
  faint: "#5e6a7c" as const,
  blue: "#64a7ff" as const,
  cyan: "#4ed4dc" as const,
  green: "#4bd49d" as const,
  violet: "#9a87f7" as const,
  amber: "#f2bd66" as const,
  red: "#ee7782" as const,
}

function wave(index: number, tick: number, phase: number) {
  return 0.5 + Math.sin(index * 0.34 + tick * 0.09 + phase) * 0.23
    + Math.sin(index * 0.11 + tick * 0.035 + phase * 2) * 0.17
}

function sparkline(width: number, height: number, tick: number, phase: number, color: Color): CanvasCommand[] {
  const commands: CanvasCommand[] = [
    { kind: "fillRect", x: 0, y: 0, width, height, color: "#111722", radius: 7 },
  ]
  const points = 34
  for (let index = 1; index < points; index++) {
    const previous = Math.max(0.08, Math.min(0.92, wave(index - 1, tick, phase)))
    const current = Math.max(0.08, Math.min(0.92, wave(index, tick, phase)))
    commands.push({
      kind: "line",
      x1: ((index - 1) / (points - 1)) * width,
      y1: height - previous * height,
      x2: (index / (points - 1)) * width,
      y2: height - current * height,
      color,
      widthPx: 2,
    })
  }
  return commands
}

function networkChart(tick: number): CanvasCommand[] {
  const width = 650
  const height = 178
  const commands: CanvasCommand[] = [
    { kind: "fillRect", x: 0, y: 0, width, height, color: "#10151d", radius: 8 },
  ]
  for (let line = 1; line < 4; line++) {
    commands.push({ kind: "line", x1: 0, y1: line * 42, x2: width, y2: line * 42, color: "#202a38", widthPx: 1 })
  }
  const points = 72
  for (let index = 1; index < points; index++) {
    const x1 = ((index - 1) / (points - 1)) * width
    const x2 = (index / (points - 1)) * width
    const incoming1 = Math.max(0.1, Math.min(0.9, wave(index - 1, tick, 0.4)))
    const incoming2 = Math.max(0.1, Math.min(0.9, wave(index, tick, 0.4)))
    const outgoing1 = Math.max(0.1, Math.min(0.9, wave(index - 1, tick, 2.2) * 0.72))
    const outgoing2 = Math.max(0.1, Math.min(0.9, wave(index, tick, 2.2) * 0.72))
    commands.push({ kind: "line", x1, y1: height - incoming1 * height, x2, y2: height - incoming2 * height, color: C.cyan, widthPx: 2 })
    commands.push({ kind: "line", x1, y1: height - outgoing1 * height, x2, y2: height - outgoing2 * height, color: C.violet, widthPx: 2 })
  }
  return commands
}

function NavItem({ label, active }: { label: string; active?: boolean }) {
  return (
    <button
      style={{
        height: 38,
        paddingHorizontal: 11,
        flexDirection: "row",
        alignItems: "center",
        gap: 10,
        backgroundColor: active ? "#1c2736" : C.sidebar,
        borderRadius: 8,
      }}
    >
      <div style={{ width: 7, height: 7, backgroundColor: active ? C.blue : C.faint, borderRadius: 3 }} />
      <text style={{ color: active ? C.text : C.muted, fontSize: 12 }}>{label}</text>
    </button>
  )
}

function MetricCard({
  label,
  value,
  detail,
  commands,
  color,
}: {
  label: string
  value: string
  detail: string
  commands: CanvasCommand[]
  color: Color
}) {
  return (
    <div style={{ flexGrow: 1, minWidth: 0, height: 142, padding: 14, gap: 9, backgroundColor: C.surface, borderWidth: 1, borderColor: C.border, borderRadius: 11 }}>
      <div style={{ flexDirection: "row", alignItems: "center" }}>
        <text style={{ flexGrow: 1, color: C.muted, fontSize: 10 }}>{label}</text>
        <div style={{ width: 6, height: 6, backgroundColor: color, borderRadius: 3 }} />
      </div>
      <div style={{ flexDirection: "row", alignItems: "end" }}>
        <text style={{ color: C.text, fontSize: 22 }}>{value}</text>
        <text style={{ marginLeft: 8, marginBottom: 3, color: C.faint, fontSize: 9 }}>{detail}</text>
      </div>
      <canvas commands={commands} style={{ width: "100%", height: 50 }} />
    </div>
  )
}

type Process = { id: number; name: string; service: string; memory: number }

const PROCESS_NAMES = ["blendx-render", "hermes-runtime", "asset-worker", "event-loop", "telemetry", "cache-agent"]
const PROCESSES: Process[] = Array.from({ length: 160 }, (_, index) => ({
  id: 8400 + index,
  name: `${PROCESS_NAMES[index % PROCESS_NAMES.length]}-${String(index + 1).padStart(2, "0")}`,
  service: index % 3 === 0 ? "renderer" : index % 3 === 1 ? "runtime" : "platform",
  memory: 38 + (index * 17) % 380,
}))

const ProcessRow = memo(function ProcessRow({ process, tick }: { process: Process; tick: number }) {
  const cpu = ((process.id * 7 + tick * 3) % 970) / 10
  const healthy = process.id % 17 !== 0
  return (
    <div style={{ width: "100%", height: 34, paddingHorizontal: 11, flexDirection: "row", alignItems: "center", backgroundColor: process.id % 2 ? "#111720" : "#0e141c" }}>
      <div style={{ width: 7, height: 7, backgroundColor: healthy ? C.green : C.amber, borderRadius: 3 }} />
      <text style={{ marginLeft: 9, width: 180, color: C.text, fontSize: 10 }}>{process.name}</text>
      <text style={{ width: 95, color: C.faint, fontSize: 10 }}>{process.service}</text>
      <text style={{ width: 76, color: cpu > 80 ? C.red : C.muted, fontSize: 10 }}>{cpu.toFixed(1)}%</text>
      <text style={{ flexGrow: 1, color: C.muted, fontSize: 10 }}>{process.memory} MB</text>
      <badge style={{ paddingHorizontal: 7, paddingVertical: 3, backgroundColor: healthy ? "#173027" : "#33291a", borderRadius: 6 }}>
        <text style={{ color: healthy ? C.green : C.amber, fontSize: 8 }}>{healthy ? "RUNNING" : "WAITING"}</text>
      </badge>
    </div>
  )
})

function Dashboard() {
  const [tick, setTick] = useState(0)
  const [paused, setPaused] = useState(false)

  useEffect(() => {
    if (paused) return
    const timer = setInterval(() => setTick((value) => value + 1), 32)
    return () => clearInterval(timer)
  }, [paused])

  const cpu = 44 + Math.sin(tick * 0.11) * 18
  const memory = 62 + Math.sin(tick * 0.047 + 1.3) * 9
  const latency = 12 + Math.sin(tick * 0.13 + 2.1) * 4
  const requests = 8.2 + Math.sin(tick * 0.073 + 0.7) * 1.6
  const cards = useMemo(() => [
    sparkline(220, 50, tick, 0.2, C.blue),
    sparkline(220, 50, tick, 1.2, C.violet),
    sparkline(220, 50, tick, 2.4, C.green),
    sparkline(220, 50, tick, 3.1, C.amber),
  ], [tick])
  const network = useMemo(() => networkChart(tick), [tick])

  return (
    <div style={{ width: "100%", height: "100%", flexDirection: "row", backgroundColor: C.canvas }}>
      <div style={{ width: 214, height: "100%", flexShrink: 0, padding: 13, gap: 8, backgroundColor: C.sidebar }}>
        <div style={{ height: 50, flexDirection: "row", alignItems: "center", gap: 10 }}>
          <div style={{ width: 32, height: 32, alignItems: "center", justifyContent: "center", backgroundColor: C.blue, borderRadius: 9 }}>
            <text style={{ color: "#ffffff", fontSize: 15 }}>B</text>
          </div>
          <div style={{ gap: 2 }}>
            <text style={{ color: C.text, fontSize: 14 }}>BlendX Ops</text>
            <text style={{ color: C.faint, fontSize: 9 }}>production</text>
          </div>
        </div>
        <separator style={{ width: "100%", height: 1, color: C.border }} />
        <text style={{ marginTop: 7, marginBottom: 2, paddingLeft: 10, color: C.faint, fontSize: 9 }}>WORKSPACE</text>
        <NavItem label="Overview" active />
        <NavItem label="Services" />
        <NavItem label="Deployments" />
        <NavItem label="Traces" />
        <NavItem label="Logs" />
        <text style={{ marginTop: 10, marginBottom: 2, paddingLeft: 10, color: C.faint, fontSize: 9 }}>INFRASTRUCTURE</text>
        <NavItem label="Machines" />
        <NavItem label="Network" />
        <div style={{ flexGrow: 1 }} />
        <div style={{ padding: 11, gap: 8, backgroundColor: C.raised, borderRadius: 9 }}>
          <div style={{ flexDirection: "row", alignItems: "center" }}>
            <text style={{ flexGrow: 1, color: C.muted, fontSize: 9 }}>SYSTEM STATUS</text>
            <div style={{ width: 7, height: 7, backgroundColor: C.green, borderRadius: 3 }} />
          </div>
          <text style={{ color: C.text, fontSize: 11 }}>All systems operational</text>
          <text style={{ color: C.faint, fontSize: 9 }}>Last incident 18 days ago</text>
        </div>
      </div>

      <separator style={{ width: 1, height: "100%", flexShrink: 0, color: C.border }} />

      <div style={{ flexGrow: 1, minWidth: 0, height: "100%" }}>
        <div style={{ height: 64, flexShrink: 0, paddingHorizontal: 20, flexDirection: "row", alignItems: "center", backgroundColor: C.surface }}>
          <div style={{ flexGrow: 1, gap: 3 }}>
            <text style={{ color: C.text, fontSize: 16 }}>System overview</text>
            <text style={{ color: C.muted, fontSize: 10 }}>Live infrastructure metrics · eu-west-3</text>
          </div>
          <badge style={{ paddingHorizontal: 9, paddingVertical: 5, backgroundColor: "#173128", borderRadius: 7 }}>
            <text style={{ color: C.green, fontSize: 9 }}>LIVE</text>
          </badge>
          <button
            onClick={() => setPaused((value) => !value)}
            style={{ marginLeft: 9, height: 31, paddingHorizontal: 12, justifyContent: "center", backgroundColor: C.raised, borderWidth: 1, borderColor: C.border, borderRadius: 8 }}
          >
            <text style={{ color: C.text, fontSize: 10 }}>{paused ? "Resume updates" : "Pause updates"}</text>
          </button>
        </div>
        <separator style={{ width: "100%", height: 1, flexShrink: 0, color: C.border }} />

        <div style={{ flexGrow: 1, minHeight: 0, padding: 16, gap: 14 }}>
          <div style={{ height: 142, flexShrink: 0, flexDirection: "row", gap: 12 }}>
            <MetricCard label="CPU UTILIZATION" value={`${cpu.toFixed(1)}%`} detail="32 cores" commands={cards[0]!} color={C.blue} />
            <MetricCard label="MEMORY" value={`${memory.toFixed(1)}%`} detail="19.8 / 32 GB" commands={cards[1]!} color={C.violet} />
            <MetricCard label="P95 LATENCY" value={`${latency.toFixed(1)} ms`} detail="−8.4%" commands={cards[2]!} color={C.green} />
            <MetricCard label="REQUESTS" value={`${requests.toFixed(1)}k`} detail="per min" commands={cards[3]!} color={C.amber} />
          </div>

          <div style={{ height: 250, flexShrink: 0, flexDirection: "row", gap: 14 }}>
            <div style={{ flexGrow: 1, minWidth: 0, padding: 14, gap: 10, backgroundColor: C.surface, borderWidth: 1, borderColor: C.border, borderRadius: 11 }}>
              <div style={{ flexDirection: "row", alignItems: "center" }}>
                <div style={{ flexGrow: 1, gap: 2 }}>
                  <text style={{ color: C.text, fontSize: 12 }}>Network throughput</text>
                  <text style={{ color: C.faint, fontSize: 9 }}>Last 72 samples</text>
                </div>
                <div style={{ width: 7, height: 7, backgroundColor: C.cyan, borderRadius: 3 }} />
                <text style={{ marginLeft: 5, color: C.muted, fontSize: 9 }}>Ingress</text>
                <div style={{ marginLeft: 12, width: 7, height: 7, backgroundColor: C.violet, borderRadius: 3 }} />
                <text style={{ marginLeft: 5, color: C.muted, fontSize: 9 }}>Egress</text>
              </div>
              <canvas commands={network} style={{ width: "100%", height: 178 }} />
            </div>

            <div style={{ width: 285, height: "100%", flexShrink: 0, padding: 14, gap: 11, backgroundColor: C.surface, borderWidth: 1, borderColor: C.border, borderRadius: 11 }}>
              <text style={{ color: C.text, fontSize: 12 }}>Service health</text>
              {[
                ["Renderer API", 99.99, C.green],
                ["Hermes workers", 98.72, C.blue],
                ["Asset pipeline", 94.18, C.amber],
                ["Event gateway", 99.91, C.cyan],
              ].map(([name, value, color]) => (
                <div key={String(name)} style={{ gap: 5 }}>
                  <div style={{ flexDirection: "row" }}>
                    <text style={{ flexGrow: 1, color: C.muted, fontSize: 9 }}>{String(name)}</text>
                    <text style={{ color: C.text, fontSize: 9 }}>{Number(value).toFixed(2)}%</text>
                  </div>
                  <progress value={Number(value)} max={100} style={{ width: "100%", height: 5, color: color as Color, backgroundColor: C.border }} />
                </div>
              ))}
            </div>
          </div>

          <div style={{ flexGrow: 1, minHeight: 0, backgroundColor: C.surface, borderWidth: 1, borderColor: C.border, borderRadius: 11, overflow: "hidden" }}>
            <div style={{ height: 42, flexShrink: 0, paddingHorizontal: 12, flexDirection: "row", alignItems: "center", backgroundColor: C.raised }}>
              <text style={{ flexGrow: 1, color: C.text, fontSize: 11 }}>Active processes</text>
              <text style={{ color: C.faint, fontSize: 9 }}>160 retained rows · virtualized</text>
            </div>
            <div style={{ height: 28, flexShrink: 0, paddingHorizontal: 11, flexDirection: "row", alignItems: "center", backgroundColor: "#10151d" }}>
              <text style={{ marginLeft: 16, width: 180, color: C.faint, fontSize: 8 }}>PROCESS</text>
              <text style={{ width: 95, color: C.faint, fontSize: 8 }}>SERVICE</text>
              <text style={{ width: 76, color: C.faint, fontSize: 8 }}>CPU</text>
              <text style={{ color: C.faint, fontSize: 8 }}>MEMORY</text>
            </div>
            <virtual-list itemHeight={34} overdraw={3} estimatedItemHeight={34} style={{ width: "100%", flexGrow: 1, minHeight: 0, overflow: "scroll" }}>
              {PROCESSES.map((process) => <ProcessRow key={process.id} process={process} tick={tick} />)}
            </virtual-list>
          </div>
        </div>
      </div>
    </div>
  )
}

const headless = process.argv.includes("--headless")
const mountedAt = performance.now()
const app = render(<Dashboard />, {
  title: "BlendX · Operations Dashboard",
  width: 1280,
  height: 800,
  threads: 4,
  headless,
})
const mountTimeMs = performance.now() - mountedAt

if (headless) {
  setTimeout(() => {
    console.log(JSON.stringify({ mountTimeMs: Number(mountTimeMs.toFixed(2)), processes: PROCESSES.length, ...app.renderer.getStats() }))
    app.stop()
  }, 3200)
}

process.on("SIGINT", () => app.stop())
