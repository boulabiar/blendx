import React from "react"
import { render } from "../src/index.js"

const { useState } = React

function Counter() {
  const [count, setCount] = useState(0)

  return (
    <div
      style={{
        width: "100%",
        height: "100%",
        flexDirection: "column",
        padding: 32,
        gap: 20,
        backgroundColor: "#11131a",
      }}
    >
      <text style={{ color: "#f5f7ff", fontSize: 32 }}>BlendX</text>
      <text style={{ color: "#929bb3", fontSize: 17 }}>
        React UI rasterized by Blend2D on the CPU
      </text>
      <div
        style={{
          width: 280,
          height: 116,
          flexDirection: "column",
          padding: 18,
          gap: 10,
          backgroundColor: "#1d2230",
          borderRadius: 16,
        }}
      >
        <text style={{ color: "#aeb8d0", fontSize: 15 }}>Current count</text>
        <text style={{ color: "#ffffff", fontSize: 34 }}>{count}</text>
      </div>
      <div
        style={{
          width: 180,
          height: 52,
          paddingHorizontal: 22,
          paddingVertical: 15,
          backgroundColor: "#6c7cff",
          borderRadius: 12,
        }}
        onClick={() => setCount((value) => value + 1)}
      >
        <text style={{ color: "#ffffff", fontSize: 17 }}>Increment</text>
      </div>
    </div>
  )
}

const app = render(<Counter />, {
  title: "BlendX CPU UI",
  width: 900,
  height: 620,
  threads: 4,
})

process.on("SIGINT", () => app.stop())
