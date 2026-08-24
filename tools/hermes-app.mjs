import { access, mkdir } from "node:fs/promises"
import { basename, dirname, resolve } from "node:path"
import { spawn } from "node:child_process"
import { fileURLToPath } from "node:url"
import { build } from "esbuild"

const [, , action, entryArgument, outputArgument, ...remaining] = process.argv
const actions = new Set(["bytecode", "run", "pack"])

if (!actions.has(action) || !entryArgument || (action === "pack" && !outputArgument)) {
  console.error("usage: node tools/hermes-app.mjs <bytecode|run> <entry.tsx> [-- app arguments...]")
  console.error("       node tools/hermes-app.mjs pack <entry.tsx> <output>")
  process.exit(2)
}

const projectRoot = resolve(dirname(fileURLToPath(import.meta.url)), "..")
const buildRoot = resolve(projectRoot, process.env.BLENDX_BUILD_DIR ?? "build")
const entry = resolve(projectRoot, entryArgument)
const appName = basename(entry).replace(/\.[^.]+$/, "")
const appDirectory = resolve(buildRoot, "hermes-apps")
const javascript = resolve(appDirectory, `${appName}.js`)
const bytecode = resolve(appDirectory, `${appName}.hbc`)
const compiler = resolve(buildRoot, "bin/hermesc")
const host = resolve(buildRoot, "blendx_hermes")

await mkdir(appDirectory, { recursive: true })
await Promise.all([required(compiler, "Hermes compiler"), required(host, "BlendX Hermes host")])

await build({
  entryPoints: [entry],
  outfile: javascript,
  bundle: true,
  platform: "neutral",
  format: "iife",
  target: "es2020",
  minify: process.env.BLENDX_DEBUG_BUNDLE !== "1",
  define: { "process.env.NODE_ENV": '"production"' },
  logLevel: "info",
})

await command(compiler, ["-O", "-w", "-emit-binary", "-out", bytecode, javascript])
console.log(`Hermes bytecode: ${bytecode}`)

if (action === "run") {
  const separator = outputArgument === "--" ? remaining : [outputArgument, ...remaining]
  const appArguments = separator.filter((argument) => argument !== undefined)
  process.exitCode = await command(host, [bytecode, ...appArguments], false)
} else if (action === "pack") {
  const output = resolve(projectRoot, outputArgument)
  await mkdir(dirname(output), { recursive: true })
  await command(process.execPath, [resolve(projectRoot, "tools/pack-hermes.mjs"), host, bytecode, output])
  console.log(`Packed executable: ${output}`)
}

async function required(path, label) {
  try {
    await access(path)
  } catch {
    throw new Error(`${label} not found at ${path}; run npm run build:native first`)
  }
}

function command(executable, arguments_, reject = true) {
  return new Promise((resolvePromise, rejectPromise) => {
    const child = spawn(executable, arguments_, { cwd: projectRoot, stdio: "inherit" })
    child.on("error", rejectPromise)
    child.on("exit", (code, signal) => {
      const status = code ?? 1
      if (status !== 0 && reject) {
        rejectPromise(new Error(`${basename(executable)} exited with ${signal ?? status}`))
      } else {
        resolvePromise(status)
      }
    })
  })
}
