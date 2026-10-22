#!/usr/bin/env -S deno test --allow-read
/** @module Tests for finding where two replays of one recording part. */
import { assertEquals, assertThrows } from "@std/assert"
import { first_divergence, parse_state_log, type StateRecord } from "./replay_diff.ts"

const header = '{"kind":"header","format":"cataclysm-bn-state-hash","version":1}'
const state = (step: number, turn: number, rng = "a", map = "m"): StateRecord => ({
  step,
  turn,
  parts: { rng, avatar: "v", monsters: "o", npcs: "n", map },
})
const log = (records: StateRecord[]) =>
  [header, ...records.map((r) => JSON.stringify({ kind: "state", ...r }))].join("\n") + "\n"

Deno.test("a state log reads back its turns in order", () => {
  const records = [state(1, 10), state(2, 11)]
  assertEquals(parse_state_log(log(records)), records)
})

Deno.test("a state log of another format, version or step order is refused", () => {
  assertThrows(
    () => parse_state_log('{"kind":"header","format":"other","version":1}\n'),
    Error,
    "not a version 1",
  )
  assertThrows(
    () => parse_state_log(log([state(1, 10)]).replace('"version":1', '"version":2')),
    Error,
    "not a version 1",
  )
  assertThrows(() => parse_state_log(log([state(2, 10)])), Error, "malformed")
})

Deno.test("identical runs do not part", () => {
  assertEquals(first_divergence([state(1, 10), state(2, 11)], [state(1, 10), state(2, 11)]), null)
})

Deno.test("the first differing turn names only the parts that differ", () => {
  const base = [state(1, 10), state(2, 11, "a", "m"), state(3, 12, "x", "y")]
  const head = [state(1, 10), state(2, 11, "b", "m"), state(3, 12, "z", "w")]
  assertEquals(first_divergence(base, head), {
    step: 2,
    base_turn: 11,
    head_turn: 11,
    parts: ["rng"],
  })
})

Deno.test("a different turn at the same step parts even with equal hashes", () => {
  assertEquals(first_divergence([state(1, 10)], [state(1, 12)]), {
    step: 1,
    base_turn: 10,
    head_turn: 12,
    parts: [],
  })
})

Deno.test("a run that ends early parts at the step the other one still logs", () => {
  assertEquals(first_divergence([state(1, 10)], [state(1, 10), state(2, 11)]), {
    step: 2,
    base_turn: null,
    head_turn: 11,
    parts: ["avatar", "map", "monsters", "npcs", "rng"],
  })
})
