/**
 * @module
 * The map screen on the generic path: a burst of screen-scoped moves that all complete, Options opened
 * through the map's own affordance, a burst of keys aimed at a popup that an earlier key of the burst
 * closed, and a held step: key down and key up, which the engine answers with a step per input prompt.
 */
import { assert, assertEquals } from "@std/assert"
import { Engine, type Scenario, type Value, without } from "./driver.ts"

export const map: Scenario = {
  area: "map",
  run: (rec) =>
    Engine.run(rec, async (engine) => {
      await engine.toMainMenu()
      await engine.enterTutorial()
      // The first turns of the tutorial open lessons; let them out of the way.
      await engine.submit({ kind: "action", action_id: "wait" })
      await engine.choose("entry:0")
      await engine.acknowledge(() => engine.mirror.interaction.interaction === null)

      const map = engine.top()
      assertEquals(map.role, "map")
      const options = map.affordances.find((entry: Value) => entry.id === "open_options")
      assert(options, "the map offers Options")
      assertEquals([options.label, options.expect], ["Options", "screen"])
      rec.accept("Options affordance of the map", "affordance", options)

      // Keys faster than the moves run: the engine queues the screen-scoped ones.
      const arrow = map.affordances.find((entry: Value) => entry.id === "RIGHT")
      assert(arrow, "a movement affordance")
      const held = await engine.burst(Array.from({ length: 5 }, () => ({ affordance: arrow.id })))
      for (const move of held) assertEquals(move.stages.at(-1), "completed", JSON.stringify(move))
      rec.command("first of five held moves", held[0])
      rec.command("last of five held moves", held[4])
      assertEquals(engine.top().screen_id, map.screen_id, "the map keeps its id")

      // Options opens from the map, with no detour through the main menu.
      const opened = await engine.invoke("open_options")
      assertEquals(opened.stages.at(-1), "completed")
      assertEquals(engine.top().context, "OPTIONS")
      rec.command("Options invoked at the map", opened)
      assertEquals((await engine.invoke("QUIT")).stages.at(-1), "completed")
      await engine.acknowledge(() => engine.top().role === "map")

      // A burst aimed at a popup: the first key closes it, the rest meet the map.
      assertEquals(
        (await engine.submit({ kind: "action", action_id: "wait" })).stages.at(-1),
        "completed",
      )
      const popup = engine.top()
      assert(popup.role !== "map", "a popup over the map")
      const cancel = popup.affordances.find((entry: Value) => entry.role === "cancel")
      assert(cancel, "a cancel affordance")
      const keys = await engine.burst(Array.from({ length: 4 }, () => ({ affordance: cancel.id })))
      assertEquals(keys[0].stages.at(-1), "completed")
      for (const late of keys.slice(1)) assertEquals(late.error, "stale_screen")
      rec.command("key of a burst that met the map after the popup closed", keys[1])
      assertEquals(engine.top().role, "map")

      // A held key is input state, not repeated presses: the client says when it goes down and comes up.
      assert(arrow.holdable, "a step of the map can be held")
      const hold = {
        epoch: engine.mirror.at.epoch,
        screen_id: engine.top().screen_id,
        affordance_id: arrow.id,
      }
      rec.accept("a step of the map the player can hold", "affordance", arrow)
      rec.accept("hold of the east step", "hold_params", hold)
      rec.accept("hold, result", "empty_params", await engine.result("bn.hold", hold))
      rec.accept("release, result", "empty_params", await engine.result("bn.release", {}))
      const wait = engine.top().affordances.find((entry: Value) => entry.id === "wait")
      assert(wait && !wait.holdable, "an action that is not a step cannot be held")
      const refused = await engine.request("bn.hold", { ...hold, affordance_id: wait.id })
      assertEquals(refused.error.data.kind, "invalid_arguments")
      rec.accept(
        "hold of an affordance that is not a step",
        "application_error",
        refused.error.data,
      )
      const gone = await engine.request("bn.hold", { ...hold, screen_id: "screen:closed" })
      assertEquals(gone.error.data.kind, "stale_screen")
      rec.accept("hold aimed at a screen that is not on top", "application_error", gone.error.data)
      rec.reject(
        "hold without the affordance",
        "hold_params",
        without(hold, "affordance_id"),
        "affordance_id is required",
      )
    }),
}
