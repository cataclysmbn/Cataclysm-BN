/**
 * @module
 * The examples are the contract in executable form: valid ones pass their definition,
 * structurally wrong ones fail it. Continuity and visibility rules are tested elsewhere.
 * The examples are split by area under `0.2.examples/`.
 */
import { assert, assertEquals } from "@std/assert"
import { Ajv2020 } from "npm:ajv@8.17.1/dist/2020.js"

type Example = { name: string; def: string; value: unknown; why?: string }
type Examples = { valid: Example[]; invalid: Example[] }
const root = new URL("../../docs/schema/engine-client/", import.meta.url)
const read = async (name: string) => JSON.parse(await Deno.readTextFile(new URL(name, root)))
const areas = async (dir: string) => {
  const names = (await Array.fromAsync(Deno.readDir(new URL(dir, root))))
    .map((entry) => entry.name).filter((name) => name.endsWith(".json")).sort()
  return Promise.all(names.map(async (name) => ({
    area: name.replace(/\.json$/, ""),
    examples: await read(`${dir}/${name}`) as Examples,
  })))
}
const schema = await read("0.2.schema.json")
const files = await areas("0.2.examples")
const ajv = new Ajv2020({ strict: false, allErrors: false })
ajv.addSchema(schema, "bn")
const check = (example: Example) => {
  const validate = ajv.getSchema(`bn#/$defs/${example.def}`)
  assert(validate, `no definition ${example.def}`)
  return { ok: validate(example.value), errors: validate.errors }
}

Deno.test("0.2: schema compiles as Draft 2020-12", () => {
  assert(ajv.validateSchema(schema))
})

for (const { area, examples } of files) {
  for (const example of examples.valid) {
    Deno.test(`0.2 ${area} valid: ${example.name}`, () => {
      const { ok, errors } = check(example)
      assert(ok, JSON.stringify(errors))
    })
  }
  for (const example of examples.invalid) {
    Deno.test(`0.2 ${area} invalid: ${example.name}`, () => {
      assertEquals(check(example).ok, false, example.why)
    })
  }
}

Deno.test("0.2: every method and notification names an existing definition", () => {
  const defs = Object.keys(schema.$defs)
  const names = [
    ...Object.values<{ params: string; result: string }>(schema["x-methods"]).flatMap((
      method,
    ) => [method.params, method.result]),
    ...Object.values<string>(schema["x-notifications"]),
  ]
  for (const name of names) assert(defs.includes(name), name)
})

Deno.test("0.2: example names are unique across areas", async () => {
  const names = (await areas("0.2.examples")).flatMap(({ examples }) =>
    [...examples.valid, ...examples.invalid].map((example) => example.name)
  )
  assertEquals(names.length, new Set(names).size)
})

Deno.test("0.2: a hello names the schema and a 0.1-only hello does not validate", () => {
  assertEquals(schema.$defs.hello_result.properties.version, { const: "0.2" })
  const hellos = files.flatMap(({ examples }) => examples.valid)
    .filter((example) => example.def === "hello_params")
  assert(hellos.length > 0)
  for (const { name, value } of hellos) {
    const { versions, schema } = value as { versions: string[]; schema?: string }
    assert(versions.includes("0.2") && schema, name)
  }
  const client = { name: "bn-deno", version: "1" }
  assertEquals(
    check({ name: "", def: "hello_params", value: { versions: ["0.1"], client } }).ok,
    false,
  )
})

Deno.test("0.2: the text a client shows is the one `text` type", async () => {
  const { $defs } = schema
  const text = { $ref: "#/$defs/text" }
  const refs = [
    $defs.affordance.properties.label,
    $defs.affordance.properties.denial,
    $defs.row.properties.label,
    $defs.row.properties.description,
    $defs.screen.properties.title,
    $defs.panel.properties.title,
    $defs.tabs.properties.tabs.items.properties.label,
    $defs.form_field.properties.label,
    $defs.text_component.properties.markup,
  ]
  for (const ref of refs) assertEquals(ref.$ref, text.$ref)
})

Deno.test("0.2: the hash of the schema file is a hello `schema` and a refusal names it", async () => {
  const digest = await crypto.subtle.digest(
    "SHA-256",
    await Deno.readFile(new URL("0.2.schema.json", root)),
  )
  const hash = "sha256:" +
    Array.from(new Uint8Array(digest), (byte) => byte.toString(16).padStart(2, "0"))
      .join("").slice(0, 16)
  const ajv = new Ajv2020({ strict: false })
  ajv.addSchema(await read("0.2.schema.json"), "bn")
  const validate = (def: string, value: unknown) => ajv.getSchema(`bn#/$defs/${def}`)!(value)
  const client = { name: "bn-deno", version: "1" }
  assert(validate("hello_params", { versions: ["0.2"], client, schema: hash }))
  assert(validate("hello_result", {
    version: "0.2",
    epoch: "epoch:1",
    engine: { build: "x", mods: [] },
    limits: { frame_bytes: 1, cells_per_part: 1, cells_per_query: 1, screen_queue: 8 },
    schema: hash,
    locale: "en",
    engine_locale: "en",
    role: "controller",
  }))
  assert(validate("application_error", {
    kind: "negotiation_failed",
    schema: { engine: hash, client: "sha256:0000000000000000" },
  }))
})
