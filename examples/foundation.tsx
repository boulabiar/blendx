import React from "react"
import {
  Dialog,
  DialogClose,
  DialogContent,
  DialogTrigger,
  ToastProvider,
  ToastViewport,
  VirtualList,
  motion,
  render,
  useToast,
} from "../src/index.js"
import type { Color, VirtualListHandle } from "../src/index.js"

const { useEffect, useMemo, useRef, useState } = React
const C = {
  canvas: "#080d14" as const,
  surface: "#111925" as const,
  raised: "#1a2535" as const,
  border: "#2c3b50" as const,
  text: "#eef3fb" as const,
  muted: "#8e9bae" as const,
  faint: "#5d6c81" as const,
  violet: "#7868f7" as const,
  cyan: "#45c8dc" as const,
  green: "#48d39b" as const,
  amber: "#f0b45c" as const,
}

const screenshotDialog = process.argv.includes("--open-dialog")
const screenshotToast = process.argv.includes("--show-toast")

type Activity = { id: number; title: string; detail: string; height: number; color: Color }
const INITIAL_ACTIVITY: Activity[] = Array.from({ length: 600 }, (_, index) => ({
  id: index,
  title: index % 7 === 0 ? `Frame ${index} exceeded its soft budget` : `Renderer commit ${index}`,
  detail: index % 5 === 0
    ? "Layout, paint and presentation timing were sampled for this longer diagnostic event."
    : `${8 + index % 31} mutations · ${42 + index % 90} painted nodes`,
  height: index % 5 === 0 ? 58 : 42,
  color: index % 7 === 0 ? C.amber : index % 3 === 0 ? C.cyan : C.violet,
}))

const buttonStyle = {
  height: 38,
  paddingHorizontal: 13,
  alignItems: "center" as const,
  justifyContent: "center" as const,
  backgroundColor: C.raised,
  borderWidth: 1,
  borderColor: C.border,
  borderRadius: 9,
  hover: { backgroundColor: "#243247" as const, borderColor: "#52647e" as const },
  active: { backgroundColor: "#2c2858" as const },
}

function SectionTitle({ eyebrow, title, detail }: { eyebrow: string; title: string; detail: string }) {
  return (
    <div style={{ gap: 4 }}>
      <text style={{ color: C.violet, fontSize: 8 }}>{eyebrow}</text>
      <text style={{ color: C.text, fontSize: 14 }}>{title}</text>
      <text style={{ color: C.muted, fontSize: 9 }}>{detail}</text>
    </div>
  )
}

let app: ReturnType<typeof render> | undefined

