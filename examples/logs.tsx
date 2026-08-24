/**
 * Live log and trace explorer for BlendX.
 *
 * Exercises large retained lists, virtualization, frequent insertion,
 * filtering, text input, badges, selection, and a detailed trace panel.
 */

import React from "react"
import { render } from "../src/index.js"

const { useEffect, useMemo, useState } = React

const C = {
  canvas: "#090c11" as const,
  sidebar: "#0d1118" as const,
  surface: "#121821" as const,
  raised: "#19212c" as const,
  border: "#283342" as const,
  text: "#e4eaf2" as const,
  muted: "#8e9aaa" as const,
  faint: "#5f6c7e" as const,
  blue: "#62a6ff" as const,
  cyan: "#4dd4dc" as const,
  green: "#4bd39d" as const,
  amber: "#f0bb63" as const,
  red: "#ef7882" as const,
  violet: "#9a87f5" as const,
}

type Level = "INFO" | "WARN" | "ERROR" | "DEBUG"
type LogEntry = {
  id: number
  time: string
  level: Level
  service: string
  message: string
  duration: number
  trace: string
  host: string
}

const SERVICES = ["renderer-api", "hermes-worker", "asset-cache", "event-gateway", "scheduler"]
const MESSAGES = [
  "frame committed with merged damage regions",
  "request completed successfully",
  "bytecode module loaded from embedded bundle",
  "asset decoded and inserted into image cache",
  "worker returned to the shared thread pool",
  "retrying upstream request after timeout",
  "layout invalidation propagated to parent",
  "slow paint detected for intersecting nodes",
  "connection accepted from local transport",
  "telemetry batch exported",
]

function levelFor(index: number): Level {
  if (index % 37 === 0) return "ERROR"
  if (index % 13 === 0) return "WARN"
  if (index % 5 === 0) return "DEBUG"
  return "INFO"
}

function makeEntry(id: number): LogEntry {
  const seconds = id % 60
  const millis = (id * 137) % 1000
  return {
    id,
    time: `14:${String((id * 3) % 60).padStart(2, "0")}:${String(seconds).padStart(2, "0")}.${String(millis).padStart(3, "0")}`,
    level: levelFor(id),
    service: SERVICES[id % SERVICES.length]!,
    message: MESSAGES[(id * 7) % MESSAGES.length]!,
    duration: 2 + (id * 29) % 184,
    trace: `${(id * 2654435761).toString(16).padStart(8, "0").slice(-8)}${id.toString(16).padStart(8, "0")}`,
    host: `node-${String((id % 24) + 1).padStart(2, "0")}.eu-west`,
  }
}

const INITIAL_ENTRIES = Array.from({ length: 2_500 }, (_, index) => makeEntry(12_500 - index))

const LEVEL_COLORS: Record<Level, { text: `#${string}`; background: `#${string}` }> = {
  INFO: { text: C.blue, background: "#17283d" },
  WARN: { text: C.amber, background: "#332a1d" },
  ERROR: { text: C.red, background: "#351e24" },
  DEBUG: { text: C.violet, background: "#27223b" },
}

function LevelBadge({ level }: { level: Level }) {
  const colors = LEVEL_COLORS[level]
  return (
    <badge style={{ width: 49, paddingVertical: 3, alignItems: "center", backgroundColor: colors.background, borderRadius: 6 }}>
      <text style={{ color: colors.text, fontSize: 8 }}>{level}</text>
    </badge>
  )
}

function FilterButton({ label, count, active, color, onClick }: {
  label: string
  count: number
  active: boolean
  color: `#${string}`
  onClick: () => void
}) {
  return (
    <button
      onClick={onClick}
      style={{
        height: 36,
        paddingHorizontal: 9,
        flexDirection: "row",
        alignItems: "center",
        gap: 8,
        backgroundColor: active ? "#1a2635" : C.sidebar,
        borderRadius: 7,
      }}
    >
      <div style={{ width: 7, height: 7, backgroundColor: color, borderRadius: 3 }} />
      <text style={{ flexGrow: 1, color: active ? C.text : C.muted, fontSize: 10 }}>{label}</text>
      <text style={{ color: C.faint, fontSize: 9 }}>{count}</text>
    </button>
  )
}

