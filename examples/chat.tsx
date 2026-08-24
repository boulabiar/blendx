import React, { memo, useEffect, useMemo, useState } from "react"
import { render } from "../src/index.js"
import type { CanvasCommand } from "../src/types.js"

const C = {
  canvas: "#17191f" as const,
  sidebar: "#121419" as const,
  raised: "#20242d" as const,
  composer: "#1d212a" as const,
  border: "#303642" as const,
  text: "#e5e7eb" as const,
  muted: "#9099a8" as const,
  faint: "#657084" as const,
  accent: "#e2795b" as const,
  blue: "#60a5fa" as const,
}

const CODE = `const frame = scene.commit();
renderer.paint(frame.dirtyRects);
console.log(frame.paintTimeMs);`

const PATCH = `@@ -41,3 +41,5 @@ renderer
- repaintEverything();
+ layoutDirtyNodes();
+ paintDirtyRegions();`

const MARKDOWN = `## Retained CPU rendering
BlendX keeps the React tree in native memory and sends only **mutations**.
- Blend2D SIMD rasterization
- dirty-region painting
- virtualized transcript rows`

type Message = { id: number; kind: number; text: string }

const MessageRow = memo(function MessageRow({ message }: { message: Message }) {
  if (message.kind === 0) {
    return (
      <div style={{ width: "100%", height: 112, paddingHorizontal: 18, paddingVertical: 14, alignItems: "end" }}>
        <div style={{ maxWidth: "70%", paddingHorizontal: 14, paddingVertical: 10, backgroundColor: "#2a303b", borderRadius: 12 }}>
          <text style={{ fontSize: 14, lineHeight: 20, color: C.text }}>{message.text}</text>
        </div>
      </div>
    )
  }
  if (message.kind === 2) {
    return (
      <div style={{ width: "100%", height: 112, paddingHorizontal: 18, paddingVertical: 8 }}>
        <code code={CODE} language="typescript" showHeader showLineNumbers style={{ width: "100%", height: 96, padding: 8, fontSize: 11, lineHeight: 18, color: "#d6deeb", backgroundColor: "#11151c", borderRadius: 8 }} />
      </div>
    )
  }
  if (message.kind === 3) {
    return (
      <div style={{ width: "100%", height: 112, paddingHorizontal: 18, paddingVertical: 8 }}>
        <diff patch={PATCH} wordDiff style={{ width: "100%", height: 96, padding: 8, fontSize: 11, lineHeight: 17, color: "#cbd5e1", backgroundColor: "#11151c", borderRadius: 8 }} />
      </div>
    )
  }
  return (
    <div style={{ width: "100%", height: 112, paddingHorizontal: 18, paddingVertical: 8 }}>
      <markdown source={MARKDOWN} style={{ width: "100%", height: 96, fontSize: 12, lineHeight: 17, color: "#cbd5e1" }} />
    </div>
  )
})

