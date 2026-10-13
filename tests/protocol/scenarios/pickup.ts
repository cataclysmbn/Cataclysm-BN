/**
 * @module
 * The pickup list over two stacks at the avatar's feet, a stack marked and given a count by its own
 * affordances, and the way out.
 */
import { assert, assertEquals } from "@std/assert"
import { Engine, type Scenario, type Value } from "./driver.ts"
import { walk, wish } from "./inventory.ts"

export const pickup: Scenario = {
  area: "pickup",
  run: (rec) =>
    Engine.run(rec, async (engine) => {
      await engine.toMainMenu()
      await engine.enterTutorial()
      // The first turns of the tutorial open lessons; let them out of the way.
      await engine.submit({ kind: "action", action_id: "wait" })
      await engine.choose("entry:0")
      await engine.acknowledge(() => engine.mirror.interaction.interaction === null)

      // A thing beside the one the tutorial's first tile holds makes a list worth opening: one alone is taken at once.
      await wish(engine, "rock")
      const opened = await engine.invoke("pickup_feet")
      assertEquals(opened.stages.at(-1), "completed", JSON.stringify(opened))
      const screen = engine.top()
      assertEquals(screen.role, "dialog")
      rec.accept("pickup screen", "screen", screen)
      const items = walk(screen.root).find((component) => component.id === "pickup:items")
      assert(items, "the list of stacks")
      assert(items.rows.length >= 2, "a row per stack")
      const row = items.rows[0]
      assertEquals(row.selected, false)
      rec.accept("stack of the pickup list, not yet marked", "row", row)

      const toggle = screen.affordances.find((entry: Value) => entry.id === items.activate)
      assert(toggle, "the list's mark affordance")
      assertEquals(toggle.expect, "boundary")
      rec.accept("mark affordance of the pickup list", "affordance", toggle)
      const marked = await engine.invoke(toggle.id, { row: row.id })
      assertEquals(marked.stages.at(-1), "completed", JSON.stringify(marked))
      rec.command("stack marked in the pickup list", marked)
      const after = () =>
        walk(engine.top().root).find((component) => component.id === "pickup:items")!.rows
          .find((entry: Value) => entry.id === row.id)
      assertEquals(after()?.selected, true, "the stack is marked")
      assertEquals(after()?.mark?.glyph, "+")

      const count = engine.top().affordances.find((entry: Value) => entry.id === "pickup:count")
      assert(count, "a count affordance")
      rec.accept("count affordance of the pickup list", "affordance", count)
      const counted = await engine.invoke(count.id, { row: row.id, count: 1 })
      assertEquals(counted.stages.at(-1), "completed", JSON.stringify(counted))
      rec.command("stack given a count in the pickup list", counted)

      const unknown = await engine.invoke(toggle.id, { row: "pickup:nothing" })
      assertEquals(unknown.error, "invalid_arguments")
      rec.command("mark of a row the list does not hold", unknown)

      const cancel = engine.top().affordances.find((entry: Value) => entry.role === "cancel")
      assert(cancel, "a cancel affordance")
      const left = await engine.invoke(cancel.id)
      assertEquals(left.stages.at(-1), "completed", JSON.stringify(left))
      rec.command("pickup list left", left)
      assertEquals(engine.top().role, "map")
    }),
}
