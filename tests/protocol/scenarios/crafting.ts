/**
 * @module
 * The crafting and construction screens on the generic path: tabs, recipe rows and the detail panel with
 * its requirement lists, a search, a batch, and the refusals of a stale row.
 */
import { assert, assertEquals } from "@std/assert"
import { Engine, type Scenario, type Value } from "./driver.ts"

/** The first component of the tree with `id`. */
const find = (node: Value, id: string): Value | undefined => {
  if (!node || typeof node !== "object") return undefined
  if (node.id === id) return node
  for (const child of [...(node.children ?? []), ...(node.child ? [node.child] : [])]) {
    const found = find(child, id)
    if (found) return found
  }
  return undefined
}

export const crafting: Scenario = {
  area: "crafting",
  run: (rec) =>
    Engine.run(rec, async (engine) => {
      await engine.toMainMenu()
      await engine.enterTutorial()
      await engine.submit({ kind: "action", action_id: "wait" })
      await engine.choose("entry:0")
      await engine.acknowledge(() => engine.mirror.interaction.interaction === null)

      const opened = await engine.submit({ kind: "action", action_id: "craft" })
      assertEquals(opened.stages.at(-1), "completed")
      const screen = engine.top()
      assert(screen.root, "the crafting screen publishes a component tree")
      assert(find(screen.root, "crafting:recipes"), "a recipe list")
      rec.accept("Crafting screen", "screen", screen)

      // The first tab can be empty; a tab other than the selected one lists recipes of its own.
      const tabs = find(screen.root, "crafting:tabs")
      let tab: Value
      for (const other of tabs.tabs.filter((entry: Value) => !entry.selected)) {
        tab = await engine.invoke("craft:tab", { row: other.id })
        assertEquals(tab.stages.at(-1), "completed")
        if (find(engine.top().root, "crafting:recipes").rows.length > 0) break
      }
      assert(find(engine.top().root, "crafting:recipes").rows.length > 0, "a tab with recipes")
      assert(find(engine.top().root, "crafting:detail"), "the detail of the row under the cursor")
      rec.command("a tab chosen", tab)

      // A search replaces the tabs and the list with its matches.
      const word = find(engine.top().root, "crafting:recipes").rows[0]?.label.split(" ")[0] ?? "a"
      const search = await engine.invoke("craft:search", { text: word })
      assertEquals(search.stages.at(-1), "completed")
      rec.command("a search", search)
      assertEquals(find(engine.top().root, "crafting:tabs").tabs.map((entry: Value) => entry.id), [
        "search",
      ])
      assertEquals((await engine.invoke("craft:search", { text: "" })).stages.at(-1), "completed")

      // Selecting a row moves the cursor and the detail with it, without crafting.
      const listed = find(engine.top().root, "crafting:recipes").rows
      const select = await engine.invoke("craft:select", { row: listed.at(-1).id })
      assertEquals(select.stages.at(-1), "completed")
      rec.command("a row selected", select)
      assertEquals(engine.top().focus?.row_id, listed.at(-1).id)

      // A row that is not there is refused.
      const missing = await engine.invoke("craft:craft", { row: "no_such_recipe" })
      rec.command("a row that is not in the list", missing)

      // The batch list of the recipe under the cursor.
      const row = find(engine.top().root, "crafting:recipes").rows.find((entry: Value) =>
        entry.enabled && entry.max !== undefined
      )
      if (row) {
        const batch = await engine.invoke("craft:batch", { row: row.id, count: 2 })
        assertEquals(batch.stages.at(-1), "completed", JSON.stringify(batch))
        rec.command("a batch of two", batch)
        assertEquals(find(engine.top().root, "crafting:tabs").tabs[0].id, "batch")
      }
      const cancel = engine.top().affordances.find((entry: Value) => entry.role === "cancel")
      assert(cancel, "a cancel affordance")
      const left = await engine.invoke(cancel.id)
      assertEquals(left.stages.at(-1), "completed", JSON.stringify(left))
      await engine.acknowledge(() => engine.top().role === "map")
    }),
}

export const construction: Scenario = {
  area: "construction",
  run: (rec) =>
    Engine.run(rec, async (engine) => {
      await engine.toMainMenu()
      await engine.enterTutorial()
      await engine.submit({ kind: "action", action_id: "wait" })
      await engine.choose("entry:0")
      await engine.acknowledge(() => engine.mirror.interaction.interaction === null)

      const opened = await engine.submit({ kind: "action", action_id: "construct" })
      assertEquals(opened.stages.at(-1), "completed")
      const screen = engine.top()
      assert(screen.root, "the construction screen publishes a component tree")
      assert(find(screen.root, "construction:groups"), "a list of constructions")
      rec.accept("Construction screen", "screen", screen)

      // The first category can be empty; another one lists constructions of its own.
      const categories = find(screen.root, "construction:tabs").tabs
      for (const other of categories.filter((entry: Value) => !entry.selected)) {
        if (find(engine.top().root, "construction:groups").rows.length > 0) break
        const hop = await engine.invoke("construct:tab", { row: other.id })
        assertEquals(hop.stages.at(-1), "completed", JSON.stringify(hop))
      }
      assert(
        find(engine.top().root, "construction:groups").rows.length > 0,
        "a category with constructions",
      )

      const search = await engine.invoke("construct:search", { text: "wall" })
      assertEquals(search.stages.at(-1), "completed")
      rec.command("a construction search", search)
      const missing = await engine.invoke("construct:construct", { row: "no_such_group" })
      rec.command("a construction row that is not in the list", missing)
      const cancel = engine.top().affordances.find((entry: Value) => entry.role === "cancel")
      assert(cancel, "a cancel affordance")
      const left = await engine.invoke(cancel.id)
      assertEquals(left.stages.at(-1), "completed", JSON.stringify(left))
    }),
}
