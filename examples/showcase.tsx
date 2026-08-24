/**
 * Port of GPUix's native-text showcase to BlendX.
 *
 * It keeps the original Markdown + code + diff composition while presenting
 * it as a small, interactive release-review screen.
 */

import React from "react"
import { render } from "../src/index.js"

const { useState } = React

const C = {
  canvas: "#090b10" as const,
  panel: "#11151c" as const,
  panelRaised: "#171c25" as const,
  border: "#293140" as const,
  text: "#e8edf5" as const,
  muted: "#8d98aa" as const,
  faint: "#5f6b7d" as const,
  accent: "#8b7cf6" as const,
  green: "#43d19e" as const,
}

const NOTES = [
  "## Release notes",
  "",
  "BlendX now runs **React** with Hermes and paints every element on the CPU.",
  "",
  "- Blend2D selects SIMD pipelines at runtime",
  "- React commits become retained native mutations",
  "- Dirty rectangles keep updates inexpensive",
].join("\n")

const SNIPPETS = [
  `const app = render(<Dashboard />, {
  title: "BlendX",
  width: 1200,
  height: 760,
  threads: 4,
})`,
  `const frame = renderer.commit()
layoutDirtyNodes(frame)
paintDirtyRegions(frame)
present(frame.damage)`,
]

const PATCHES = [
  [
    "diff --git a/src/renderer.ts b/src/renderer.ts",
    "--- a/src/renderer.ts",
    "+++ b/src/renderer.ts",
    "@@ -41,8 +41,10 @@ export function commit() {",
    "   const frame = scene.commit()",
    "-  layoutEverything(frame)",
    "-  repaintEverything(frame)",
    "+  const dirty = layoutDirtyNodes(frame)",
    "+  const damage = mergeDamageRects(dirty)",
    "+  paintDirtyRegions(frame, damage)",
    "+  present(damage)",
    "   return frame.stats",
    " }",
  ].join("\n"),
  [
    "diff --git a/src/runtime.ts b/src/runtime.ts",
    "--- a/src/runtime.ts",
    "+++ b/src/runtime.ts",
    "@@ -12,7 +12,9 @@ export async function launch() {",
    "-  await startJavaScriptRuntime()",
    "+  const bytecode = readEmbeddedBundle()",
    "+  const runtime = createHermesRuntime()",
    "+  await runtime.evaluate(bytecode)",
    "   renderer.run()",
    " }",
  ].join("\n"),
]

function Label({ children }: { children: React.ReactNode }) {
  return <text style={{ color: C.faint, fontSize: 10 }}>{children}</text>
}

function Panel({ title, children }: { title: string; children: React.ReactNode }) {
  return (
    <div style={{ flexGrow: 1, minWidth: 0, height: "100%", gap: 9 }}>
      <Label>{title}</Label>
      <div
        style={{
          flexGrow: 1,
          minHeight: 0,
          padding: 16,
          backgroundColor: C.panel,
          borderWidth: 1,
          borderColor: C.border,
          borderRadius: 10,
          overflow: "hidden",
        }}
      >
        {children}
      </div>
    </div>
  )
}

function Showcase() {
  const [sample, setSample] = useState(0)

  return (
    <div style={{ width: "100%", height: "100%", padding: 22, gap: 18, backgroundColor: C.canvas }}>
      <div style={{ height: 54, flexShrink: 0, flexDirection: "row", alignItems: "center", gap: 12 }}>
        <div
          style={{
            width: 38,
            height: 38,
            alignItems: "center",
            justifyContent: "center",
            backgroundColor: C.accent,
            borderRadius: 10,
          }}
        >
          <text style={{ color: "#ffffff", fontSize: 19 }}>B</text>
        </div>
        <div style={{ flexGrow: 1, gap: 3 }}>
          <text style={{ color: C.text, fontSize: 18 }}>Native text showcase</text>
          <text style={{ color: C.muted, fontSize: 11 }}>Ported from GPUix · rendered by Blend2D on the CPU</text>
        </div>
        <badge style={{ paddingHorizontal: 9, paddingVertical: 5, backgroundColor: "#182d29", borderRadius: 8 }}>
          <text style={{ color: C.green, fontSize: 10 }}>HERMES</text>
        </badge>
        <badge style={{ paddingHorizontal: 9, paddingVertical: 5, backgroundColor: "#24213a", borderRadius: 8 }}>
          <text style={{ color: "#b7acf9", fontSize: 10 }}>SIMD</text>
        </badge>
        <button
          onClick={() => setSample((value) => (value + 1) % SNIPPETS.length)}
          style={{
            height: 32,
            paddingHorizontal: 12,
            justifyContent: "center",
            backgroundColor: C.panelRaised,
            borderWidth: 1,
            borderColor: C.border,
            borderRadius: 8,
          }}
        >
          <text style={{ color: C.text, fontSize: 11 }}>Change sample</text>
        </button>
      </div>

      <separator style={{ width: "100%", height: 1, flexShrink: 0, color: C.border }} />

      <div style={{ height: 220, flexShrink: 0, flexDirection: "row", gap: 18 }}>
        <Panel title="MARKDOWN">
          <markdown
            source={NOTES}
            style={{ width: "100%", height: "100%", color: C.text, fontSize: 13, lineHeight: 23 }}
          />
        </Panel>
        <Panel title="TYPESCRIPT">
          <code
            code={SNIPPETS[sample]!}
            language="typescript"
            showHeader
            showLineNumbers
            style={{
              width: "100%",
              height: "100%",
              padding: 12,
              color: "#d9e2f2",
              fontSize: 12,
              lineHeight: 22,
              backgroundColor: "#0c1016",
              borderRadius: 7,
            }}
          />
        </Panel>
      </div>

      <div style={{ flexGrow: 1, minHeight: 0, gap: 9 }}>
        <div style={{ height: 18, flexShrink: 0, flexDirection: "row", alignItems: "center" }}>
          <Label>DIFF REVIEW</Label>
          <div style={{ flexGrow: 1 }} />
          <text style={{ color: C.green, fontSize: 10 }}>+4</text>
          <text style={{ marginLeft: 8, color: "#f27f86", fontSize: 10 }}>−2</text>
        </div>
        <diff
          patch={PATCHES[sample]!}
          wordDiff
          style={{
            flexGrow: 1,
            minHeight: 0,
            padding: 14,
            color: "#cbd5e1",
            fontSize: 12,
            lineHeight: 22,
            backgroundColor: C.panel,
            borderWidth: 1,
            borderColor: C.border,
            borderRadius: 10,
            overflow: "scroll",
          }}
        />
      </div>

      <div style={{ height: 18, flexShrink: 0, flexDirection: "row", alignItems: "center" }}>
        <div style={{ width: 6, height: 6, backgroundColor: C.green, borderRadius: 3 }} />
        <text style={{ marginLeft: 7, color: C.muted, fontSize: 10 }}>All native painters ready</text>
        <div style={{ flexGrow: 1 }} />
        <text style={{ color: C.faint, fontSize: 10 }}>Markdown · code · diff · retained layout</text>
      </div>
    </div>
  )
}

const headless = process.argv.includes("--headless")
const mountedAt = performance.now()
const app = render(<Showcase />, {
  title: "BlendX · Native Text Showcase",
  width: 1180,
  height: 760,
  threads: 4,
  headless,
})
const mountTimeMs = performance.now() - mountedAt

if (headless) {
  setTimeout(() => {
    console.log(JSON.stringify({ mountTimeMs: Number(mountTimeMs.toFixed(2)), ...app.renderer.getStats() }))
    app.stop()
  }, 250)
}

process.on("SIGINT", () => app.stop())
