import { chmod, copyFile, open, readFile } from "node:fs/promises"

const [, , hostPath, bytecodePath, outputPath] = process.argv
if (!hostPath || !bytecodePath || !outputPath) {
  throw new Error("usage: node tools/pack-hermes.mjs <host> <bytecode> <output>")
}

await copyFile(hostPath, outputPath)
const bytecode = await readFile(bytecodePath)
const footer = Buffer.alloc(16)
footer.writeBigUInt64LE(BigInt(bytecode.length), 0)
footer.write("BLENDXH1", 8, "ascii")
const output = await open(outputPath, "a")
await output.write(bytecode)
await output.write(footer)
await output.close()
await chmod(outputPath, 0o755)
