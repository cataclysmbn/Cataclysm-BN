import { assertEquals, assertThrows } from "@std/assert"
import { Mirror } from "./mirror.ts"

const at = { epoch: "e", sequence: "0", revision: "0" }
const boundary = (id: string) => ({ boundary_id: id, interaction: null })
const pos = (x: number) => ({ dim: "", x, y: 0, z: 0 })
const cell = (x: number) => ({ at: pos(x), known: "visible" })
const fresh = () =>
  Mirror.fromSnapshot(
    {
      at,
      coverage: { min: pos(0), max: pos(9) },
      interaction: boundary("b:0"),
      entities: [],
      parts: 1,
    },
    [{ index: 0, last: true, at, cells: [cell(1)] }],
  )
const event = (sequence: number, changes: unknown) => ({
  sequence: String(sequence),
  revision: String(sequence),
  type: "cells.seen",
  changes,
})

Deno.test("a batch applies completely or not at all", () => {
  const mirror = fresh()
  const before = structuredClone(mirror.state())
  const good = event(1, { cells: [cell(2)] })
  assertThrows(() => mirror.applyEvents("e", [good, good]))
  assertEquals(mirror.state(), before)
  mirror.applyEvents("e", [good])
  assertEquals(mirror.state().cells.length, 2)
})

Deno.test("changes that do not fit leave the mirror untouched", () => {
  const mirror = fresh()
  const before = structuredClone(mirror.state())
  assertThrows(() =>
    mirror.applyEvents("e", [
      event(1, { cells: [cell(3)], gone: [{ id: "e:nope", reason: "lost_sight" }] }),
    ])
  )
  assertEquals(mirror.state(), before)
  assertThrows(() => mirror.applyEvents("other", [event(1, { cells: [cell(3)] })]))
  assertEquals(mirror.state(), before)
})
