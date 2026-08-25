import React from "react"
import { batchUpdates, render } from "../src/index.js"
import type { BlendxElement, CanvasCommand, Color, NativeStats } from "../src/index.js"

const { memo, useCallback, useEffect, useMemo, useRef, useState, useSyncExternalStore } = React

type Profile = "sparse" | "dense" | "layout" | "paint" | "churn" | "scroll" | "native"

const C = {
  canvas: "#070b11" as const,
  surface: "#111824" as const,
  raised: "#1a2433" as const,
  border: "#2a394d" as const,
  text: "#eef3fb" as const,
  muted: "#8d9aad" as const,
  faint: "#5d6b7f" as const,
  violet: "#7868f7" as const,
  cyan: "#43c7db" as const,
  green: "#47d29a" as const,
  amber: "#efb45b" as const,
  red: "#ed7480" as const,
}

const COLORS: Color[] = [C.violet, C.cyan, C.green, C.amber, C.red]
const PROFILES: Array<{ value: Profile; label: string; detail: string }> = [
  { value: "sparse", label: "Sparse", detail: "Change a deterministic subset" },
  { value: "dense", label: "Dense", detail: "Change every retained tile" },
  { value: "layout", label: "Layout", detail: "Change geometry and content" },
  { value: "paint", label: "Paint", detail: "Charts, progress and colors" },
  { value: "churn", label: "Churn", detail: "Mount and remove 20 percent" },
  { value: "scroll", label: "Scroll", detail: "Move the retained grid" },
  { value: "native", label: "Native", detail: "Animate progress without React commits" },
]
const SCALES = [250, 1_000, 2_500, 5_000]
const RATES = [1, 5, 25, 100]
const FPS = [30, 60, 120]
const COLUMNS = 8
const ROW_HEIGHT = 112

function argument(name: string): string | undefined {
  return process.argv.find((value) => value.startsWith(`--${name}=`))?.slice(name.length + 3)
}

const headless = process.argv.includes("--headless")
const initialProfile = (PROFILES.some(({ value }) => value === argument("profile")) ? argument("profile") : "sparse") as Profile
const initialScale = Math.max(1, Math.min(10_000, Number(argument("components")) || (headless ? 1_000 : 250)))
const initialRate = Math.max(1, Math.min(100, Number(argument("update")) || 5))
const initialFps = Math.max(1, Math.min(240, Number(argument("fps")) || 60))
const screenshot = argument("screenshot")
const benchmarkDuration = Math.max(1_000, Number(argument("duration")) || 4_200)

function chart(index: number, phase: number, color: Color): CanvasCommand[] {
  const commands: CanvasCommand[] = [{ kind: "fillRect", x: 0, y: 0, width: 120, height: 20, color: "#0e151f", radius: 4 }]
  for (let point = 1; point < 7; point += 1) {
    const previous = 4 + ((index * 7 + phase * 5 + (point - 1) * 11) % 13)
    const current = 4 + ((index * 7 + phase * 5 + point * 11) % 13)
    commands.push({ kind: "line", x1: (point - 1) * 20, y1: previous, x2: point * 20, y2: current, color, widthPx: 1.5 })
  }
  return commands
}

class VersionStore {
  private versions: number[] = []
  private listeners = new Map<number, Set<() => void>>()

  resize(size: number): void {
    this.versions = Array.from({ length: size }, (_, index) => this.versions[index] ?? 0)
  }

  value(index: number): number { return this.versions[index] ?? 0 }

  subscribe(index: number, listener: () => void): () => void {
    let listeners = this.listeners.get(index)
    if (!listeners) this.listeners.set(index, listeners = new Set())
    listeners.add(listener)
    return () => {
      listeners?.delete(listener)
      if (listeners?.size === 0) this.listeners.delete(index)
    }
  }

  advance(indices: number[]): void {
    for (const index of indices) this.versions[index] = (this.versions[index] ?? 0) + 1
    batchUpdates(() => {
      for (const index of indices) {
        for (const listener of this.listeners.get(index) ?? []) listener()
      }
    })
  }
}

const versionStore = new VersionStore()
versionStore.resize(initialScale)

