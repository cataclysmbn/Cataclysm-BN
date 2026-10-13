/**
 * @module
 * A seeded walk from the main menu over the affordances the engine offers: every enabled one that can be
 * given arguments is invoked and must not fail with an error the engine's own offer rules out, and an
 * invoke whose arguments do not fit the affordance's shape must be refused with `invalid_arguments`.
 * Affordances that need a map position and quit are left alone (the game would end).
 *
 * BN_BINARY=out/build/linux-full/src/cataclysm-bn-tiles deno test --allow-read --allow-write \
 *   --allow-run --allow-env tests/protocol/crawler_test.ts
 */
import { assert, assertEquals } from "@std/assert"
import { Engine, Recorder, type Value } from "./scenarios/driver.ts"

const steps = 40
const walkSeed = 424242
/** What an engine that offered an affordance must not answer an invoke of it with. */
const refusals = [
  "validation_failed",
  "invalid_arguments",
  "unknown_affordance",
  "stale_screen",
  "stale_boundary",
  "stale_revision",
]

/** Small deterministic generator, so the walk is the same for the same engine. */
const mulberry32 = (seed: number) => () => {
  seed = (seed + 0x6D2B79F5) | 0
  let t = Math.imul(seed ^ (seed >>> 15), 1 | seed)
  t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t
  return ((t ^ (t >>> 14)) >>> 0) / 4294967296
}

/** Every component below `value`, itself included. */
const components = (value: Value): Value[] =>
  value === null || typeof value !== "object" ? [] : [
    ...(typeof value.kind === "string" && typeof value.id === "string" ? [value] : []),
    ...Object.values(value).flatMap(components),
  ]

/** The rows a component holds, in the sense of `arg_shape.row`, that can be chosen. */
const rowsOf = (component: Value): { id: string; label?: string }[] => {
  switch (component.kind) {
    case "list":
      return component.rows.filter((row: Value) => row.enabled && row.selectable && !row.header)
    case "tabs":
      return component.tabs.filter((tab: Value) => tab.enabled)
    case "grid":
      return component.cells.filter((cell: Value) => cell.enabled && cell.id)
    case "form":
      return component.fields
    default:
      return []
  }
}

/** Quitting ends the game, so the walk leaves it alone; the engine names it by its label, and ids are opaque. */
const leaving = /quit|exit/i
const staying = (entry: { id: string; label?: string }) => !leaving.test(entry.label ?? "")

/** Arguments that fit the affordance's shape on this screen, or undefined when the screen offers none to give. */
function argumentsFor(screen: Value, shape: Value, pick: <T>(items: T[]) => T): Value | undefined {
  const all = screen.root ? components(screen.root) : []
  const find = (id: string) => all.find((component) => component.id === id)
  const args: Value = {}
  if (shape.row) {
    const ids = rowsOf(find(shape.row.component_id) ?? {}).filter(staying).map((row) => row.id)
    if (ids.length === 0) return undefined
    args.row = pick(ids)
  }
  if (shape.rows) {
    const ids = rowsOf(find(shape.rows.component_id) ?? {}).filter(staying).map((row) => row.id)
    if (ids.length === 0) return undefined
    args.rows = [shape.rows.counts ? { id: pick(ids), count: 1 } : { id: pick(ids) }]
  }
  if (shape.count) {
    const { min, max, default: initial } = shape.count
    args.count = Math.min(max ?? Infinity, Math.max(min ?? 0, initial ?? min ?? 1))
  }
  if (shape.text) args.text = "x"
  if (shape.pos) return undefined
  if (shape.pane) {
    const panes = find(shape.pane.component_id)
    const ids = panes ? components(panes).filter((c) => c !== panes && c.kind === "panel") : []
    if (ids.length === 0) return undefined
    args.pane = pick(ids).id
  }
  return args
}

Deno.test({
  name: "crawler: from the main menu, offered affordances run and malformed invokes are refused",
  ignore: !Deno.env.get("BN_BINARY"),
  async fn() {
    await Engine.run(new Recorder(), async (engine) => {
      await engine.toMainMenu()
      const random = mulberry32(walkSeed)
      const pick = <T>(items: T[]) => items[Math.floor(random() * items.length)]
      const tried = new Set<string>()
      const path: string[] = []
      const counts = { invoked: 0, skipped: 0, refused: 0 }
      const screens = new Set<string>()

      for (let step = 0; step < steps; step++) {
        if (engine.stale) await engine.subscribe()
        const top = engine.top()
        screens.add(`${top.role}:${top.context ?? top.title ?? ""}`)
        const enabled: Value[] = top.affordances.filter((entry: Value) =>
          entry.enabled && staying(entry) &&
          !(entry.role === "cancel" && engine.mirror.interaction.stack.length === 1)
        )

        // Malformed arguments, once per affordance of a screen: nothing may run.
        for (const found of enabled) {
          const key = `${top.screen_id}|${found.id}`
          if (tried.has(key)) continue
          tried.add(key)
          const shape = found.args ?? {}
          const malformed = Object.keys(shape).length > 0 ? {} : { text: "x" }
          const outcome = await engine.invoke(found.id, malformed)
          assertEquals(
            outcome.error,
            "invalid_arguments",
            `step ${step} ${path.join(" > ")} ${found.id}`,
          )
          counts.refused++
        }

        const runnable = enabled.flatMap((found) => {
          const args = argumentsFor(top, found.args ?? {}, pick)
          return args === undefined ? [] : [{ found, args }]
        })
        counts.skipped += enabled.length - runnable.length
        if (runnable.length === 0) break
        const { found, args } = pick(runnable)
        path.push(found.id)
        const outcome = await engine.invoke(
          found.id,
          Object.keys(args).length > 0 ? args : undefined,
        )
        counts.invoked++
        assert(
          !refusals.includes(outcome.error ?? ""),
          `step ${step}: ${found.id} ${JSON.stringify(args)} failed with ${outcome.error}; path ${
            path.join(" > ")
          }`,
        )
      }
      console.log(
        `crawler: ${counts.invoked} invoked, ${counts.refused} malformed refused, ${counts.skipped} skipped, ${screens.size} screens: ${
          [...screens].join(", ")
        }`,
      )
      assert(
        counts.invoked > 0 && counts.refused > 0,
        "the walk reached affordances to run and to refuse",
      )
    })
  },
})
