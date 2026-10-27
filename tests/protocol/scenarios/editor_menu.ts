/**
 * @module
 * The map editor from the main menu: the Editor tab's map editor entry starts a world of its own without a
 * character and opens the editor as the whole view at once; leaving ends that world and returns to the main
 * menu.
 */
import { assertEquals } from "@std/assert"
import { Engine, type Scenario, type Value } from "./driver.ts"
import { walk } from "./inventory.ts"
import { atMainMenu } from "../mirror.ts"

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
      assertEquals(part(tab, "menu:entries").rows.map((row: Value) => row.id), [
        "editor:map",
        "editor:vehicle",
        "editor:save",
      ])
      rec.accept("title screen on the editor tab", "screen", tab)

      // The mod list comes first; leaving it and answering no returns to the menu with no world.
      assertEquals(
        (await engine.invoke("menu:entry", { row: "editor:map" })).stages.at(-1),
        "completed",
      )
      assertEquals(engine.top().kind, "world_creation")
      rec.accept("mod list of the main menu's editor", "screen", engine.top())
      assertEquals((await engine.invoke("QUIT")).stages.at(-1), "completed")
      assertEquals((await engine.invoke("NO")).stages.at(-1), "completed")
      assertEquals(engine.top().kind, "main_menu")

      assertEquals(
        (await engine.invoke("menu:entry", { row: "editor:map" })).stages.at(-1),
        "completed",
      )
      assertEquals((await engine.invoke("QUIT")).stages.at(-1), "completed")
      const started = await engine.invoke("YES")
      rec.command("map editor started from the main menu with the chosen mods", started)
      await engine.follow(() => engine.stale)
      await engine.subscribe()
      // No character is made: the editor is the whole view of the new world.
      await engine.acknowledge(() => engine.top().kind === "editor")
      assertEquals(engine.top().role, "workspace")
      assertEquals(path(engine.top()), "")

      assertEquals((await engine.invoke("editor:open", { text: file })).stages.at(-1), "completed")
      assertEquals(path(engine.top()), file)
      rec.accept("map editor of the main menu with a document open", "screen", engine.top())
      // Leaving ends the editor's world, so the command ends with it and the main menu comes back.
      const left = await engine.invoke("QUIT")
      assertEquals(left.stages.at(-1), "interrupted")
      rec.command("map editor of the main menu left for the main menu", left)
      await engine.follow(() => engine.stale)
      await engine.subscribe()
      await engine.acknowledge(() => atMainMenu(engine.mirror))
    }),
}
