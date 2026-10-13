/**
 * @module
 * The NPC trade window: two panes with their headers, category rows and price columns, a count set on a
 * row in the item's own units, a pane filtered by its own text, and the window left by its cancel.
 * The trader is the shopkeeper template the debug menu spawns by id, so the run does not depend on the seed.
 */
import { assert, assertEquals } from "@std/assert"
import { Engine, type Scenario, type Value } from "./driver.ts"

/** The component with `id` in the tree under `root`, `root` included. */
const find = (root: Value, id: string): Value | undefined => {
  if (root.id === id) return root
  for (const child of [root.child, ...root.children ?? []]) {
    const found = child && find(child, id)
    if (found) return found
  }
}

export const trade: Scenario = {
  area: "trade",
  run: (rec) =>
    Engine.run(rec, async (engine) => {
      await engine.toMainMenu()
      await engine.enterTutorial()
      await engine.submit({ kind: "action", action_id: "wait" })
      await engine.choose("entry:0")
      await engine.acknowledge(() => engine.mirror.interaction.interaction === null)

      // Entries are numbered by position: Spawning is the third of the debug menu, and the trade entry the third of Spawning.
      // It places the same shopkeeper template every run and opens its trade.
      assertEquals(
        (await engine.submit({ kind: "action", action_id: "debug" })).stages.at(-1),
        "completed",
      )
      assertEquals((await engine.choose("entry:2")).stages.at(-1), "completed")
      assertEquals((await engine.choose("entry:2")).stages.at(-1), "completed")

      // The window opens after the command that chose it has completed.
      await engine.follow(() => engine.top().context === "NPC_TRADE")
      const window = engine.top()
      assertEquals(window.context, "NPC_TRADE")
      assertEquals(window.role, "screen")
      rec.accept("trade screen", "screen", window)

      const you = find(window.root, "list:you")
      const them = find(window.root, "list:npc")
      assert(you && them, "both panes are lists")
      const balance = find(window.root, "trade:balance")
      assert(balance, "the balance is a header")
      rec.reject(
        "balance tone that is a color",
        "text_component",
        { ...balance, tone: "c_light_red" },
        "tone says good or bad news; colours are the client's",
      )
      assert(them.rows.length === 0 || them.rows[0].header, "a category heading leads the rows")
      const item = [...them.rows, ...you.rows].find((row: Value) => !row.header && row.max >= 1)
      assert(item, "the window lists an item")
      const pane = them.rows.includes(item) ? "list:npc" : "list:you"

      // A count in the item's own units, on a row of either pane.
      const count = await engine.invoke("trade:set_count", { row: item.id, count: 1 })
      assertEquals(count.stages.at(-1), "completed")
      rec.command("count set on an item row", count)
      const picked = find(engine.top().root, pane).rows.find((row: Value) => row.id === item.id)
      assertEquals(picked.selected_count, 1)

      // A pane filtered by its own text hides every row, and the other pane is as it was.
      const filtered = await engine.invoke("trade:filter", {
        pane: "pane:you",
        text: "__no_match__",
      })
      assertEquals(filtered.stages.at(-1), "completed")
      rec.command("player pane filtered", filtered)
      assertEquals(find(engine.top().root, "list:you").rows.length, 0)
      assertEquals(find(engine.top().root, "list:npc").rows.length, them.rows.length)
      assertEquals(
        (await engine.invoke("trade:filter", { pane: "pane:you", text: "" })).stages.at(-1),
        "completed",
      )

      // Cancelling leaves the window and the map is back on top.
      const cancel = await engine.invoke("trade:cancel")
      assertEquals(cancel.stages.at(-1), "completed")
      rec.command("trade cancelled", cancel)
    }),
}