function FoundationContent() {
  const { toast } = useToast()
  const list = useRef<VirtualListHandle>(null)
  const [expanded, setExpanded] = useState(true)
  const [note, setNote] = useState("BlendX keeps editor state in React while native code owns pixels and input.")
  const [secret, setSecret] = useState("simd-powered")
  const [release, setRelease] = useState("foundation-preview")
  const [activity, setActivity] = useState(INITIAL_ACTIVITY)
  const [followTail, setFollowTail] = useState(false)
  const [visible, setVisible] = useState("0–0")
  const [semantics, setSemantics] = useState("Press Inspect to query the retained accessibility tree.")

  const inspect = () => {
    const tree = app?.renderer.getAccessibilityTree() ?? []
    const counts = new Map<string, number>()
    for (const node of tree) counts.set(node.role, (counts.get(node.role) ?? 0) + 1)
    setSemantics(`${tree.length} nodes · ${[...counts].map(([role, count]) => `${role} ${count}`).join(" · ")}`)
  }

  const addActivity = () => {
    setActivity((current) => [...current, {
      id: current.length,
      title: `Live activity ${current.length}`,
      detail: "Inserted from React; follow-tail keeps the newest variable-height row visible.",
      height: 58,
      color: C.green,
    }])
  }

  useEffect(() => {
    const timer = setTimeout(inspect, 80)
    return () => clearTimeout(timer)
  }, [])

  useEffect(() => {
    if (!screenshotToast) return
    const timer = setTimeout(() => toast({ title: "Foundation ready", description: "Dialog, motion, editor and virtual-list primitives are active.", duration: 0 }), 60)
    return () => clearTimeout(timer)
  }, [toast])

  const animationTarget = useMemo(() => expanded
    ? { width: 238, height: 46, opacity: 1, borderRadius: 12 }
    : { width: 104, height: 30, opacity: 0.55, borderRadius: 15 }, [expanded])

  return (
    <div style={{ width: "100%", height: "100%", padding: 22, gap: 16, backgroundColor: C.canvas }}>
      <div style={{ height: 58, flexShrink: 0, flexDirection: "row", alignItems: "center", gap: 13 }}>
        <div style={{ width: 42, height: 42, alignItems: "center", justifyContent: "center", backgroundColor: C.violet, borderRadius: 12 }}><text style={{ color: "#ffffff", fontSize: 16 }}>FX</text></div>
        <div style={{ flexGrow: 1, gap: 3 }}><text style={{ color: C.text, fontSize: 19 }}>Application foundation</text><text style={{ color: C.muted, fontSize: 10 }}>Dialogs, notifications, motion, editing, semantics and memory-windowed data</text></div>
        <badge style={{ paddingHorizontal: 10, paddingVertical: 6, backgroundColor: "#143329", borderRadius: 8 }}><text style={{ color: C.green, fontSize: 8 }}>INTERACTIVE GALLERY</text></badge>
      </div>

      <div style={{ flexGrow: 1, minHeight: 0, flexDirection: "row", gap: 16 }}>
        <div style={{ width: 0, flexGrow: 1, minWidth: 0, gap: 14 }}>
          <div style={{ height: 148, flexShrink: 0, padding: 16, flexDirection: "row", gap: 18, backgroundColor: C.surface, borderWidth: 1, borderColor: C.border, borderRadius: 13 }}>
            <div style={{ width: 250, flexShrink: 0, gap: 13 }}>
              <SectionTitle eyebrow="DECLARATIVE MOTION" title="Native-style interpolation" detail="Numeric styles animate without browser CSS." />
              <button accessibilityLabel="Toggle motion sample" onClick={() => setExpanded((value) => !value)} style={{ ...buttonStyle, width: 132 }}><text style={{ color: C.text, fontSize: 9 }}>Toggle target</text></button>
            </div>
            <div style={{ flexGrow: 1, justifyContent: "center", padding: 14, backgroundColor: "#0d141f", borderRadius: 10 }}>
              <motion.div initial={{ width: 104, height: 30, opacity: 0.4, borderRadius: 15 }} animate={animationTarget} transition={{ duration: 360, easing: "easeInOut" }} style={{ alignItems: "center", justifyContent: "center", backgroundColor: C.violet }}>
                <text style={{ color: "#ffffff", fontSize: 9 }}>{expanded ? "238 px animated target" : "compact"}</text>
              </motion.div>
            </div>
          </div>

          <div style={{ height: 210, flexShrink: 0, padding: 16, gap: 13, backgroundColor: C.surface, borderWidth: 1, borderColor: C.border, borderRadius: 13 }}>
            <SectionTitle eyebrow="EDITOR INPUT" title="Selection and composition" detail="Caret navigation, clipboard, undo/redo, password masking and multiline text." />
            <div style={{ flexGrow: 1, minHeight: 0, flexDirection: "row", gap: 12 }}>
              <textarea accessibilityLabel="Foundation notes" value={note} onChange={(event) => setNote(event.value ?? "")} style={{ width: 0, flexGrow: 1, height: "100%", padding: 11, color: C.text, fontSize: 11, lineHeight: 17, whiteSpace: "preWrap", backgroundColor: C.raised, borderWidth: 1, borderColor: C.border, borderRadius: 9 }} />
              <div style={{ width: 250, flexShrink: 0, gap: 8 }}>
                <text style={{ color: C.muted, fontSize: 9 }}>Password field</text>
                <input accessibilityLabel="Demo password" password value={secret} onChange={(event) => setSecret(event.value ?? "")} style={{ width: "100%", height: 38, paddingHorizontal: 10, color: C.text, fontSize: 11, backgroundColor: C.raised, borderWidth: 1, borderColor: C.border, borderRadius: 8 }} />
                <text style={{ color: C.faint, fontSize: 8 }}>Try Ctrl+A, copy/paste, arrows and undo.</text>
              </div>
            </div>
          </div>

          <div style={{ flexGrow: 1, minHeight: 0, padding: 14, gap: 10, backgroundColor: C.surface, borderWidth: 1, borderColor: C.border, borderRadius: 13 }}>
            <div style={{ height: 44, flexShrink: 0, flexDirection: "row", alignItems: "center", gap: 8 }}>
              <div style={{ flexGrow: 1 }}><SectionTitle eyebrow="VARIABLE VIRTUAL LIST" title="600 activity records" detail={`Mounted window ${visible} · ${activity.length} logical rows`} /></div>
              <button onClick={() => list.current?.scrollToIndex(300, "center")} style={{ ...buttonStyle, width: 94, height: 34 }}><text style={{ color: C.text, fontSize: 8 }}>Jump to 300</text></button>
              <button onClick={() => { setFollowTail(true); addActivity() }} style={{ ...buttonStyle, width: 92, height: 34 }}><text style={{ color: C.text, fontSize: 8 }}>Add + follow</text></button>
            </div>
            <VirtualList
              ref={list}
              items={activity}
              estimatedItemHeight={46}
              getItemHeight={(item) => item.height}
              getItemKey={(item) => item.id}
              followTail={followTail}
              overdraw={2}
              onVisibleRangeChange={(start, end) => setVisible(`${start}–${Math.max(start, end - 1)}`)}
              style={{ width: "100%", flexGrow: 1, minHeight: 0, backgroundColor: "#0d141f", borderRadius: 9 }}
              renderItem={(item) => (
                <div style={{ width: "100%", height: item.height, paddingHorizontal: 11, flexDirection: "row", alignItems: "center", gap: 10, borderWidth: 1, borderColor: "#202d3e" }}>
                  <div style={{ width: 6, height: 6, flexShrink: 0, backgroundColor: item.color, borderRadius: 3 }} />
                  <div style={{ flexGrow: 1, minWidth: 0, gap: 3 }}><text style={{ color: C.text, fontSize: 9 }}>{item.title}</text><text style={{ color: C.muted, fontSize: 8, whiteSpace: "normal" }}>{item.detail}</text></div>
                  <text style={{ color: C.faint, fontSize: 8 }}>#{item.id}</text>
                </div>
              )}
            />
          </div>
        </div>

        <div style={{ width: 382, flexShrink: 0, gap: 14 }}>
          <div style={{ padding: 16, gap: 13, backgroundColor: C.surface, borderWidth: 1, borderColor: C.border, borderRadius: 13 }}>
            <SectionTitle eyebrow="LAYERED UI" title="Dialog and notifications" detail="Focus trapping, dismissal, restoration and timed queues." />
            <div style={{ flexDirection: "row", gap: 8 }}>
              <Dialog defaultOpen={screenshotDialog}>
                <DialogTrigger accessibilityLabel="Open deployment dialog" style={{ ...buttonStyle, width: 0, flexGrow: 1, backgroundColor: "#292653", borderColor: "#5d55b8" }}><text style={{ color: "#ffffff", fontSize: 9 }}>Open dialog</text></DialogTrigger>
                <DialogContent style={{ width: 430, padding: 20, gap: 15, backgroundColor: C.surface, borderWidth: 1, borderColor: "#566883", borderRadius: 14 }}>
                  <SectionTitle eyebrow="MODAL FOCUS" title="Deploy this foundation?" detail="Tab stays inside this dialog. Escape and the backdrop dismiss it." />
                  <input autoFocus accessibilityLabel="Release name" value={release} onChange={(event) => setRelease(event.value ?? "")} style={{ width: "100%", height: 40, paddingHorizontal: 11, color: C.text, fontSize: 11, backgroundColor: C.raised, borderWidth: 1, borderColor: C.border, borderRadius: 8 }} />
                  <div style={{ flexDirection: "row", justifyContent: "end", gap: 8 }}>
                    <DialogClose style={{ ...buttonStyle, width: 88 }}><text style={{ color: C.muted, fontSize: 9 }}>Cancel</text></DialogClose>
                    <DialogClose onClick={() => toast({ title: "Deployment queued", description: "The modal restored focus to its trigger." })} style={{ ...buttonStyle, width: 106, backgroundColor: C.violet, borderColor: C.violet }}><text style={{ color: "#ffffff", fontSize: 9 }}>Deploy</text></DialogClose>
                  </div>
                </DialogContent>
              </Dialog>
              <button accessibilityLabel="Show notification" onClick={() => toast({ title: "Renderer healthy", description: "The notification queue is rendered above native content." })} style={{ ...buttonStyle, width: 0, flexGrow: 1 }}><text style={{ color: C.text, fontSize: 9 }}>Show toast</text></button>
            </div>
          </div>

          <div style={{ padding: 16, gap: 12, backgroundColor: C.surface, borderWidth: 1, borderColor: C.border, borderRadius: 13 }}>
            <SectionTitle eyebrow="SEMANTIC TREE" title="Inspectable accessibility" detail="Roles, labels, values, states and native bounds." />
            <div style={{ minHeight: 54, padding: 11, justifyContent: "center", backgroundColor: C.raised, borderRadius: 8 }}><text selectable style={{ color: C.muted, fontSize: 9, lineHeight: 15, whiteSpace: "normal" }}>{semantics}</text></div>
            <button accessibilityLabel="Inspect accessibility tree" onClick={inspect} style={{ ...buttonStyle, width: "100%" }}><text style={{ color: C.text, fontSize: 9 }}>Inspect retained semantics</text></button>
          </div>

          <div style={{ padding: 16, gap: 11, backgroundColor: C.surface, borderWidth: 1, borderColor: C.border, borderRadius: 13 }}>
            <SectionTitle eyebrow="SELECTABLE TEXT" title="Wrapped native copy" detail="Drag across wrapped lines and press Ctrl+C." />
            <text selectable accessibilityLabel="Selectable renderer description" style={{ color: C.muted, fontSize: 10, lineHeight: 16, whiteSpace: "normal" }}>BlendX lays out this paragraph, wraps it to the card, paints the selection range with Blend2D, and places the selected UTF-8 text on the system clipboard.</text>
          </div>

          <div style={{ flexGrow: 1, minHeight: 0, padding: 16, gap: 8, backgroundColor: "#101925", borderWidth: 1, borderColor: C.border, borderRadius: 13 }}>
            <text style={{ color: C.text, fontSize: 11 }}>Try these interactions</text>
            {["Tab through semantic controls", "Resize the window and wrapped text", "Drag the activity scrollbar", "Select and edit multiline content", "Open the modal, then press Escape"].map((line) => <div key={line} style={{ height: 29, paddingHorizontal: 9, flexDirection: "row", alignItems: "center", backgroundColor: C.raised, borderRadius: 7 }}><div style={{ width: 5, height: 5, marginRight: 8, backgroundColor: C.violet, borderRadius: 3 }} /><text style={{ color: C.muted, fontSize: 8 }}>{line}</text></div>)}
          </div>
        </div>
      </div>

      <ToastViewport />
    </div>
  )
}

function FoundationApp() {
  return <ToastProvider><FoundationContent /></ToastProvider>
}

const headless = process.argv.includes("--headless")
const screenshot = process.argv.find((argument) => argument.startsWith("--screenshot="))?.slice("--screenshot=".length)
const mountedAt = performance.now()
app = render(<FoundationApp />, { title: "BlendX · Application Foundation", width: 1280, height: 800, threads: 4, headless })
const mountTimeMs = performance.now() - mountedAt

if (headless) setTimeout(() => {
  if (screenshot) app?.renderer.captureScreenshot(screenshot)
  console.log(JSON.stringify({ mountTimeMs: Number(mountTimeMs.toFixed(2)), records: INITIAL_ACTIVITY.length, ...app?.renderer.getStats() }))
  app?.stop()
}, 550)

process.on("SIGINT", () => app?.stop())
