import React from "react"
import { render } from "../../src/index.js"

const app = render(
  <div style={{ width: "100%", height: "100%" }} />,
  { width: 80, height: 60, headless: true },
)

// Model an animation timer that React has not cleaned up because the renderer
// was closed externally by SDL rather than through app.stop().
setInterval(() => {}, 10)
setTimeout(() => app.renderer.shutdown(), 25)