function LogRow({ entry, selected, onSelect }: { entry: LogEntry; selected: boolean; onSelect: () => void }) {
  return (
    <button
      onClick={onSelect}
      style={{
        width: "100%",
        height: 34,
        paddingHorizontal: 9,
        flexDirection: "row",
        alignItems: "center",
        backgroundColor: selected ? "#1a293b" : entry.id % 2 ? "#0d1219" : "#10151d",
        borderWidth: selected ? 1 : 0,
        borderColor: selected ? "#31547b" : C.canvas,
      }}
    >
      <text style={{ width: 88, color: C.faint, fontSize: 9 }}>{entry.time}</text>
      <LevelBadge level={entry.level} />
      <text style={{ marginLeft: 9, width: 114, color: C.cyan, fontSize: 9 }}>{entry.service}</text>
      <text style={{ flexGrow: 1, color: entry.level === "ERROR" ? "#f2b1b7" : C.text, fontSize: 10, textOverflow: "ellipsis", whiteSpace: "nowrap" }}>{entry.message}</text>
      <text style={{ width: 55, color: entry.duration > 120 ? C.amber : C.muted, fontSize: 9 }}>{entry.duration} ms</text>
    </button>
  )
}

function DetailRow({ label, value, color = C.text }: { label: string; value: string; color?: `#${string}` }) {
  return (
    <div style={{ gap: 4 }}>
      <text style={{ color: C.faint, fontSize: 8 }}>{label}</text>
      <text style={{ color, fontSize: 10 }}>{value}</text>
    </div>
  )
}

