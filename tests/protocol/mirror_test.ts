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

Deno.test("a route change replaces the planned route and an empty one clears it", () => {
  const mirror = fresh()
  assertEquals(mirror.state().route, [])
  mirror.applyEvents("e", [event(1, { route: [pos(2), pos(3)] })])
  assertEquals(mirror.state().route, [pos(2), pos(3)])
  mirror.applyEvents("e", [event(2, { route: [pos(4)] })])
  assertEquals(mirror.state().route, [pos(4)])
  mirror.applyEvents("e", [event(3, { route: [] })])
  assertEquals(mirror.state().route, [])
})

Deno.test("a view change replaces the clickable view whole", () => {
  const mirror = fresh()
  assertEquals(mirror.state().view, null)
  const view = { min: pos(0), max: pos(4) }
  mirror.applyEvents("e", [event(1, { view })])
  assertEquals(mirror.state().view, view)
  const moved = { min: pos(1), max: pos(5) }
  mirror.applyEvents("e", [event(2, { view: moved })])
  assertEquals(mirror.state().view, moved)
})

Deno.test("message lines append in order and a repeat replaces its line without a revision", () => {
  const mirror = fresh()
  const line = (sequence: number, id: string, text: string, count: number) => ({
    sequence: String(sequence),
    revision: "0",
    type: "message.logged",
    changes: {},
    data: { id, text, kind: "neutral", color: "c_white", count },
  })
  const start = Number(mirror.at.sequence)
  mirror.applyEvents("e", [
    line(start + 1, "7", "You open the door.", 1),
    line(start + 2, "8", "Bang.", 1),
    line(start + 3, "8", "Bang.", 2),
  ])
  assertEquals(mirror.messages.map((entry) => [entry.text, entry.count]), [
    ["You open the door.", 1],
    ["Bang.", 2],
  ])
})

Deno.test("a snapshot carries the log, and applying events gives the log a fresh snapshot carries", () => {
  const line = (id: string, text: string, count = 1) => ({
    id,
    text,
    kind: "neutral",
    color: "c_white",
    count,
  })
  const header = {
    at,
    coverage: { min: pos(0), max: pos(9) },
    interaction: boundary("b:0"),
    entities: [],
    messages: [line("1", "Saved line."), line("2", "Another.")],
    parts: 0,
  }
  const mirror = Mirror.fromSnapshot(header, [])
  assertEquals(mirror.state().messages.map((entry) => entry.text), ["Saved line.", "Another."])
  const logged = (sequence: number, data: ReturnType<typeof line>) => ({
    sequence: String(sequence),
    revision: "0",
    type: "message.logged",
    changes: {},
    data,
  })
  mirror.applyEvents("e", [logged(1, line("2", "Another.", 2)), logged(2, line("3", "New."))])
  // What a subscribe at sequence 2 would carry.
  const later = Mirror.fromSnapshot({
    ...header,
    at: { epoch: "e", sequence: "2", revision: "0" },
    messages: [line("1", "Saved line."), line("2", "Another.", 2), line("3", "New.")],
  }, [])
  assertEquals(mirror.state(), later.state())
})

Deno.test("the log keeps the newest hundred lines", () => {
  const mirror = fresh()
  const logged = (n: number) => ({
    sequence: String(n),
    revision: "0",
    type: "message.logged",
    changes: {},
    data: { id: String(n), text: `line ${n}`, kind: "neutral", color: "c_white", count: 1 },
  })
  mirror.applyEvents("e", Array.from({ length: 105 }, (_, i) => logged(i + 1)))
  assertEquals(mirror.messages.length, 100)
  assertEquals(mirror.messages[0].text, "line 6")
})
