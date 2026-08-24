/**
 * A pull-request review application inspired by GPUix's diff example.
 *
 * The original focuses on the diff itself. This BlendX port adds file
 * navigation, review controls, progress, and an anchored quick-file picker.
 */

import React from "react"
import { render } from "../src/index.js"

const { useMemo, useState } = React

const C = {
  canvas: "#0b0e14" as const,
  sidebar: "#0f131a" as const,
  surface: "#141922" as const,
  raised: "#1b222d" as const,
  border: "#2a3341" as const,
  text: "#e6ebf2" as const,
  muted: "#929dac" as const,
  faint: "#647083" as const,
  blue: "#65a5ff" as const,
  green: "#4bd5a0" as const,
  red: "#f47b86" as const,
  amber: "#f4bd63" as const,
}

function makeRendererPatch() {
  const lines = [
    "diff --git a/src/renderer.ts b/src/renderer.ts",
    "--- a/src/renderer.ts",
    "+++ b/src/renderer.ts",
    "@@ -1,18 +1,25 @@",
    " import { Scene } from './scene'",
    " import { present } from './window'",
    " ",
    " export function render(scene: Scene) {",
    "-  layoutEverything(scene.root)",
    "-  repaintEverything(scene)",
    "+  const dirtyNodes = scene.takeDirtyNodes()",
    "+  const previousBounds = captureBounds(dirtyNodes)",
    "+  layoutDirtyNodes(dirtyNodes)",
    "+  const damage = mergeDamage(previousBounds, dirtyNodes)",
    "+  paintDamage(scene, damage)",
    "+  present(damage)",
    " }",
    " ",
    "+function mergeDamage(before: Rect[], nodes: Node[]) {",
    "+  const after = nodes.map((node) => node.bounds)",
    "+  return coalesceRectangles([...before, ...after])",
    "+}",
  ]
  for (let index = 0; index < 34; index++) {
    lines.push(index % 8 === 0
      ? `+  metrics.record("damage.batch.${index}", damage.length)`
      : `   pipeline.paintLayer(${index}, damage)`)
  }
  return lines.join("\n")
}

function makeHostPatch() {
  const lines = [
    "diff --git a/native/hermes_host.cpp b/native/hermes_host.cpp",
    "--- a/native/hermes_host.cpp",
    "+++ b/native/hermes_host.cpp",
    "@@ -84,11 +84,18 @@ int runApplication(const Bundle& bundle) {",
    "-  auto source = loadJavaScript(bundle.path);",
    "-  runtime.evaluateJavaScript(source);",
    "+  auto bytecode = bundle.embeddedBytecode();",
    "+  if (!bytecode.valid()) {",
    "+    return reportInvalidBundle(bundle);",
    "+  }",
    "+  runtime.evaluateHermesBytecode(bytecode);",
    " ",
    "-  while (renderer.poll()) renderer.paint();",
    "+  while (renderer.poll()) {",
    "+    runtime.drainMicrotasks();",
    "+    renderer.paintDamage();",
    "+  }",
    "   return 0;",
    " }",
  ]
  for (let index = 0; index < 24; index++) {
    lines.push(index % 7 === 0
      ? `-  trace("legacy frame ${index}");`
      : `+  traceFrame(runtime, renderer, ${index});`)
  }
  return lines.join("\n")
}

function makeTypesPatch() {
  return [
    "diff --git a/src/types.ts b/src/types.ts",
    "--- a/src/types.ts",
    "+++ b/src/types.ts",
    "@@ -12,9 +12,16 @@ export interface FrameStats {",
    "   paintTimeMs: number",
    "+  layoutTimeMs: number",
    "+  presentTimeMs: number",
    "+  dirtyRectCount: number",
    "+  paintedPixels: number",
    " }",
    " ",
    " export interface RenderOptions {",
    "-  threads?: number",
    "+  /** Zero selects Blend2D's synchronous context. */",
    "+  threads?: 0 | 1 | 2 | 4 | 8",
    "+  headless?: boolean",
    "+  fontPath?: string",
    " }",
  ].join("\n")
}

type ReviewFile = {
  path: string
  added: number
  removed: number
  patch: string
}

const FILES: ReviewFile[] = [
  { path: "src/renderer.ts", added: 14, removed: 2, patch: makeRendererPatch() },
  { path: "native/hermes_host.cpp", added: 31, removed: 9, patch: makeHostPatch() },
  { path: "src/types.ts", added: 8, removed: 1, patch: makeTypesPatch() },
  { path: "examples/benchmark.tsx", added: 42, removed: 0, patch: makeRendererPatch().replaceAll("src/renderer.ts", "examples/benchmark.tsx") },
]

function ChangeCount({ file }: { file: ReviewFile }) {
  return (
    <div style={{ flexDirection: "row", alignItems: "center", gap: 7 }}>
      <text style={{ color: C.green, fontSize: 10 }}>+{file.added}</text>
      <text style={{ color: C.red, fontSize: 10 }}>−{file.removed}</text>
    </div>
  )
}

