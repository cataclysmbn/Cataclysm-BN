/**
 * @module
 * A save opened in editor mode from the main menu: a new game is saved and left, the Editor tab's `editor:save`
 * lists the save, and choosing it loads the game with the editor on top of the map the avatar stands on, where the
 * save put it.
 */
import { assert, assertEquals } from "@std/assert"
import { Engine, type Scenario, type Value } from "./driver.ts"

const completed = (outcome: { stages: string[] }) =>
  assertEquals(outcome.stages.at(-1), "completed")

export const editorSave: Scenario = {
  area: "editor_save",
  run: (rec) =>
    Engine.run(rec, async (engine) => {
      await engine.toMainMenu()
      completed(await engine.invoke("menu:tab", { row: "new_game" }))
      await engine.invoke("menu:entry", { row: "new_game:play_now_default" })
      await engine.follow(() => engine.stale)
      await engine.subscribe()
      await engine.acknowledge(() => engine.mirror.interaction.interaction === null)
      assertEquals(engine.top().role, "map")
      const step = engine.top().affordances.find((entry: Value) => entry.id === "RIGHT")
      assert(step, "the map offers a step")
      completed(await engine.invoke("RIGHT"))
      const saved = structuredClone(engine.mirror.avatar.at)
      assert(saved, "the avatar has a position")

      // Save and quit leaves the game for the main menu.
      completed(await engine.submit({ kind: "action", action_id: "save" }))
      assertEquals(engine.top().context, "YESNO")
      await engine.invoke("YES")
      await engine.follow(() => engine.stale)
      await engine.subscribe()
      await engine.acknowledge(() => engine.top().kind === "main_menu")

      completed(await engine.invoke("menu:tab", { row: "editor" }))
      completed(await engine.invoke("menu:entry", { row: "editor:save" }))
      const list = engine.mirror.interaction.interaction
      assert(list, "the saves are a list")
      rec.accept("saves to open in the editor", "screen", engine.top())
      const opened = await engine.choose(list.choices[0].id)
      rec.command("save chosen to open in the editor", opened)
      await engine.follow(() => engine.stale)
      await engine.subscribe()
      await engine.acknowledge(() => engine.top().kind === "editor")

      assertEquals(engine.top().kind, "editor")
      assertEquals(engine.mirror.interaction.stack.at(-2).role, "map")
      assertEquals(engine.mirror.avatar.at, saved)
      completed(await engine.invoke("QUIT"))
      assertEquals(engine.top().role, "map")
      assertEquals(engine.mirror.avatar.at, saved)
    }),
}
