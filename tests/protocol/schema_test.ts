/**
 * @module
 * The examples are the contract in executable form: valid ones pass their definition,
 * structurally wrong ones fail it. Continuity and visibility rules are tested elsewhere.
 * 0.1 keeps one examples file; 0.2 splits them by area under `0.2.examples/`.
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
const versions = [
  {
    version: "0.1",
    schema: await read("0.1.schema.json"),
    files: [{ area: "", examples: await read("0.1.examples.json") as Examples }],
  },
  { version: "0.2", schema: await read("0.2.schema.json"), files: await areas("0.2.examples") },
]

for (const { version, schema, files } of versions) {
  const ajv = new Ajv2020({ strict: false, allErrors: false })
  ajv.addSchema(schema, "bn")
  const check = (example: Example) => {
    const validate = ajv.getSchema(`bn#/$defs/${example.def}`)
    assert(validate, `no definition ${example.def}`)
    return { ok: validate(example.value), errors: validate.errors }
  }

  Deno.test(`${version}: schema compiles as Draft 2020-12`, () => {
    assert(ajv.validateSchema(schema))
  })

  for (const { area, examples } of files) {
    const prefix = area ? `${version} ${area}` : version
    for (const example of examples.valid) {
      Deno.test(`${prefix} valid: ${example.name}`, () => {
        const { ok, errors } = check(example)
        assert(ok, JSON.stringify(errors))
      })
    }
    for (const example of examples.invalid) {
      Deno.test(`${prefix} invalid: ${example.name}`, () => {
        assertEquals(check(example).ok, false, example.why)
      })
    }
  }

  Deno.test(`${version}: every method and notification names an existing definition`, () => {
    const defs = Object.keys(schema.$defs)
    const names = [
      ...Object.values<{ params: string; result: string }>(schema["x-methods"]).flatMap((
        method,
      ) => [method.params, method.result]),
      ...Object.values<string>(schema["x-notifications"]),
    ]
    for (const name of names) assert(defs.includes(name), name)
  })
}

Deno.test("0.2: example names are unique across areas", async () => {
  const names = (await areas("0.2.examples")).flatMap(({ examples }) =>
    [...examples.valid, ...examples.invalid].map((example) => example.name)
  )
  assertEquals(names.length, new Set(names).size)
})

Deno.test("0.2: hello offers only 0.2", async () => {
  const schema = await read("0.2.schema.json")
  assertEquals(schema.$defs.hello_result.properties.version, { const: "0.2" })
  const hellos = (await areas("0.2.examples")).flatMap(({ examples }) => examples.valid)
    .filter((example) => example.def === "hello_params")
  assert(hellos.length > 0)
  for (const hello of hellos) {
    assertEquals((hello.value as { versions: string[] }).versions, ["0.2"], hello.name)
  }
})
