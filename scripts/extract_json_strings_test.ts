import { assertEquals } from "@std/assert"
import { dirname, fromFileUrl, join } from "@std/path"

const repoRoot = dirname(dirname(fromFileUrl(import.meta.url)))
const fixtureDir = join(repoRoot, "scripts/testdata/extract_json_strings/input")
const expectedSnapshot = join(repoRoot, "scripts/testdata/extract_json_strings/expected.pot")

const decoder = new TextDecoder()

const runDenoExtractor = async (args: string[]) => {
  const result = await new Deno.Command("deno", {
    args: [
      "run",
      "--allow-read",
      "--allow-write",
      "--allow-run",
      "scripts/extract_json_strings.ts",
      ...args,
    ],
    cwd: repoRoot,
    stdout: "piped",
    stderr: "piped",
  }).output()
  const stdout = decoder.decode(result.stdout)
  const stderr = decoder.decode(result.stderr)
  assertEquals(
    result.code,
    0,
    `deno run scripts/extract_json_strings.ts ${args.join(" ")}\n${stdout}\n${stderr}`,
  )
  return { stdout, stderr }
}

const normalizePot = (pot: string) => {
  return pot
    .replace(/^"POT-Creation-Date: .*\\n"$/m, '"POT-Creation-Date: <normalized>\\n"')
    .replaceAll(`${repoRoot}/`, "")
}

Deno.test("Deno extractor matches fixture snapshot", async () => {
  const tempDir = await Deno.makeTempDir()
  const denoPot = join(tempDir, "deno.pot")
  try {
    await runDenoExtractor([
      "-p",
      "Extractor Fixture Test",
      "-i",
      fixtureDir,
      "-o",
      denoPot,
    ])

    const denoOutput = normalizePot(await Deno.readTextFile(denoPot))
    const snapshot = await Deno.readTextFile(expectedSnapshot)

    assertEquals(denoOutput, snapshot)
  } finally {
    await Deno.remove(tempDir, { recursive: true })
  }
})