const MetricTile = memo(function MetricTile({ index, profile }: { index: number; profile: Profile }) {
  const subscribe = useCallback((listener: () => void) => versionStore.subscribe(index, listener), [index])
  const snapshot = useCallback(() => versionStore.value(index), [index])
  const phase = useSyncExternalStore(subscribe, snapshot, snapshot)
  const paintPhase = profile === "paint" || profile === "dense" ? phase : 0
  const color = COLORS[(index + paintPhase) % COLORS.length]!
  const value = (20 + ((index * 17 + phase * 13) % 800)) / 10
  const layoutPressure = profile === "layout"
  const commands = useMemo(() => chart(index, paintPhase, color), [color, index, paintPhase])
  return (
    <div
      style={{
        width: 0,
        flexGrow: 1,
        minWidth: 0,
        height: layoutPressure ? 96 + (phase % 3) * 3 : 102,
        padding: layoutPressure ? 7 + (phase % 3) : 8,
        gap: layoutPressure ? 3 + (phase % 2) : 4,
        backgroundColor: phase % 2 ? "#182435" : C.surface,
        borderWidth: 1,
        borderColor: phase % 5 === 0 ? color : C.border,
        borderRadius: 9,
        layoutContain: true,
      }}
    >
      <div style={{ height: 15, flexShrink: 0, flexDirection: "row", alignItems: "center" }}>
        <text style={{ flexGrow: 1, color: C.muted, fontSize: 7 }}>worker-{String(index).padStart(4, "0")}</text>
        <badge style={{ paddingHorizontal: 5, paddingVertical: 2, backgroundColor: `${color}24` as Color, borderRadius: 4 }}><text style={{ color, fontSize: 6 }}>{phase % 7 === 0 ? "HOT" : "LIVE"}</text></badge>
      </div>
      <div style={{ height: 22, flexShrink: 0, flexDirection: "row", alignItems: "end" }}>
        <text style={{ color: C.text, fontSize: 15 }}>{value.toFixed(1)}</text>
        <text style={{ marginLeft: 4, marginBottom: 2, color: C.faint, fontSize: 6 }}>ms</text>
      </div>
      <progress
        value={value}
        max={100}
        animateValue={profile === "native" ? 90 - (index % 70) : undefined}
        animationDurationMs={profile === "native" ? 700 + (index % 11) * 70 : undefined}
        animationLoop={profile === "native" ? true : undefined}
        animationAlternate={profile === "native" ? true : undefined}
        style={{ width: "100%", height: 4, flexShrink: 0, color, backgroundColor: "#273245", borderRadius: 2 }}
      />
      <canvas commands={commands} style={{ width: "100%", height: 20, flexShrink: 0 }} />
    </div>
  )
})

function ChoiceButton({ active, label, detail, onClick }: { active: boolean; label: string; detail?: string; onClick: () => void }) {
  return (
    <button
      onClick={onClick}
      style={{
        width: "100%", height: detail ? 40 : 32, paddingHorizontal: 9, gap: 2, justifyContent: "center",
        backgroundColor: active ? "#292653" : C.raised,
        borderWidth: 1, borderColor: active ? "#6258bd" : C.border, borderRadius: 7,
        hover: { backgroundColor: active ? "#312d62" : "#223047" },
      }}
    >
      <text style={{ color: active ? "#ffffff" : C.text, fontSize: 8 }}>{label}</text>
      {detail ? <text style={{ color: C.faint, fontSize: 6 }}>{detail}</text> : null}
    </button>
  )
}

function Stat({ label, value, color = C.text }: { label: string; value: string; color?: Color }) {
  return <div style={{ height: 25, flexDirection: "row", alignItems: "center" }}><text style={{ flexGrow: 1, color: C.muted, fontSize: 7 }}>{label}</text><text style={{ color, fontSize: 8 }}>{value}</text></div>
}

let app: ReturnType<typeof render> | undefined
let benchmarkState = { profile: initialProfile, scale: initialScale, rate: initialRate, fps: initialFps, liveCount: initialScale, actualFps: 0 }

