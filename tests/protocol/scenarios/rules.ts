/**
 * @module
 * The distraction manager: a row per distraction with its switch, the cursor moved and a distraction switched
 * off by its token (and a row the list does not hold, refused). The colour manager: a row per colour with its
 * customs, a column picked by its token, a custom colour set through the editor beside the list (and a colour it
 * does not offer, refused), and the save question answered No.
 */
import { assert, assertEquals } from "@std/assert"
import { Engine, type Scenario, type Value } from "./driver.ts"
import { walk } from "./inventory.ts"

const part = (engine: Engine, id: string) =>
  walk(engine.top().root).find((entry) => entry.id === id)

/** Settings from the main menu: the tab, then the entry. */
const openSetting = async (engine: Engine, entry: string) => {
  await engine.toMainMenu()
  assertEquals((await engine.invoke("menu:tab", { row: "settings" })).stages.at(-1), "completed")
  assertEquals((await engine.invoke("menu:entry", { row: entry })).stages.at(-1), "completed")
}

export const distractions: Scenario = {
  area: "distractions",
  run: (rec) =>
    Engine.run(rec, async (engine) => {
      await openSetting(engine, "settings:distractions")
      const screen = engine.top()
      assertEquals(screen.context, "DISTRACTION_MANAGER")
      rec.accept("distraction manager", "screen", screen)
      const rows = part(engine, "distractions:items").rows
      assert(rows.length > 1 && rows.every((row: Value) => row.toggled === true), "all switched on")

      const select = await engine.invoke("distractions:select", { row: rows[1].id })
      assertEquals(select.stages.at(-1), "completed")
      rec.command("distraction picked in the list", select)
      assertEquals(engine.top().focus.row_id, rows[1].id)

      const toggle = await engine.invoke("distractions:toggle", { row: rows[1].id })
      assertEquals(toggle.stages.at(-1), "completed")
      rec.command("distraction switched off", toggle)
      const switched = part(engine, "distractions:items").rows[1]
      assertEquals(switched.toggled, false)
      rec.accept("distraction manager with a distraction switched off", "screen", engine.top())

      const unknown = await engine.invoke("distractions:toggle", { row: "distraction:Nothing" })
      assertEquals(unknown.error, "invalid_arguments")
      rec.command("distraction that the list does not hold", unknown)

      // Leaving saves at once, as the native screen does.
      assertEquals((await engine.invoke("QUIT")).stages.at(-1), "completed")
      await engine.acknowledge(() => engine.top().role !== "dialog")
    }),
}

export const colors: Scenario = {
  area: "colors",
  run: (rec) =>
    Engine.run(rec, async (engine) => {
      await openSetting(engine, "settings:colors")
      const screen = engine.top()
      assertEquals(screen.context, "COLORS")
      rec.accept("colour manager", "screen", screen)
      const columns = part(engine, "colors:columns")
      assert(columns && columns.kind === "tabs", "the columns the cursor can be in")
      assertEquals(columns.tabs.map((tab: Value) => tab.id), ["normal", "invert"])
      assertEquals(columns.select, "colors:column")
      const list = part(engine, "colors:items")
      assert(list.total > 100, "a row per colour")
      const rows = list.rows

      const column = await engine.invoke("colors:column", { row: "invert" })
      assertEquals(column.stages.at(-1), "completed")
      rec.command("colour column picked by its token", column)
      assert(part(engine, "colors:columns").tabs[1].selected, "the invert column")

      const select = await engine.invoke("colors:select", { row: rows[1].id })
      assertEquals(select.stages.at(-1), "completed")
      rec.command("colour picked in the list", select)
      const editor = part(engine, "colors:editor")
      assertEquals(editor.fields[0].id, rows[1].id)
      assertEquals(editor.fields[0].type, "select")

      const set = await engine.invoke("colors:set", { row: rows[1].id, text: rows[0].label })
      assertEquals(set.stages.at(-1), "completed")
      rec.command("custom colour set through the editor", set)
      assertEquals(part(engine, "colors:editor").fields[0].value, rows[0].label)
      assertEquals(
        part(engine, "colors:items").rows[1].columns.find((entry: Value) =>
          entry.key === "invert_custom"
        ).value,
        rows[0].label,
      )
      rec.accept("colour manager with a custom colour set", "screen", engine.top())

      // The editor offers the colours of the list and nothing else, and only for the colour under the cursor.
      const foreign = await engine.invoke("colors:set", { row: rows[1].id, text: "no such colour" })
      assertEquals(foreign.error, "invalid_arguments")
      rec.command("custom colour that the editor does not offer", foreign)
      const other = await engine.invoke("colors:set", { row: rows[2].id, text: rows[0].label })
      assertEquals(other.error, "invalid_arguments")
      rec.command("custom colour for a colour that is not under the cursor", other)

      // Leaving with a change asks to save it; the answer is No, so nothing reaches the profile.
      assertEquals((await engine.invoke("QUIT")).stages.at(-1), "completed")
      assertEquals((await engine.choose("option:1")).stages.at(-1), "completed")
      await engine.acknowledge(() => engine.top().role !== "dialog")
    }),
}
