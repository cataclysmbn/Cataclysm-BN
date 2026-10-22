/**
 * @module
 * The map editor from the main menu: the Editor tab's map editor entry starts a world of its own without a
 * character and opens the editor at once; leaving shows the map, and the editor action comes back to the
 * document that was open.
 */
import { assert, assertEquals } from "@std/assert"
import { Engine, type Scenario, type Value } from "./driver.ts"
import { walk } from "./inventory.ts"

const part = (screen: Value, id: string) => walk(screen.root).find((entry) => entry.id === id)
const path = (screen: Value) => part(screen, "editor:file").fields[0].value

/** A nested chunk of floor. */
const chunk = [{
  type: "mapgen",
  method: "json",
  nested_mapgen_id: "editor_menu_scenario_chunk",
  object: { mapgensize: [2, 2], rows: ["..", ".."], terrain: { ".": "t_floor" } },
}]

export const editorMenu: Scenario = {
  area: "editor_menu",
  run: (rec) =>
    Engine.run(rec, async (engine) => {
      rec.mask(engine.profile, "profile")
      const file = `${engine.profile}/mods/editor_menu_scenario/chunk.json`
      await Deno.mkdir(`${engine.profile}/mods/editor_menu_scenario`, { recursive: true })
      await Deno.writeTextFile(file, JSON.stringify(chunk, null, 2) + "\n")

      await engine.toMainMenu()
      assertEquals((await engine.invoke("menu:tab", { row: "editor" })).stages.at(-1), "completed")
      const tab = engine.top()
      assertEquals(part(tab, "menu:entries").rows.map((row: Value) => row.id), ["editor:map"])
      rec.accept("title screen on the editor tab", "screen", tab)

      const started = await engine.invoke("menu:entry", { row: "editor:map" })
      rec.command("map editor started from the main menu", started)
      await engine.follow(() => engine.stale)
      await engine.subscribe()
      // No character is made: the editor is the first screen of the new world.
      await engine.acknowledge(() => engine.top().kind === "editor")
      assertEquals(engine.mirror.interaction.stack.at(-2).role, "map")
      assertEquals(path(engine.top()), "")

      assertEquals((await engine.invoke("editor:open", { text: file })).stages.at(-1), "completed")
      assertEquals(path(engine.top()), file)
      const left = await engine.invoke("QUIT")
      assertEquals(left.stages.at(-1), "completed")
      rec.command("map editor of the main menu left for the map", left)
      assertEquals(engine.top().role, "map")
      const offered = engine.top().affordances.find((entry: Value) => entry.id === "editor")
      assert(offered.enabled, "the editor world's avatar has the Debug Editor trait")

      const back = await engine.submit({ kind: "action", action_id: "editor" })
      assertEquals(back.stages.at(-1), "completed")
      await engine.follow(() => engine.top().kind === "editor")
      assertEquals(path(engine.top()), file)
      rec.accept("map editor reopened on the document it was left with", "screen", engine.top())
      assertEquals((await engine.invoke("QUIT")).stages.at(-1), "completed")
      assertEquals(engine.top().role, "map")
    }),
}
