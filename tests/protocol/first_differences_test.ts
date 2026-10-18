import { assertEquals } from "@std/assert"
import { firstDifferences } from "./first_differences.ts"

Deno.test("the first differing lines are numbered from one, old then new", () => {
  assertEquals(firstDifferences("a\nb\nc", "a\nB\nc"), "  2: - b\n  2: + B")
})

Deno.test("a line one text lacks reads as empty", () => {
  assertEquals(firstDifferences("a", "a\nb"), "  2: - \n  2: + b")
})

Deno.test("no more than the limit of lines is shown", () => {
  assertEquals(firstDifferences("1\n2\n3", "x\ny\nz", 2).split("\n").length, 4)
})