function FileButton({ file, active, onClick }: { file: ReviewFile; active: boolean; onClick: () => void }) {
  const slash = file.path.lastIndexOf("/")
  const folder = slash >= 0 ? file.path.slice(0, slash + 1) : ""
  const name = slash >= 0 ? file.path.slice(slash + 1) : file.path
  return (
    <button
      onClick={onClick}
      style={{
        height: 54,
        paddingHorizontal: 11,
        paddingVertical: 8,
        gap: 4,
        backgroundColor: active ? "#1b2635" : C.sidebar,
        borderRadius: 8,
        borderWidth: active ? 1 : 0,
        borderColor: active ? "#345074" : C.sidebar,
      }}
    >
      <div style={{ flexDirection: "row", alignItems: "center" }}>
        <text style={{ flexGrow: 1, color: active ? C.text : "#c4ccd7", fontSize: 12 }}>{name}</text>
        <ChangeCount file={file} />
      </div>
      <text style={{ color: C.faint, fontSize: 9 }}>{folder || "root"}</text>
    </button>
  )
}

function ReviewApp() {
  const [selected, setSelected] = useState(0)
  const [pickerOpen, setPickerOpen] = useState(false)
  const [review, setReview] = useState<"pending" | "approved" | "changes">("pending")
  const file = FILES[selected]!
  const lineCount = useMemo(() => file.patch.split("\n").length, [file])
  const totalAdded = FILES.reduce((sum, item) => sum + item.added, 0)
  const totalRemoved = FILES.reduce((sum, item) => sum + item.removed, 0)

  const selectFile = (index: number) => {
    setSelected(index)
    setPickerOpen(false)
  }

  return (
    <div style={{ width: "100%", height: "100%", backgroundColor: C.canvas }}>
      <div
        style={{
          height: 66,
          flexShrink: 0,
          paddingHorizontal: 18,
          flexDirection: "row",
          alignItems: "center",
          gap: 12,
          backgroundColor: C.surface,
        }}
      >
        <div style={{ width: 36, height: 36, alignItems: "center", justifyContent: "center", backgroundColor: "#24324a", borderRadius: 10 }}>
          <text style={{ color: C.blue, fontSize: 17 }}>↗</text>
        </div>
        <div style={{ flexGrow: 1, gap: 3 }}>
          <div style={{ flexDirection: "row", alignItems: "center", gap: 8 }}>
            <text style={{ color: C.text, fontSize: 16 }}>Optimize retained rendering pipeline</text>
            <badge style={{ paddingHorizontal: 7, paddingVertical: 3, backgroundColor: "#183128", borderRadius: 7 }}>
              <text style={{ color: C.green, fontSize: 9 }}>OPEN</text>
            </badge>
          </div>
          <text style={{ color: C.muted, fontSize: 10 }}>blendx/blendx  #42 · boulabiar wants to merge 6 commits into master</text>
        </div>
        <button
          onClick={() => setPickerOpen((open) => !open)}
          style={{ height: 32, paddingHorizontal: 11, justifyContent: "center", backgroundColor: C.raised, borderWidth: 1, borderColor: C.border, borderRadius: 8 }}
        >
          <text style={{ color: C.text, fontSize: 11 }}>Jump to file  ▾</text>
        </button>
        <button
          onClick={() => setReview("changes")}
          style={{ height: 32, paddingHorizontal: 12, justifyContent: "center", backgroundColor: "#302023", borderWidth: 1, borderColor: "#63353b", borderRadius: 8 }}
        >
          <text style={{ color: "#f4a1a8", fontSize: 11 }}>Request changes</text>
        </button>
        <button
          onClick={() => setReview("approved")}
          style={{ height: 32, paddingHorizontal: 14, justifyContent: "center", backgroundColor: "#17342a", borderWidth: 1, borderColor: "#28634c", borderRadius: 8 }}
        >
          <text style={{ color: "#75e5ba", fontSize: 11 }}>Approve</text>
        </button>
      </div>

      <separator style={{ width: "100%", height: 1, flexShrink: 0, color: C.border }} />

      <div style={{ flexGrow: 1, minHeight: 0, flexDirection: "row" }}>
        <div style={{ width: 264, height: "100%", flexShrink: 0, padding: 13, gap: 10, backgroundColor: C.sidebar }}>
          <div style={{ flexDirection: "row", alignItems: "center" }}>
            <text style={{ flexGrow: 1, color: C.muted, fontSize: 10 }}>CHANGED FILES</text>
            <badge style={{ paddingHorizontal: 7, paddingVertical: 3, backgroundColor: C.raised, borderRadius: 7 }}>
              <text style={{ color: C.muted, fontSize: 9 }}>{FILES.length}</text>
            </badge>
          </div>
          <progress value={selected + 1} max={FILES.length} style={{ width: "100%", height: 4, color: C.blue, backgroundColor: C.border }} />
          <div style={{ gap: 5 }}>
            {FILES.map((item, index) => (
              <FileButton key={item.path} file={item} active={index === selected} onClick={() => selectFile(index)} />
            ))}
          </div>
          <div style={{ flexGrow: 1 }} />
          <separator style={{ width: "100%", height: 1, color: C.border }} />
          <div style={{ gap: 6, padding: 3 }}>
            <text style={{ color: C.muted, fontSize: 10 }}>PULL REQUEST</text>
            <div style={{ flexDirection: "row", alignItems: "center" }}>
              <text style={{ flexGrow: 1, color: C.faint, fontSize: 10 }}>Total changes</text>
              <text style={{ color: C.green, fontSize: 10 }}>+{totalAdded}</text>
              <text style={{ marginLeft: 7, color: C.red, fontSize: 10 }}>−{totalRemoved}</text>
            </div>
            <div style={{ flexDirection: "row", alignItems: "center" }}>
              <text style={{ flexGrow: 1, color: C.faint, fontSize: 10 }}>Review status</text>
              <text style={{ color: review === "approved" ? C.green : review === "changes" ? C.red : C.amber, fontSize: 10 }}>
                {review === "approved" ? "Approved" : review === "changes" ? "Changes requested" : "Pending"}
              </text>
            </div>
          </div>
        </div>

        <separator style={{ width: 1, height: "100%", flexShrink: 0, color: C.border }} />

        <div style={{ flexGrow: 1, minWidth: 0, height: "100%", padding: 16, gap: 10 }}>
          <div
            style={{
              height: 48,
              flexShrink: 0,
              paddingHorizontal: 13,
              flexDirection: "row",
              alignItems: "center",
              backgroundColor: C.surface,
              borderWidth: 1,
              borderColor: C.border,
              borderRadius: 9,
            }}
          >
            <text style={{ flexGrow: 1, color: C.text, fontSize: 12 }}>{file.path}</text>
            <ChangeCount file={file} />
            <badge style={{ marginLeft: 12, paddingHorizontal: 8, paddingVertical: 4, backgroundColor: "#202735", borderRadius: 7 }}>
              <text style={{ color: C.muted, fontSize: 9 }}>UNIFIED</text>
            </badge>
          </div>

          <div
            style={{
              flexGrow: 1,
              minHeight: 0,
              overflow: "scroll",
              backgroundColor: "#0c1016",
              borderWidth: 1,
              borderColor: C.border,
              borderRadius: 9,
            }}
          >
            <diff
              patch={file.patch}
              wordDiff
              style={{
                width: "100%",
                height: lineCount * 21 + 28,
                padding: 14,
                color: "#cbd5e1",
                fontSize: 12,
                lineHeight: 21,
                backgroundColor: "#0c1016",
              }}
            />
          </div>

          <div style={{ height: 24, flexShrink: 0, flexDirection: "row", alignItems: "center" }}>
            <div style={{ width: 6, height: 6, backgroundColor: C.green, borderRadius: 3 }} />
            <text style={{ marginLeft: 7, color: C.muted, fontSize: 10 }}>{lineCount} lines · scroll to inspect the complete patch</text>
            <div style={{ flexGrow: 1 }} />
            <text style={{ color: C.faint, fontSize: 10 }}>Rendered natively with Blend2D</text>
          </div>
        </div>
      </div>

      {pickerOpen && (
        <anchored
          position={{ x: 1042, y: 58 }}
          side="bottom"
          align="end"
          anchorGap={7}
          style={{
            width: 310,
            padding: 8,
            gap: 4,
            position: "absolute",
            backgroundColor: "#1b222d",
            borderWidth: 1,
            borderColor: "#3a4658",
            borderRadius: 10,
          }}
        >
          <text style={{ paddingHorizontal: 8, paddingVertical: 6, color: C.faint, fontSize: 9 }}>QUICK FILE PICKER</text>
          {FILES.map((item, index) => (
            <button
              key={item.path}
              onClick={() => selectFile(index)}
              style={{ height: 34, paddingHorizontal: 9, flexDirection: "row", alignItems: "center", backgroundColor: index === selected ? "#263246" : C.raised, borderRadius: 6 }}
            >
              <text style={{ flexGrow: 1, color: C.text, fontSize: 11 }}>{item.path}</text>
              <ChangeCount file={item} />
            </button>
          ))}
        </anchored>
      )}
    </div>
  )
}

const headless = process.argv.includes("--headless")
const mountedAt = performance.now()
const app = render(<ReviewApp />, {
  title: "BlendX · Pull Request Review",
  width: 1280,
  height: 800,
  threads: 4,
  headless,
})
const mountTimeMs = performance.now() - mountedAt

if (headless) {
  setTimeout(() => {
    console.log(JSON.stringify({ mountTimeMs: Number(mountTimeMs.toFixed(2)), files: FILES.length, ...app.renderer.getStats() }))
    app.stop()
  }, 250)
}

process.on("SIGINT", () => app.stop())
