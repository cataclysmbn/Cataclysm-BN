/**
 * @module
 * The 1.0 examples are the contract in executable form: valid ones pass their definition,
 * structurally wrong ones fail it. Continuity and visibility rules are tested elsewhere.
 */
import { assert, assertEquals } from "@std/assert"
import { Ajv2020 } from "npm:ajv@8.17.1/dist/2020.js"

type Example = { name: string; def: string; value: unknown; why?: string }
const read = async (name: string) =>
  JSON.parse(
    await Deno.readTextFile(new URL(`../../docs/schema/engine-client/${name}`, import.meta.url)),
  )
const schema = await read("1.0.schema.json")
const examples: { valid: Example[]; invalid: Example[] } = await read("1.0.examples.json")

const ajv = new Ajv2020({ strict: false, allErrors: false })
ajv.addSchema(schema, "bn")
const check = (example: Example) => {
  const validate = ajv.getSchema(`bn#/$defs/${example.def}`)
  assert(validate, `no definition ${example.def}`)
  return { ok: validate(example.value), errors: validate.errors }
}

Deno.test("schema compiles as Draft 2020-12", () => {
  assert(ajv.validateSchema(schema))
})

for (const example of examples.valid) {
  Deno.test(`valid: ${example.name}`, () => {
    const { ok, errors } = check(example)
    assert(ok, JSON.stringify(errors))
  })
}

for (const example of examples.invalid) {
  Deno.test(`invalid: ${example.name}`, () => {
    assertEquals(check(example).ok, false, example.why)
  })
}

Deno.test("every method and notification names an existing definition", () => {
  const defs = Object.keys(schema.$defs)
  const names = [
    ...Object.values<{ params: string; result: string }>(schema["x-methods"]).flatMap((
      method,
    ) => [method.params, method.result]),
    ...Object.values<string>(schema["x-notifications"]),
  ]
  for (const name of names) assert(defs.includes(name), name)
})
