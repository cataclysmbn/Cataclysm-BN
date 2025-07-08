import { deepSort } from "./deep_sort.ts"
import { assertEquals } from "@std/assert"

const objectToSort = {
  a: { A: "Hello", B: "World" }, // Unordered
  b: "foo",
  c: { C: "bar", A: "baz" }, // Unordered inner object
  d: "extra", // Property not in reference
}

const expectedSortedObject = {
  c: { C: "bar", A: "baz" }, // 'c' comes first. Inner object not sorted by reference.
  a: { B: "World", A: "Hello" }, // 'a' comes second. Inner object sorted (B then A).
  b: "foo", // 'b' comes third.
  d: "extra", // 'd' comes last as it wasn't in reference.
}

Deno.test("deepSort() works for Maps", () => {
  const sortedResult = deepSort(
    new Map<string, unknown>([
      ["c", 1],
      ["a", new Map([["B", 1], ["A", 2]])],
      ["b", 2],
    ]),
    objectToSort,
  )
  assertEquals(sortedResult, expectedSortedObject)
})
Deno.test("deepSort() works for objects", () => {
  const sortedResult = deepSort(
    { a: { B: 1, A: 2 }, b: 2, c: 1 },
    objectToSort,
  )
  assertEquals(sortedResult, expectedSortedObject)
})
