/**
 * @module
 * A world: New Game and Tutorial from the main menu with the loading it announces and the resync that
 * follows, the map boundary, a move, and a popup over the map with a key that reaches it too late.
 */
import { assert, assertEquals } from "@std/assert"
import { Engine, type Scenario, type Value, without } from "./driver.ts"

export const tutorial: Scenario = {
  area: "tutorial",
  run: (rec) =>
    Engine.run(rec, async (engine) => {
      await engine.toMainMenu()
      await engine.enterTutorial()
      // The first turns of the tutorial open lessons; let them out of the way.
      await engine.submit({ kind: "action", action_id: "wait" })
      await engine.choose("5 minutes")
      await engine.acknowledge(() => engine.mirror.interaction.interaction === null)

      assert(engine.loading.at(-1)?.done, "loading ends with done")
      rec.accept("loading done", "loading_notification", engine.loading.at(-1))
      assertEquals(engine.resyncs.length, 1)
      rec.accept("resync after the world was replaced", "resync_notification", engine.resyncs[0])

      const map = engine.top()
      assertEquals(map.role, "map")
      rec.accept("boundary of the tutorial map", "interaction_boundary", engine.mirror.interaction)
      rec.accept("map screen", "screen", map)
      rec.reject(
        "map screen without its affordances",
        "screen",
        without(map, "affordances"),
        "a screen lists what it offers, empty or not",
      )

      const arrow = map.affordances.find((entry: Value) =>
        ["RIGHT", "LEFT", "UP", "DOWN"].includes(entry.id)
      )
      assert(arrow, "a movement affordance")
      rec.accept("movement affordance of the map", "affordance", arrow)
      rec.reject(
        "affordance without its expectation scope",
        "affordance",
        without(arrow, "expect"),
        "the engine assigns the scope; a client does not guess it",
      )

      const before = engine.events.length
      const move = await engine.invoke(arrow.id)
      rec.command("move invoked at the map", move)
      // One event of each type the move produced.
      const seen = new Set<string>()
      for (
        const event of engine.events.slice(before).filter((event) => event.command === move.id)
      ) {
        if (seen.has(event.type)) continue
        seen.add(event.type)
        rec.accept(`${event.type} event after a move`, "event", event)
      }

      // The wait menu is a screen over the map.
      assertEquals(
        (await engine.submit({ kind: "action", action_id: "wait" })).stages.at(-1),
        "completed",
      )
      const popup = engine.top()
      assert(popup.role !== "map", "a screen over the map")
      rec.accept(
        "boundary of the map under the wait menu",
        "interaction_boundary",
        engine.mirror.interaction,
      )
      rec.accept("wait menu screen", "screen", popup)
      const cancel = popup.affordances.find((entry: Value) => entry.role === "cancel")
      assert(cancel, "a cancel affordance")
      rec.accept("cancel affordance of a popup", "affordance", cancel)
      rec.command("cancel invoked at the wait menu", await engine.invoke(cancel.id))
      assertEquals(engine.top().role, "map")

      // The same key again, aimed at the popup that has closed.
      const late = await engine.submit(
        { kind: "invoke", affordance_id: cancel.id },
        { screen_id: popup.screen_id },
      )
      assertEquals(late.error, "stale_screen")
      rec.command("cancel aimed at a popup that has closed", late)
    }),
}
