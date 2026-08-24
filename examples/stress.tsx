import React from "react"
import { render, VirtualList } from "../src/index.js"

const { memo, useEffect, useState } = React

const ROW_COUNT = 20_000
const CELL_COUNT = 320
const headless = process.argv.includes("--headless")

const Row = memo(function Row({ index }: { index: number }) {
  const backgroundColor = index % 2 === 0 ? "#151923" : "#181d28"
  return (
    <div
      style={{
        width: "100%",
        height: 36,
        paddingHorizontal: 12,
        paddingVertical: 8,
        backgroundColor,
      }}
    >
      <text style={{ color: "#aeb8d0", fontSize: 14 }}>
        Row {index.toString().padStart(5, "0")} · retained in C++, painted only when visible
      </text>
    </div>
  )
})

const rows = Array.from({ length: ROW_COUNT }, (_, index) => index)

function StressApp() {
  const [tick, setTick] = useState(0)

  useEffect(() => {
    const timer = setInterval(() => setTick((value) => value + 1), 16)
    return () => clearInterval(timer)
  }, [])

  return (
    <div
      style={{
        width: "100%",
        height: "100%",
        padding: 24,
        gap: 12,
        backgroundColor: "#0f1117",
      }}
    >
      <text style={{ color: "#f5f7ff", fontSize: 26 }}>BlendX stress workload</text>
      <text style={{ color: "#8f99b2", fontSize: 14 }}>
        20,000 virtual rows + 320 changing elements · scroll with the mouse wheel
      </text>
      <div
        style={{
          width: "100%",
          height: 80,
          padding: 8,
          gap: 2,
          flexDirection: "row",
          overflow: "hidden",
          backgroundColor: "#171b25",
          borderRadius: 10,
        }}
      >
        {Array.from({ length: CELL_COUNT }, (_, index) => {
          const active = (index + tick) % 17 < 5
          return (
            <div
              key={index}
              style={{
                width: 3,
                height: 64,
                backgroundColor: active ? "#7181ff" : "#272d3d",
                borderRadius: 2,
              }}
            />
          )
        })}
      </div>
      <VirtualList
        items={rows}
        estimatedItemHeight={36}
        overdraw={3}
        style={{
          width: "100%",
          height: 500,
          backgroundColor: "#151923",
          borderRadius: 10,
        }}
        renderItem={(index) => <Row index={index} />}
      />
    </div>
  )
}

const mountedAt = performance.now()
const app = render(<StressApp />, {
  title: "BlendX Stress · 40k retained nodes",
  width: 1000,
  height: 760,
  threads: 4,
  headless,
})
const mountTimeMs = performance.now() - mountedAt

function report() {
  const stats = app.renderer.getStats()
  console.log(JSON.stringify({ mountTimeMs: Number(mountTimeMs.toFixed(2)), ...stats }))
}

const reporter = setInterval(report, 1000)
if (headless) {
  setTimeout(() => {
    clearInterval(reporter)
    report()
    app.stop()
  }, 3200)
}

process.on("SIGINT", () => {
  clearInterval(reporter)
  app.stop()
})
