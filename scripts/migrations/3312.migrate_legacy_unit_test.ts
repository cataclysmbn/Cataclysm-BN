import { assertEquals } from "https://deno.land/std@0.208.0/assert/assert_equals.ts"
import { base, bionic, schemasTransformer, vehiclePart } from "./3312.migrate_legacy_unit.ts"

const input = [{
  type: "vehicle_part",
  storage: 100,
  workbench: { mass: 1000, volume: 1000 },
}]

const expected = [{
  type: "vehicle_part",
  storage: "25 L",
  workbench: { mass: "1 kg", volume: "250 L" },
}]

Deno.test("forwards", () => {
  const transformer = schemasTransformer([base, vehiclePart])

  const result = transformer(input)
  assertEquals(result, expected)
})

Deno.test("backwards", () => {
  const transformer = schemasTransformer([vehiclePart, base])

  const result = transformer(input)
  assertEquals(result, expected)
})

Deno.test("even if first doesn't match, it should still match second", () => {
  const transformer = schemasTransformer([bionic, vehiclePart, base])

  const result = transformer(input)
  assertEquals(result, expected)
})

Deno.test("smoke test", () => {
  const transformer = schemasTransformer([vehiclePart, bionic, base])

  const result = transformer(input)
  assertEquals(result, expected)
})
