import { spawnSync } from "node:child_process"

const profiles = ["sparse", "dense", "layout", "paint", "churn", "scroll", "native"]
const forwarded = process.argv.slice(2)
const hasComponents = forwarded.some((value) => value.startsWith("--components="))
const hasDuration = forwarded.some((value) => value.startsWith("--duration="))

for (const profile of profiles) {
  const arguments_ = [
    "tools/hermes-app.mjs",
    "run",
    "examples/visual-stress.tsx",
    "--",
    "--headless",
    `--profile=${profile}`,
    ...(hasComponents ? [] : ["--components=250"]),
    ...(hasDuration ? [] : ["--duration=2800"]),
    ...forwarded,
  ]
  const result = spawnSync(process.execPath, arguments_, { cwd: process.cwd(), stdio: "inherit" })
  if (result.status !== 0) process.exit(result.status ?? 1)
}
