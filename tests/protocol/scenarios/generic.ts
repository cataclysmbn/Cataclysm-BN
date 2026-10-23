/**
 * @module
 * The screens every uilist, query popup and text field shares: a query popup over the map, the debug menu
 * as a filterable list, its filter prompt showing the rows that are left as they are typed, and a list of
 * switches that say whether each one is on.
 */
import { assert, assertEquals } from "@std/assert"
import { Engine, type Scenario, type Value } from "./driver.ts"

const rows = (screen: Value): Value[] =>
  screen.root.rows ?? screen.root.children.flatMap((part: Value) => part.rows ?? [])
const part = (screen: Value, kind: string): Value =>
  screen.root.kind === kind
    ? screen.root
    : screen.root.children.find((part: Value) => part.kind === kind)

export const generic: Scenario = {
  area: "generic",
  run: (rec) =>
    Engine.run(rec, async (engine) => {
      await engine.toMainMenu()
      await engine.enterTutorial()
      await engine.submit({ kind: "action", action_id: "wait" })
      await engine.choose("entry:0")
      await engine.acknowledge(() => engine.mirror.interaction.interaction === null)

      // A query popup: a question with its options, over the map.
      assertEquals(
        (await engine.submit({ kind: "action", action_id: "save" })).stages.at(-1),
        "completed",
      )
      // The first popups are lessons of the tutorial; the question comes after them.
      for (let lessons = 0; engine.top().context === "POPUP_WAIT" && lessons < 5; lessons++) {
        await engine.invoke(
          engine.top().affordances.find((entry: Value) => entry.role === "cancel").id,
        )
      }
      const query = engine.top()
      assertEquals(query.context, "YESNO")
      assert(query.role !== "map", "a popup over the map")
      rec.accept("query popup screen", "screen", query)
      const no = query.affordances.find((entry: Value) => entry.id === "NO")
      assert(no, "a question can be declined")
      rec.command("query popup declined", await engine.invoke(no.id))
      assertEquals(engine.top().role, "map")

      // The debug menu is a filterable list.
      assertEquals(
        (await engine.submit({ kind: "action", action_id: "debug" })).stages.at(-1),
        "completed",
      )
      const debug = engine.top()
      const list = part(debug, "list")
      assertEquals(list.filterable, true)
      assert(list.affordances.includes("FILTER"), "the list names its filter")
      rec.accept("filterable list screen", "screen", debug)

      // Its filter prompt is a text field that shows the rows left by the text typed so far.
      const all = rows(debug).length
      assertEquals((await engine.invoke("FILTER")).stages.at(-1), "completed")
      const prompt = engine.top()
      assertEquals(rows(prompt).length, all, "the prompt starts with every row")
      rec.accept("filter prompt before typing", "screen", prompt)
      const fill = prompt.affordances.find((entry: Value) =>
        entry.args?.text && entry.role !== "confirm"
      )
      assert(fill, "a prompt takes typing")
      const typed = await engine.invoke(fill.id, { text: "tele" })
      assertEquals(typed.stages.at(-1), "completed")
      const filtered = engine.top()
      assert(rows(filtered).length > 0 && rows(filtered).length < all, "the rows follow the text")
      rows(filtered).forEach((row: Value) => assert(/tele/i.test(row.label), row.label))
      rec.command("filter text typed", typed)
      rec.accept("filter prompt after typing", "screen", filtered)
      const decline = filtered.affordances.find((entry: Value) => entry.role === "cancel")
      assert(decline, "a prompt can be cancelled")
      rec.command("filter prompt cancelled", await engine.invoke(decline.id))

      // A submenu of switches says which are on.
      const info = rows(engine.top()).find((row: Value) => /^Info/.test(row.label))
      assert(info, "the debug menu has an Info submenu")
      const choose = engine.top().affordances.find((entry: Value) =>
        entry.args?.row && entry.id.startsWith("bn:choose")
      )
      assert(choose, "a list can be chosen from")
      assertEquals((await engine.invoke(choose.id, { row: info.id })).stages.at(-1), "completed")
      const switches = engine.top()
      assert(
        rows(switches).some((row: Value) => row.toggled === false),
        "a switch that is off says so",
      )
      rec.accept("list of switches screen", "screen", switches)

      // The keys a list's handler tests itself are buttons of the screen, never rows to pick.
      const leave = (screen: Value) =>
        screen.affordances.find((entry: Value) => entry.role === "cancel")
      const pick = async (label: RegExp) => {
        const top = engine.top()
        const target = rows(top).find((row: Value) => label.test(row.label))
        assert(target, `a row ${label}`)
        const chooser = top.affordances.find((entry: Value) =>
          entry.args?.row && entry.id.startsWith("bn:choose")
        )
        assertEquals(
          (await engine.invoke(chooser.id, { row: target.id })).stages.at(-1),
          "completed",
        )
      }
      await engine.invoke(leave(engine.top()).id)
      await pick(/^Spawning/)
      await pick(/^Spawn an item/)
      const spawner = engine.top()
      const keys = spawner.affordances.filter((entry: Value) => entry.id.startsWith("bn:key:"))
      assert(keys.length >= 3, "the item spawner offers its keys")
      assert(
        keys.some((entry: Value) =>
          entry.label === "Choose a container" && entry.keys.length === 1
        ),
        "a key is named and bound",
      )
      assert(
        rows(spawner).every((row: Value) => !/^(action|bn):key:/.test(row.id)),
        "no key is a row",
      )
      const narrow = spawner.affordances.find((entry: Value) => entry.id.startsWith("bn:filter:"))
      assert(narrow, "the spawner takes a filter")
      assertEquals((await engine.invoke(narrow.id, { text: "shirt" })).stages.at(-1), "completed")
      const found = engine.top()
      assert(rows(found).length > 0, "the filter leaves results")
      rows(found).forEach((row: Value) => assert(/shirt/i.test(row.label), row.label))
      assertEquals(
        found.affordances.filter((entry: Value) => entry.id.startsWith("bn:key:")).length,
        keys.length,
        "the keys stay while the list is filtered",
      )
      rec.accept("list with key buttons screen", "screen", found)
      const toggle = keys.find((entry: Value) => entry.label === "Toggle spawning everything")
      assert(toggle, "the spawner toggles spawning everything")
      rec.command("list key pressed", await engine.invoke(toggle.id))
    }),
}