function Sidebar({ active, onSelect }: { active: number; onSelect: (id: number) => void }) {
  return (
    <div style={{ width: 248, height: "100%", flexShrink: 0, padding: 12, gap: 9, backgroundColor: C.sidebar }}>
      <div style={{ height: 42, flexDirection: "row", alignItems: "center", gap: 10 }}>
        <img src="/usr/share/pixmaps/debian-logo.png" alt="BlendX" objectFit="contain" style={{ width: 28, height: 28, borderRadius: 7 }} />
        <text style={{ flexGrow: 1, fontSize: 17, color: C.text }}>BlendX Chat</text>
        <button style={{ width: 28, height: 28, alignItems: "center", justifyContent: "center", backgroundColor: C.raised, borderRadius: 6 }} onClick={() => onSelect(0)}>
          <text style={{ color: C.text, fontSize: 18 }}>+</text>
        </button>
      </div>
      <button style={{ height: 36, flexDirection: "row", alignItems: "center", paddingHorizontal: 10, gap: 8, backgroundColor: C.raised, borderRadius: 8 }}>
        <svg src={'<svg viewBox="0 0 24 24" fill="none" stroke="#000" stroke-width="2"><circle cx="11" cy="11" r="7"/><path d="m16 16 5 5"/></svg>'} style={{ width: 16, height: 16, color: C.muted }} />
        <text style={{ color: C.muted, fontSize: 13 }}>Search conversations</text>
      </button>
      <separator style={{ width: "100%", color: C.border }} />
      <text style={{ fontSize: 11, color: C.faint }}>RECENT</text>
      {Array.from({ length: 7 }, (_, index) => (
        <button key={index} onClick={() => onSelect(index)} style={{ height: 48, paddingHorizontal: 10, paddingVertical: 7, gap: 4, backgroundColor: active === index ? "#272c36" : C.sidebar, borderRadius: 7 }}>
          <text style={{ color: C.text, fontSize: 13 }}>{index === 0 ? "Blend2D renderer architecture" : `Performance run ${index}`}</text>
          <text style={{ color: C.faint, fontSize: 11 }}>{index + 1}h · blendx</text>
        </button>
      ))}
      <div style={{ flexGrow: 1 }} />
      <separator style={{ width: "100%", color: C.border }} />
      <div style={{ height: 40, flexDirection: "row", alignItems: "center", gap: 9 }}>
        <badge style={{ paddingHorizontal: 8, paddingVertical: 4, backgroundColor: "#173629", borderRadius: 9 }}>
          <text style={{ fontSize: 11, color: "#86efac" }}>SIMD</text>
        </badge>
        <text style={{ fontSize: 12, color: C.muted }}>CPU renderer ready</text>
      </div>
    </div>
  )
}

