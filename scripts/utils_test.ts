import { alias, omitEmpty } from "./utils.ts"
import { assertEquals } from "@std/assert"

Deno.test("omitEmpty() omits empty", () => {
  const input = {
    a: 1,
    b: "hello",
    c: null,
    d: undefined,
    e: [],
    f: [1, 2],
    g: {}, // Note: empty objects are not removed
    h: 0,
    i: "", // Note: empty strings are not removed
  }
  const expected = {
    a: 1,
    b: "hello",
    f: [1, 2],
    g: {},
    h: 0,
    i: "",
  }
  assertEquals(omitEmpty(input), expected)
})

Deno.test("omitEmpty() is idempotent", () => {
  const input = {
    a: 1,
    b: "hello",
    f: [1, 2],
    g: {},
    h: 0,
    i: "",
  }
  assertEquals(omitEmpty(input), input)
  assertEquals(omitEmpty(omitEmpty(input)), input)
})

Deno.test("alias() renames keys", () => {
  const input = { a: 1, b: 2, c: 3 }
  const renames = { a: "A", b: "B" }
  const expected = { A: 1, B: 2, c: 3 }
  assertEquals(alias(input, renames), expected)
})
