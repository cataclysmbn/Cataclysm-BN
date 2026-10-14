/**
 * @module
 * Looking around from the map: the cursor's square told fact by fact, the cursor moved by the screen's
 * own affordance, a square outside the map refused, and the way out.
 */
import { assert, assertEquals } from "@std/assert"
import { Engine, type Scenario, type Value } from "./driver.ts"
import { walk } from "./inventory.ts"

export const look: Scenario = {
  area: "look",
  run: (rec) =>
    Engine.run(rec, async (engine) => {
      await engine.toMainMenu()
      await engine.enterTutorial()
      // The first turns of the tutorial open lessons; let them out of the way.
      await engine.submit({ kind: "action", action_id: "wait" })
      await engine.choose("entry:0")
      await engine.acknowledge(() => engine.mirror.interaction.interaction === null)

      const opened = await engine.invoke("look")
      assertEquals(opened.stages.at(-1), "completed", JSON.stringify(opened))
      const screen = engine.top()
      assertEquals(screen.role, "dialog")
      rec.accept("look-around screen", "screen", screen)
      const target = () => walk(engine.top().root).find((c) => c.kind === "map")!.target
      const here = target()
      assertEquals(here.status, "look")
      assert(here.parts.length > 0, "the square is described")
      assertEquals(here.parts[0].kind, "location")

      const move = screen.affordances.find((entry: Value) => entry.id === "target:move")
      assert(move, "the cursor move")
      assertEquals(move.expect, "boundary")
      rec.accept("cursor move of the look-around screen", "affordance", move)
      const east = { ...here.current, x: here.current.x + 1 }
      const moved = await engine.invoke(move.id, { pos: east })
      assertEquals(moved.stages.at(-1), "completed", JSON.stringify(moved))
      rec.command("look-around cursor moved one square east", moved)
      assertEquals(target().current, east)
      assertEquals(target().distance, 1)

      // A square before the map the game holds is refused: the cursor stays.
      const outside = await engine.invoke(move.id, { pos: { ...east, x: -5 } })
      assertEquals(outside.stages.at(-1), "rejected", JSON.stringify(outside))
      assertEquals(target().current, east)

      const cancel = engine.top().affordances.find((entry: Value) => entry.role === "cancel")
      assert(cancel, "a cancel affordance")
      const left = await engine.invoke(cancel.id)
      assertEquals(left.stages.at(-1), "completed", JSON.stringify(left))
      rec.command("look-around left", left)
      assertEquals(engine.top().role, "map")
    }),
}
