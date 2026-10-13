/**
 * @module
 * Generates `docs/schema/engine-client/0.2.examples/<area>.json` by playing the scenarios against a real
 * engine: every example value is a message the engine sent or accepted, checked against the schema before
 * it is written. The epoch and the build, which differ per run, are masked; nothing else is edited.
 *
 *   BN_BINARY=out/build/linux-full/src/cataclysm-bn-tiles deno task contract:gen
 *   BN_BINARY=... deno task contract:check    (exits 1 when a file on disk differs from a fresh run)
 *
 * A generated area replaces the hand-written file of the same name; the other hand-written files stay
 * until a scenario covers their screens.
 */
import { assert } from "@std/assert"
import { fileURLToPath } from "node:url"
import { Ajv2020 } from "npm:ajv@8.17.1/dist/2020.js"
import { type Example, Recorder } from "./scenarios/driver.ts"
import { scenarios } from "./scenarios/mod.ts"

const root = new URL("../../docs/schema/engine-client/", import.meta.url)
const examplesDir = new URL("0.2.examples/", root)
const check = Deno.args.includes("--check")

const ajv = new Ajv2020({ strict: false })
ajv.addSchema(JSON.parse(await Deno.readTextFile(new URL("0.2.schema.json", root))), "bn")
const conforms = (example: Example) => {
  const validate = ajv.getSchema(`bn#/$defs/${example.def}`)
  assert(validate, `no definition ${example.def}`)
  return { ok: validate(example.value), errors: validate.errors }
}

/**
 * Replaces the masked strings, longest first, in the text of the examples. Command and boundary ids are
 * digests of the epoch, so they are numbered in order of appearance instead.
 */
const masked = (text: string, masks: Map<string, string>) => {
  const numbered = new Map<string, string>()
  return [...masks].sort(([a], [b]) => b.length - a.length)
    .reduce((out, [real, stable]) => out.replaceAll(real, stable), text)
    .replace(/\b(command|boundary):[0-9a-f]{8,}\b/g, (id, kind) => {
      if (!numbered.has(id)) {
        numbered.set(
          id,
          `${kind}:${[...numbered.keys()].filter((key) => key.startsWith(kind)).length + 1}`,
        )
      }
      return numbered.get(id)!
    })
}

/** The formatter's own output, so a `deno fmt` after the run changes nothing. */
async function format(text: string): Promise<string> {
  const process = new Deno.Command(Deno.execPath(), {
    args: [
      "fmt",
      "--config",
      fileURLToPath(new URL("../../deno.jsonc", import.meta.url)),
      "--ext",
      "json",
      "-",
    ],
    stdin: "piped",
    stdout: "piped",
    stderr: "piped",
  }).spawn()
  const writer = process.stdin.getWriter()
  await writer.write(new TextEncoder().encode(text))
  await writer.close()
  const { success, stdout, stderr } = await process.output()
  assert(success, new TextDecoder().decode(stderr))
  return new TextDecoder().decode(stdout)
}

const generated = new Map<string, string>()
for (const { area, run } of scenarios) {
  const rec = new Recorder()
  await run(rec)
  const text = masked(JSON.stringify({ valid: rec.valid, invalid: rec.invalid }), rec.masks)
  const { valid, invalid } = JSON.parse(text) as { valid: Example[]; invalid: Example[] }
  for (const example of valid) {
    const { ok, errors } = conforms(example)
    assert(ok, `${area}: "${example.name}" violates ${example.def}: ${JSON.stringify(errors)}`)
  }
  for (const example of invalid) {
    assert(
      !conforms(example).ok,
      `${area}: "${example.name}" is valid ${example.def}: ${example.why}`,
    )
  }
  generated.set(`${area}.json`, await format(JSON.stringify({ valid, invalid }, null, 2) + "\n"))
}

// Example names are unique across areas (schema_test), generated or not.
const names = new Map<string, string>()
const clashes: string[] = []
const file = (name: string) => new URL(name, examplesDir)
const others = (await Array.fromAsync(Deno.readDir(examplesDir)))
  .map((entry) => entry.name).filter((name) => name.endsWith(".json") && !generated.has(name))
for (
  const [name, text] of [
    ...generated,
    ...await Promise.all(
      others.map(async (name) => [name, await Deno.readTextFile(file(name))] as const),
    ),
  ]
) {
  const { valid, invalid } = JSON.parse(text) as { valid: Example[]; invalid: Example[] }
  for (const example of [...valid, ...invalid]) {
    const owner = names.get(example.name)
    if (owner) clashes.push(`"${example.name}" is in ${owner} and ${name}`)
    names.set(example.name, name)
  }
}
assert(clashes.length === 0, clashes.join("\n"))

const drifted: string[] = []
for (const [name, text] of generated) {
  const current = await Deno.readTextFile(file(name)).catch(() => undefined)
  if (current === text) continue
  drifted.push(name)
  if (!check) await Deno.writeTextFile(file(name), text)
}
if (check && drifted.length > 0) {
  console.error(
    `examples differ from a fresh run: ${drifted.join(", ")}; run deno task contract:gen`,
  )
  Deno.exit(1)
}
console.log(drifted.length > 0 ? `wrote ${drifted.join(", ")}` : "examples are current")
