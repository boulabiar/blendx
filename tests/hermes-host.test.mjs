import assert from "node:assert/strict"
import { spawnSync } from "node:child_process"
import test from "node:test"

test("Hermes host exits when the renderer closes despite live intervals", () => {
  const result = spawnSync(
    process.execPath,
    ["tools/hermes-app.mjs", "run", "tests/fixtures/hermes-shutdown.tsx", "--"],
    { cwd: process.cwd(), encoding: "utf8", timeout: 5_000 },
  )
  assert.equal(result.signal, null, `host timed out\n${result.stdout}\n${result.stderr}`)
  assert.equal(result.status, 0, `host failed\n${result.stdout}\n${result.stderr}`)
})