function ChatApp({ count }: { count: number }) {
  const [tick, setTick] = useState(0)
  const [active, setActive] = useState(0)
  const [composer, setComposer] = useState("")
  const [picker, setPicker] = useState(false)
  const [sent, setSent] = useState<string[]>([])

  useEffect(() => {
    const timer = setInterval(() => setTick((value) => value + 1), 32)
    return () => clearInterval(timer)
  }, [])

  const messages = useMemo<Message[]>(() => Array.from({ length: count }, (_, index) => ({
    id: index,
    kind: index % 4,
    text: index === 0 ? "Can we render a large React chat UI efficiently on the CPU?" : `Message ${index.toLocaleString()} in the retained transcript`,
  })), [count])

  const graph = useMemo<CanvasCommand[]>(() => {
    const commands: CanvasCommand[] = [{ kind: "fillRect", x: 0, y: 0, width: 188, height: 32, color: "#151923", radius: 6 }]
    for (let index = 0; index < 45; index++) {
      const height = 5 + ((index * 13 + tick * 3) % 22)
      commands.push({ kind: "fillRect", x: 5 + index * 4, y: 28 - height, width: 2, height, color: index % 5 === 0 ? C.accent : C.blue, radius: 1 })
    }
    return commands
  }, [tick])

  const send = (value: string) => {
    const text = value.trim()
    if (!text) return
    setSent((items) => [...items, text])
    setComposer("")
  }

  return (
    <div style={{ width: "100%", height: "100%", flexDirection: "row", backgroundColor: C.canvas }}>
      <Sidebar active={active} onSelect={setActive} />
      <separator style={{ width: 1, height: "100%", flexShrink: 0, color: C.border }} />
      <div style={{ flexGrow: 1, height: "100%", position: "relative", backgroundColor: C.canvas }}>
        <div style={{ width: "100%", height: 54, paddingHorizontal: 18, flexDirection: "row", alignItems: "center", gap: 10, backgroundColor: "#191c22" }}>
          <div style={{ flexGrow: 1, gap: 2 }}>
            <text style={{ color: C.text, fontSize: 14 }}>Blend2D renderer architecture</text>
            <text style={{ color: C.faint, fontSize: 11 }}>{count.toLocaleString()} messages · {sent.length} sent locally</text>
          </div>
          <canvas commands={graph} style={{ width: 188, height: 32 }} />
          <progress value={(tick * 1.7) % 100} max={100} style={{ width: 100, height: 7, color: C.accent, backgroundColor: "#303642" }} />
          <button onClick={() => setPicker((value) => !value)} style={{ height: 30, paddingHorizontal: 10, justifyContent: "center", backgroundColor: C.raised, borderRadius: 7, borderWidth: 1, borderColor: C.border }}>
            <text style={{ color: C.text, fontSize: 12 }}>Model ▾</text>
          </button>
        </div>
        <separator style={{ width: "100%", color: C.border }} />
        <virtual-list itemHeight={112} overdraw={4} estimatedItemHeight={112} style={{ width: "100%", height: 612, overflow: "scroll", backgroundColor: C.canvas }}>
          {messages.map((message) => <MessageRow key={message.id} message={message} />)}
        </virtual-list>
        <div style={{ width: "100%", height: 128, paddingHorizontal: 22, paddingVertical: 14, backgroundColor: "#191c22" }}>
          <div style={{ width: "100%", height: 96, padding: 10, gap: 8, backgroundColor: C.composer, borderRadius: 12, borderWidth: 1, borderColor: C.border }}>
            <textarea value={composer} placeholder="Ask BlendX anything… (Ctrl+Enter to send)" minRows={2} maxRows={3} autoFocus style={{ width: "100%", height: 46, paddingHorizontal: 8, fontSize: 14, lineHeight: 20, color: C.text, backgroundColor: C.composer }} onChange={(event) => setComposer(event.value ?? "")} onSubmit={(event) => send(event.value ?? composer)} />
            <div style={{ height: 28, flexDirection: "row", alignItems: "center", gap: 7 }}>
              <badge style={{ paddingHorizontal: 8, paddingVertical: 4, backgroundColor: "#31261f", borderRadius: 7 }}><text style={{ color: C.accent, fontSize: 11 }}>BUILD</text></badge>
              <badge style={{ paddingHorizontal: 8, paddingVertical: 4, backgroundColor: C.raised, borderRadius: 7 }}><text style={{ color: C.muted, fontSize: 11 }}>GPT-5</text></badge>
              <div style={{ flexGrow: 1 }} />
              <button onClick={() => send(composer)} style={{ width: 28, height: 28, alignItems: "center", justifyContent: "center", backgroundColor: composer.trim() ? C.accent : "#343a46", borderRadius: 8 }}>
                <text style={{ color: C.text, fontSize: 16 }}>↑</text>
              </button>
            </div>
          </div>
        </div>
        {picker && (
          <anchored position={{ x: 925, y: 48 }} side="bottom" align="end" anchorGap={6} style={{ width: 230, padding: 8, gap: 5, position: "absolute", backgroundColor: "#252a34", borderRadius: 10, borderWidth: 1, borderColor: "#414958" }}>
            <text style={{ paddingHorizontal: 8, paddingVertical: 5, color: C.faint, fontSize: 11 }}>SELECT MODEL</text>
            {['GPT-5', 'Claude Sonnet', 'DeepSeek V4'].map((model) => (
              <button key={model} onClick={() => setPicker(false)} style={{ height: 32, paddingHorizontal: 8, justifyContent: "center", backgroundColor: model === 'GPT-5' ? "#343b48" : "#252a34", borderRadius: 6 }}>
                <text style={{ color: C.text, fontSize: 13 }}>{model}</text>
              </button>
            ))}
          </anchored>
        )}
        {sent.length > 0 && (
          <div style={{ position: "absolute", right: 18, bottom: 142, paddingHorizontal: 12, paddingVertical: 8, backgroundColor: "#173629", borderRadius: 8 }}>
            <text style={{ color: "#86efac", fontSize: 12 }}>Message queued</text>
          </div>
        )}
      </div>
    </div>
  )
}

const countArgument = process.argv.find((value) => value.startsWith("--messages="))
const count = countArgument ? Math.max(1, Number(countArgument.split("=")[1])) : 5_000
const headless = process.argv.includes("--headless")
const mountedAt = performance.now()
const app = render(<ChatApp count={count} />, { title: `BlendX Chat · ${count.toLocaleString()} messages`, width: 1200, height: 796, threads: 4, headless })
const mountTimeMs = performance.now() - mountedAt

const report = () => console.log(JSON.stringify({ mountTimeMs: Number(mountTimeMs.toFixed(2)), messages: count, ...app.renderer.getStats() }))
const reporter = setInterval(report, 1000)
if (headless) setTimeout(() => { clearInterval(reporter); report(); app.stop() }, 3200)
process.on("SIGINT", () => { clearInterval(reporter); app.stop() })
