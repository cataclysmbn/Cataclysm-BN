/**
 * @module
 * The options screen: tabs for its pages over the rows of one, the tooltip and the value editor of the row
 * under the cursor, a value set through the editor (and one it refuses), a group folded open, and the
 * keybindings screen: the actions of a context as rows, a filter, a mode chosen with the registered action
 * and a row picked in it.
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

export const options: Scenario = {
  area: "options",
  run: (rec) =>
    Engine.run(rec, async (engine) => {
      await openSetting(engine, "settings:options")
      const screen = engine.top()
      assertEquals(screen.context, "OPTIONS")
      rec.accept("options screen", "screen", screen)
      const pages = part(engine, "options:pages")
      assert(pages && pages.kind === "tabs" && pages.tabs.length >= 5, "a tab per page")
      assertEquals(pages.select, "options:page")

      // The cursor goes to an option by its name; its editor carries what the native menu would cycle.
      const select = await engine.invoke("options:select", { row: "option:SAFEMODEPROXIMITY" })
      assertEquals(select.stages.at(-1), "completed")
      rec.command("option picked in the options list", select)
      const editor = part(engine, "options:editor")
      assert(editor && editor.kind === "form", "the editor of the option under the cursor")
      assertEquals(editor.fields[0].type, "integer")
      rec.accept("options screen with the editor of an integer option", "screen", engine.top())

      const set = await engine.invoke("options:set", { row: "option:SAFEMODEPROXIMITY", text: "7" })
      assertEquals(set.stages.at(-1), "completed")
      rec.command("option value set through its editor", set)
      assertEquals(part(engine, "options:editor").fields[0].value, "7")
      // A value the editor does not offer changes nothing.
      const over = await engine.invoke("options:set", {
        row: "option:SAFEMODEPROXIMITY",
        text: String(editor.fields[0].max + 1),
      })
      assertEquals(over.stages.at(-1), "completed")
      assertEquals(part(engine, "options:editor").fields[0].value, "7")
      // The editor is the option's own: another row's id is refused.
      const other = await engine.invoke("options:set", { row: "option:SAFEMODE", text: "false" })
      assertEquals(other.error, "invalid_arguments")
      rec.command("value set for an option that is not under the cursor", other)

      const page = await engine.invoke("options:page", { row: "debug" })
      assertEquals(page.stages.at(-1), "completed")
      rec.command("options page picked by its tab", page)
      const group = part(engine, "options:items").rows.find((row: Value) =>
        row.toggled !== undefined
      )
      assert(group, "a group that folds its options")
      const toggled = await engine.invoke("options:activate", { row: group.id })
      assertEquals(toggled.stages.at(-1), "completed")
      rec.command("options group folded open", toggled)
      rec.accept("options screen on a page with an open group", "screen", engine.top())

      // Leaving with a change asks to save it; the answer is No, so nothing reaches the profile.
      assertEquals((await engine.invoke("QUIT")).stages.at(-1), "completed")
      assertEquals((await engine.choose("choice:no")).stages.at(-1), "completed")
      await engine.acknowledge(() => engine.top().role !== "dialog")
    }),
}

export const keybindings: Scenario = {
  area: "keybindings",
  run: (rec) =>
    Engine.run(rec, async (engine) => {
      await openSetting(engine, "settings:keybindings")
      const screen = engine.top()
      assertEquals(screen.context, "HELP_KEYBINDINGS")
      rec.accept("keybindings screen", "screen", screen)
      const actions = part(engine, "keybindings:actions")
      assert(actions && actions.kind === "list" && actions.filterable, "the actions as a list")
      const pick = screen.affordances.find((entry: Value) => entry.id === "keybindings:pick")
      assert(pick && !pick.enabled, "nothing is picked before a mode is chosen")
      rec.accept("pick affordance of the keybindings screen outside a mode", "affordance", pick)

      const filter = await engine.invoke("keybindings:filter", { text: "Move East" })
      assertEquals(filter.stages.at(-1), "completed")
      rec.command("keybindings filtered by text", filter)
      assertEquals(part(engine, "keybindings:actions").rows.map((row: Value) => row.id), [
        "action:RIGHT",
      ])

      // A mode is chosen with the registered action of the screen, and a row picked in it.
      const mode = await engine.invoke("REMOVE")
      assertEquals(mode.stages.at(-1), "completed")
      rec.command("keybindings mode chosen", mode)
      assertEquals(part(engine, "keybindings:window").stats.map((stat: Value) => stat.key), [
        "mode",
      ])
      rec.accept("keybindings screen in the remove mode", "screen", engine.top())
      const unknown = await engine.invoke("keybindings:pick", { row: "action:NOTHING" })
      assertEquals(unknown.error, "invalid_arguments")
      rec.command("keybindings row that the list does not hold", unknown)
      // The mode is left as it was found; the screen is left the way the native one is.
      assertEquals((await engine.invoke("QUIT")).stages.at(-1), "completed")
      assertEquals(part(engine, "keybindings:window").stats, [])
      assertEquals((await engine.invoke("QUIT")).stages.at(-1), "completed")
      await engine.acknowledge(() => engine.top().role !== "dialog")
    }),
}