function LogsApp() {
  const [entries, setEntries] = useState(INITIAL_ENTRIES)
  const [paused, setPaused] = useState(false)
  const [level, setLevel] = useState<Level | "ALL">("ALL")
  const [service, setService] = useState("ALL")
  const [query, setQuery] = useState("")
  const [selectedId, setSelectedId] = useState(INITIAL_ENTRIES[0]!.id)

  useEffect(() => {
    if (paused) return
    const timer = setInterval(() => {
      setEntries((current) => {
        const newest = makeEntry(current[0]!.id + 1)
        return [newest, ...current.slice(0, 2_999)]
      })
    }, 160)
    return () => clearInterval(timer)
  }, [paused])

  const counts = useMemo(() => {
    const result: Record<Level, number> = { INFO: 0, WARN: 0, ERROR: 0, DEBUG: 0 }
    for (const entry of entries) result[entry.level] += 1
    return result
  }, [entries])

  const visible = useMemo(() => {
    const needle = query.trim().toLowerCase()
    return entries.filter((entry) =>
      (level === "ALL" || entry.level === level)
      && (service === "ALL" || entry.service === service)
      && (!needle || entry.message.toLowerCase().includes(needle) || entry.trace.includes(needle)),
    )
  }, [entries, level, service, query])

  const selected = entries.find((entry) => entry.id === selectedId) ?? visible[0] ?? entries[0]!
  const errorRate = (counts.ERROR / entries.length) * 100

  return (
    <div style={{ width: "100%", height: "100%", backgroundColor: C.canvas }}>
      <div style={{ height: 62, flexShrink: 0, paddingHorizontal: 16, flexDirection: "row", alignItems: "center", gap: 11, backgroundColor: C.surface }}>
        <div style={{ width: 34, height: 34, alignItems: "center", justifyContent: "center", backgroundColor: "#243452", borderRadius: 9 }}>
          <text style={{ color: C.blue, fontSize: 14 }}>⌁</text>
        </div>
        <div style={{ flexGrow: 1, gap: 2 }}>
          <text style={{ color: C.text, fontSize: 15 }}>Live logs</text>
          <text style={{ color: C.muted, fontSize: 9 }}>production · eu-west-3 · newest events first</text>
        </div>
        <div style={{ width: 280, height: 32, paddingHorizontal: 10, flexDirection: "row", alignItems: "center", backgroundColor: C.raised, borderWidth: 1, borderColor: C.border, borderRadius: 8 }}>
          <text style={{ color: C.faint, fontSize: 11 }}>⌕</text>
          <input
            value={query}
            placeholder="Search message or trace…"
            onChange={(event) => setQuery(event.value ?? "")}
            style={{ flexGrow: 1, height: 28, marginLeft: 7, color: C.text, backgroundColor: C.raised, fontSize: 10 }}
          />
        </div>
        <badge style={{ paddingHorizontal: 9, paddingVertical: 5, backgroundColor: paused ? "#332a1d" : "#173128", borderRadius: 7 }}>
          <text style={{ color: paused ? C.amber : C.green, fontSize: 8 }}>{paused ? "PAUSED" : "LIVE"}</text>
        </badge>
        <button
          onClick={() => setPaused((value) => !value)}
          style={{ height: 32, paddingHorizontal: 12, justifyContent: "center", backgroundColor: C.raised, borderWidth: 1, borderColor: C.border, borderRadius: 8 }}
        >
          <text style={{ color: C.text, fontSize: 10 }}>{paused ? "Resume stream" : "Pause stream"}</text>
        </button>
      </div>
      <separator style={{ width: "100%", height: 1, flexShrink: 0, color: C.border }} />

      <div style={{ flexGrow: 1, minHeight: 0, flexDirection: "row" }}>
        <div style={{ width: 208, height: "100%", flexShrink: 0, padding: 12, gap: 5, backgroundColor: C.sidebar }}>
          <text style={{ paddingHorizontal: 8, paddingVertical: 7, color: C.faint, fontSize: 8 }}>LEVEL</text>
          <FilterButton label="All events" count={entries.length} active={level === "ALL"} color={C.blue} onClick={() => setLevel("ALL")} />
          <FilterButton label="Info" count={counts.INFO} active={level === "INFO"} color={C.blue} onClick={() => setLevel("INFO")} />
          <FilterButton label="Warnings" count={counts.WARN} active={level === "WARN"} color={C.amber} onClick={() => setLevel("WARN")} />
          <FilterButton label="Errors" count={counts.ERROR} active={level === "ERROR"} color={C.red} onClick={() => setLevel("ERROR")} />
          <FilterButton label="Debug" count={counts.DEBUG} active={level === "DEBUG"} color={C.violet} onClick={() => setLevel("DEBUG")} />
          <text style={{ marginTop: 10, paddingHorizontal: 8, paddingVertical: 7, color: C.faint, fontSize: 8 }}>SERVICES</text>
          <FilterButton label="All services" count={entries.length} active={service === "ALL"} color={C.green} onClick={() => setService("ALL")} />
          {SERVICES.map((name, index) => (
            <FilterButton key={name} label={name} count={500} active={service === name} color={index % 2 ? C.cyan : C.green} onClick={() => setService(name)} />
          ))}
          <div style={{ flexGrow: 1 }} />
          <div style={{ padding: 10, gap: 7, backgroundColor: C.raised, borderRadius: 8 }}>
            <div style={{ flexDirection: "row" }}>
              <text style={{ flexGrow: 1, color: C.muted, fontSize: 8 }}>ERROR RATE</text>
              <text style={{ color: errorRate > 4 ? C.red : C.green, fontSize: 9 }}>{errorRate.toFixed(2)}%</text>
            </div>
            <progress value={errorRate} max={10} style={{ width: "100%", height: 5, color: errorRate > 4 ? C.red : C.green, backgroundColor: C.border }} />
            <text style={{ color: C.faint, fontSize: 8 }}>SLO target below 4%</text>
          </div>
        </div>

        <separator style={{ width: 1, height: "100%", flexShrink: 0, color: C.border }} />

        <div style={{ flexGrow: 1, minWidth: 0, height: "100%" }}>
          <div style={{ height: 40, flexShrink: 0, paddingHorizontal: 10, flexDirection: "row", alignItems: "center", backgroundColor: C.raised }}>
            <text style={{ width: 88, color: C.faint, fontSize: 8 }}>TIMESTAMP</text>
            <text style={{ width: 58, color: C.faint, fontSize: 8 }}>LEVEL</text>
            <text style={{ width: 123, color: C.faint, fontSize: 8 }}>SERVICE</text>
            <text style={{ flexGrow: 1, color: C.faint, fontSize: 8 }}>MESSAGE</text>
            <text style={{ width: 55, color: C.faint, fontSize: 8 }}>DURATION</text>
          </div>
          <virtual-list itemHeight={34} overdraw={5} estimatedItemHeight={34} style={{ width: "100%", flexGrow: 1, minHeight: 0, overflow: "scroll", backgroundColor: C.canvas }}>
            {visible.map((entry) => (
              <LogRow key={entry.id} entry={entry} selected={entry.id === selected.id} onSelect={() => setSelectedId(entry.id)} />
            ))}
          </virtual-list>
          <div style={{ height: 30, flexShrink: 0, paddingHorizontal: 10, flexDirection: "row", alignItems: "center", backgroundColor: C.surface }}>
            <div style={{ width: 6, height: 6, backgroundColor: paused ? C.amber : C.green, borderRadius: 3 }} />
            <text style={{ marginLeft: 7, color: C.muted, fontSize: 8 }}>{visible.length.toLocaleString()} matching events</text>
            <div style={{ flexGrow: 1 }} />
            <text style={{ color: C.faint, fontSize: 8 }}>160 ms ingest interval · virtualized native rows</text>
          </div>
        </div>

        <separator style={{ width: 1, height: "100%", flexShrink: 0, color: C.border }} />

        <div style={{ width: 304, height: "100%", flexShrink: 0, padding: 14, gap: 14, backgroundColor: C.sidebar }}>
          <div style={{ flexDirection: "row", alignItems: "center" }}>
            <text style={{ flexGrow: 1, color: C.text, fontSize: 12 }}>Event detail</text>
            <LevelBadge level={selected.level} />
          </div>
          <separator style={{ width: "100%", height: 1, color: C.border }} />
          <DetailRow label="MESSAGE" value={selected.message} />
          <DetailRow label="TRACE ID" value={selected.trace} color={C.blue} />
          <div style={{ flexDirection: "row", gap: 18 }}>
            <div style={{ flexGrow: 1 }}><DetailRow label="DURATION" value={`${selected.duration} ms`} color={selected.duration > 120 ? C.amber : C.green} /></div>
            <div style={{ flexGrow: 1 }}><DetailRow label="EVENT ID" value={String(selected.id)} /></div>
          </div>
          <DetailRow label="SERVICE" value={selected.service} color={C.cyan} />
          <DetailRow label="HOST" value={selected.host} />
          <text style={{ marginTop: 5, color: C.faint, fontSize: 8 }}>TRACE SPANS</text>
          {[
            ["gateway.receive", 4],
            ["runtime.evaluate", Math.max(3, selected.duration - 31)],
            ["renderer.commit", 17],
            ["blend2d.paint", 8],
          ].map(([name, duration], index) => (
            <div key={String(name)} style={{ padding: 9, gap: 6, backgroundColor: C.raised, borderRadius: 7 }}>
              <div style={{ flexDirection: "row" }}>
                <text style={{ flexGrow: 1, color: C.text, fontSize: 9 }}>{String(name)}</text>
                <text style={{ color: C.muted, fontSize: 8 }}>{Number(duration)} ms</text>
              </div>
              <progress value={Number(duration)} max={Math.max(selected.duration, 1)} style={{ width: "100%", height: 4, color: index === 1 ? C.violet : C.blue, backgroundColor: C.border }} />
            </div>
          ))}
          <div style={{ flexGrow: 1 }} />
          <button style={{ height: 34, alignItems: "center", justifyContent: "center", backgroundColor: C.raised, borderWidth: 1, borderColor: C.border, borderRadius: 8 }}>
            <text style={{ color: C.text, fontSize: 10 }}>Copy trace identifier</text>
          </button>
        </div>
      </div>
    </div>
  )
}

const headless = process.argv.includes("--headless")
const mountedAt = performance.now()
const app = render(<LogsApp />, {
  title: "BlendX · Live Log Explorer",
  width: 1280,
  height: 800,
  threads: 4,
  headless,
})
const mountTimeMs = performance.now() - mountedAt

if (headless) {
  setTimeout(() => {
    console.log(JSON.stringify({ mountTimeMs: Number(mountTimeMs.toFixed(2)), initialEntries: INITIAL_ENTRIES.length, ...app.renderer.getStats() }))
    app.stop()
  }, 3200)
}

process.on("SIGINT", () => app.stop())