function VisualStressApp() {
  const [profile, setProfile] = useState<Profile>(initialProfile)
  const [scale, setScale] = useState(initialScale)
  const [rate, setRate] = useState(initialRate)
  const [targetFps, setTargetFps] = useState(initialFps)
  const [paused, setPaused] = useState(false)
  const [liveCount, setLiveCount] = useState(initialScale)
  const [stats, setStats] = useState<NativeStats | null>(null)
  const [actualFps, setActualFps] = useState(0)
  const cursor = useRef(0)
  const churnHigh = useRef(true)
  const grid = useRef<BlendxElement | null>(null)
  const scrollOffset = useRef(0)

  useEffect(() => {
    versionStore.resize(scale)
    setLiveCount(scale)
    scrollOffset.current = 0
  }, [scale])

  useEffect(() => {
    const effectiveRate = profile === "dense" ? 100 : profile === "churn" ? 20 : rate
    benchmarkState = { profile, scale, rate: effectiveRate, fps: targetFps, liveCount, actualFps: benchmarkState.actualFps }
  }, [liveCount, profile, rate, scale, targetFps])

  useEffect(() => {
    if (paused) return
    if (profile === "native") return
    const interval = profile === "churn" ? 180 : Math.max(4, Math.round(1000 / targetFps))
    const timer = setInterval(() => {
      if (profile === "scroll") {
        const maximum = Math.max(0, Math.ceil(scale / COLUMNS) * ROW_HEIGHT - 620)
        scrollOffset.current = maximum > 0 ? (scrollOffset.current + 34) % maximum : 0
        if (grid.current) app?.renderer.scrollToOffset(grid.current.id, scrollOffset.current)
        return
      }
      if (profile === "churn") {
        churnHigh.current = !churnHigh.current
        setLiveCount(churnHigh.current ? scale : Math.max(1, Math.floor(scale * 0.8)))
        return
      }
      const count = profile === "dense" ? scale : Math.max(1, Math.ceil(scale * rate / 100))
      const changed: number[] = []
      for (let offset = 0; offset < count; offset += 1) {
        changed.push(profile === "dense" ? offset : (cursor.current + offset * 37) % scale)
      }
      cursor.current = (cursor.current + count * 13 + 1) % scale
      versionStore.advance(changed)
    }, interval)
    return () => clearInterval(timer)
  }, [paused, profile, rate, scale, targetFps])

  useEffect(() => {
    let previousFrames = app?.renderer.getStats().frameCount ?? 0
    let previousTime = performance.now()
    const timer = setInterval(() => {
      const next = app?.renderer.getStats()
      if (!next) return
      const now = performance.now()
      const measuredFps = (next.frameCount - previousFrames) * 1000 / Math.max(1, now - previousTime)
      benchmarkState = { ...benchmarkState, actualFps: Number(measuredFps.toFixed(1)) }
      setActualFps(measuredFps)
      previousFrames = next.frameCount
      previousTime = now
      setStats(next)
    }, 300)
    return () => clearInterval(timer)
  }, [])

  const rows = useMemo(() => {
    const result: React.ReactNode[] = []
    for (let start = 0; start < liveCount; start += COLUMNS) {
      const cells: React.ReactNode[] = []
      for (let column = 0; column < COLUMNS; column += 1) {
        const index = start + column
        if (index >= liveCount) cells.push(<div key={`empty-${column}`} style={{ width: 0, flexGrow: 1, minWidth: 0 }} />)
        else cells.push(<MetricTile key={index} index={index} profile={profile} />)
      }
      result.push(<div key={start} style={{ width: "100%", height: ROW_HEIGHT, flexShrink: 0, flexDirection: "row", gap: 6 }}>{cells}</div>)
    }
    return result
  }, [liveCount, profile])

  const p95 = stats?.frameP95Ms ?? 0
  const effectiveRate = profile === "dense" ? 100 : profile === "churn" ? 20 : rate
  const budgetColor = p95 <= 8 ? C.green : p95 <= 16.67 ? C.amber : C.red
  return (
    <div style={{ width: "100%", height: "100%", padding: 18, gap: 13, backgroundColor: C.canvas }}>
      <div style={{ height: 54, flexShrink: 0, flexDirection: "row", alignItems: "center", gap: 12 }}>
        <div style={{ width: 40, height: 40, alignItems: "center", justifyContent: "center", backgroundColor: C.violet, borderRadius: 11 }}><text style={{ color: "#ffffff", fontSize: 12 }}>VB</text></div>
        <div style={{ flexGrow: 1, gap: 3 }}><text style={{ color: C.text, fontSize: 18 }}>Visual component benchmark</text><text style={{ color: C.muted, fontSize: 9 }}>{liveCount.toLocaleString()} metric widgets · deterministic {profile} workload · target {targetFps} FPS</text></div>
        <badge style={{ paddingHorizontal: 11, paddingVertical: 6, backgroundColor: `${budgetColor}20` as Color, borderRadius: 8 }}><text style={{ color: budgetColor, fontSize: 8 }}>{paused ? "PAUSED" : p95 <= 16.67 ? "IN FRAME BUDGET" : "OVER FRAME BUDGET"}</text></badge>
      </div>

      <div style={{ flexGrow: 1, minHeight: 0, flexDirection: "row", gap: 13 }}>
        <div style={{ width: 0, flexGrow: 1, minWidth: 0, padding: 10, gap: 8, backgroundColor: C.surface, borderWidth: 1, borderColor: C.border, borderRadius: 12 }}>
          <div style={{ height: 34, flexShrink: 0, flexDirection: "row", alignItems: "center" }}>
            <text style={{ flexGrow: 1, color: C.text, fontSize: 11 }}>Retained component field</text>
            <text style={{ color: C.faint, fontSize: 8 }}>Scroll to inspect offscreen retained widgets</text>
          </div>
          <div ref={grid} style={{ width: "100%", flexGrow: 1, minHeight: 0, padding: 6, gap: 0, overflow: "scroll", backgroundColor: "#0b111a", borderRadius: 8 }}>{rows}</div>
        </div>

        <div style={{ width: 326, flexShrink: 0, gap: 10 }}>
          <div style={{ padding: 12, gap: 7, backgroundColor: C.surface, borderWidth: 1, borderColor: C.border, borderRadius: 11 }}>
            <div style={{ flexDirection: "row", alignItems: "center" }}><text style={{ flexGrow: 1, color: C.text, fontSize: 10 }}>Frame telemetry</text><text style={{ color: budgetColor, fontSize: 12 }}>{actualFps.toFixed(0)} FPS</text></div>
            <Stat label="Frame p50 / p95" value={`${(stats?.frameP50Ms ?? 0).toFixed(2)} / ${p95.toFixed(2)} ms`} color={budgetColor} />
            <Stat label="Frame p99 / maximum" value={`${(stats?.frameP99Ms ?? 0).toFixed(2)} / ${(stats?.frameMaxMs ?? 0).toFixed(2)} ms`} />
            <Stat label="Batch decode / Yoga" value={`${(stats?.batchTimeMs ?? 0).toFixed(2)} / ${(stats?.yogaTimeMs ?? 0).toFixed(2)} ms`} />
            <Stat label="React commit / bridge" value={`${(stats?.reactCommitTimeMs ?? 0).toFixed(2)} / ${(stats?.bridgeTimeMs ?? 0).toFixed(2)} ms`} />
            <Stat label="Box sync / special layout" value={`${(stats?.boxSyncTimeMs ?? 0).toFixed(2)} / ${(stats?.specialLayoutTimeMs ?? 0).toFixed(2)} ms`} />
            <Stat label="Paint / presentation" value={`${(stats?.paintTimeMs ?? 0).toFixed(2)} / ${(stats?.presentTimeMs ?? 0).toFixed(2)} ms`} />
            <Stat label="Native nodes" value={(stats?.nodeCount ?? 0).toLocaleString()} />
            <Stat label="Mutations last commit" value={(stats?.mutationsLastCommit ?? 0).toLocaleString()} />
            <Stat label="Painted visits / retained" value={`${(stats?.paintedNodes ?? 0).toLocaleString()} / ${(stats?.nodeCount ?? 0).toLocaleString()}`} />
            <Stat label="Dirty rectangles" value={String(stats?.dirtyRectCount ?? 0)} />
            <Stat label="Over 16.67 ms (rolling)" value={String(stats?.framesOverBudget ?? 0)} color={(stats?.framesOverBudget ?? 0) ? C.red : C.green} />
            <Stat label="Over 8.33 ms / animations" value={`${stats?.framesOver120Budget ?? 0} / ${stats?.activeAnimations ?? 0}`} color={(stats?.framesOver120Budget ?? 0) ? C.red : C.green} />
          </div>

          <div style={{ padding: 11, gap: 6, backgroundColor: C.surface, borderWidth: 1, borderColor: C.border, borderRadius: 11 }}>
            <text style={{ color: C.violet, fontSize: 7 }}>WORKLOAD PROFILE</text>
            <div style={{ flexDirection: "row", gap: 6 }}>
              <div style={{ width: 0, flexGrow: 1, gap: 5 }}>{PROFILES.slice(0, 3).map((item) => <ChoiceButton key={item.value} active={profile === item.value} label={item.label} detail={item.detail} onClick={() => setProfile(item.value)} />)}</div>
              <div style={{ width: 0, flexGrow: 1, gap: 5 }}>{PROFILES.slice(3).map((item) => <ChoiceButton key={item.value} active={profile === item.value} label={item.label} detail={item.detail} onClick={() => setProfile(item.value)} />)}</div>
            </div>
          </div>

          <div style={{ padding: 11, gap: 7, backgroundColor: C.surface, borderWidth: 1, borderColor: C.border, borderRadius: 11 }}>
            <text style={{ color: C.violet, fontSize: 7 }}>RETAINED WIDGETS</text>
            <div style={{ flexDirection: "row", gap: 5 }}>{SCALES.map((value) => <div key={value} style={{ width: 0, flexGrow: 1 }}><ChoiceButton active={scale === value} label={value.toLocaleString()} onClick={() => setScale(value)} /></div>)}</div>
            <text style={{ marginTop: 3, color: C.violet, fontSize: 7 }}>UPDATE SHARE · EFFECTIVE {effectiveRate}%</text>
            <div style={{ flexDirection: "row", gap: 5 }}>{RATES.map((value) => <div key={value} style={{ width: 0, flexGrow: 1 }}><ChoiceButton active={rate === value} label={`${value}%`} onClick={() => setRate(value)} /></div>)}</div>
            <text style={{ marginTop: 3, color: C.violet, fontSize: 7 }}>TARGET RATE</text>
            <div style={{ flexDirection: "row", gap: 5 }}>{FPS.map((value) => <div key={value} style={{ width: 0, flexGrow: 1 }}><ChoiceButton active={targetFps === value} label={`${value}`} onClick={() => setTargetFps(value)} /></div>)}<div style={{ width: 0, flexGrow: 1 }}><ChoiceButton active={paused} label={paused ? "Run" : "Pause"} onClick={() => setPaused((value) => !value)} /></div></div>
          </div>

          <div style={{ flexGrow: 1, minHeight: 0, padding: 11, gap: 6, backgroundColor: "#101925", borderWidth: 1, borderColor: C.border, borderRadius: 11 }}>
            <text style={{ color: C.text, fontSize: 9 }}>Reading the result</text>
            <text style={{ color: C.muted, fontSize: 7, lineHeight: 12, whiteSpace: "normal" }}>The 16.67 ms line is the 60 FPS native renderer budget. Headless results exclude the desktop compositor and React/Hermes time outside native frame rendering. Compare identical profile, scale, update share and target rate between commits.</text>
          </div>
        </div>
      </div>
    </div>
  )
}

const mountedAt = performance.now()
app = render(<VisualStressApp />, { title: "BlendX · Visual Component Benchmark", width: 1400, height: 840, threads: 4, headless })
const mountTimeMs = performance.now() - mountedAt

if (headless) setTimeout(() => {
  const stats = app?.renderer.getStats()
  if (screenshot) app?.renderer.captureScreenshot(screenshot)
  console.log(JSON.stringify({ mountTimeMs: Number(mountTimeMs.toFixed(2)), ...benchmarkState, ...stats }))
  app?.stop()
}, benchmarkDuration)

process.on("SIGINT", () => app?.stop())
