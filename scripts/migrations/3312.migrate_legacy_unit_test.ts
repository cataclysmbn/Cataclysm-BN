import { assertEquals } from "https://deno.land/std@0.208.0/assert/assert_equals.ts"
import { base, bionic, schemasTransformer, vehiclePart } from "./3312.migrate_legacy_unit.ts"

const input = [{
  type: "vehicle_part",
  storage: 100,
  relative: { weight: 500, volume: -2, barrel_volume: -2, price: 5000 },
  workbench: { mass: 1000, volume: 1000 },
}]

const expected = [{
  type: "vehicle_part",
  storage: "25 L",
  relative: { weight: "500 g", volume: "-500 ml", barrel_volume: "-500 ml", price: "50 USD" },
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

Deno.test("converts top-level gun barrel volume", () => {
  const transformer = schemasTransformer([base])

  const result = transformer([{ type: "GUN", barrel_volume: 4 }])
  assertEquals(result, [{ type: "GUN", barrel_volume: "1 L" }])
})

Deno.test("preserves use_action arrays while converting sibling units", () => {
  const transformer = schemasTransformer([base])

  const result = transformer([{
    type: "TOOL",
    magazine_well: 1,
    use_action: [{ type: "repair_item" }],
  }])
  assertEquals(result, [{
    type: "TOOL",
    magazine_well: "250 ml",
    use_action: [{ type: "repair_item" }],
  }])
})
