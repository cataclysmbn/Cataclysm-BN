/**
 * @module
 * The armor layers screen of the tutorial avatar, a body part and a worn item picked by their own affordances and
 * an item grabbed to be moved by registered actions; and the item examine menu the inventory opens, an action
 * picked through its list.
 */
import { assert, assertEquals } from "@std/assert"
import { Engine, type Scenario, type Value } from "./driver.ts"
import { walk, wish } from "./inventory.ts"

const part = (screen: Value, id: string) => walk(screen.root).find((entry) => entry.id === id)

/** Leaves the screens above the map. */
const close = async (engine: Engine) => {
  for (let step = 0; engine.top().role !== "map" && step < 4; step++) {
    const cancel = engine.top().affordances.find((entry: Value) => entry.role === "cancel")
    assert(cancel, "a way out")
    await engine.invoke(cancel.id)
  }
  assertEquals(engine.top().role, "map")
}

/** Wishes `item` to the avatar's feet and wears it, picked in the wear selector. */
export const wear = async (engine: Engine, item: string) => {
  await wish(engine, item)
  assertEquals((await engine.invoke("wear")).stages.at(-1), "completed")
  const list = walk(engine.top().root).find((component) =>
    component.kind === "list" && component.rows.some((row: Value) => row.label.includes(item))
  )
  const row = list?.rows.find((entry: Value) => entry.label.includes(item))
  assert(list && row, `the wear selector lists ${item}`)
  assertEquals((await engine.invoke(list.activate, { row: row.id })).stages.at(-1), "completed")
  await engine.acknowledge(() => engine.top().role === "map")
}

export const armor: Scenario = {
  area: "armor",
  run: (rec) =>
    Engine.run(rec, async (engine) => {
      await engine.toMainMenu()
      await engine.enterTutorial()
      await engine.submit({ kind: "action", action_id: "wait" })
      await engine.choose("entry:0")
      await engine.acknowledge(() => engine.mirror.interaction.interaction === null)

      for (const item of ["t-shirt", "jeans", "hoodie"]) await wear(engine, item)

      // The layers of what the avatar wears.
      assertEquals(
        (await engine.submit({ kind: "action", action_id: "sort_armor" })).stages.at(-1),
        "completed",
      )
      const layers = engine.top()
      assertEquals(layers.context, "SORT_ARMOR")
      rec.accept("armor layers screen", "screen", layers)
      const worn = part(layers, "armor:worn:rows")
      assert(worn && worn.rows.length > 1, "worn items")
      assertEquals(layers.focus_row ?? layers.focus?.row_id, worn.rows[0].id)
      assert(part(layers, "armor:detail"), "the focused item's details")
      assert(part(layers, "armor:encumbrance:rows").rows.length > 0, "the encumbrance table")

      const second = worn.rows[1]
      const picked = await engine.invoke("armor:pick", { row: second.id })
      assertEquals(picked.stages.at(-1), "completed", JSON.stringify(picked))
      rec.command("worn item picked", picked)
      assertEquals(engine.top().focus_row ?? engine.top().focus?.row_id, second.id)
      const unknownItem = await engine.invoke("armor:pick", { row: "worn:nothing:0" })
      assertEquals(unknownItem.error, "invalid_arguments")
      rec.command("pick of a worn item the list does not hold", unknownItem)

      const torso = part(engine.top(), "armor:tabs").tabs.find((tab: Value) =>
        tab.id === "part:torso"
      )
      assert(torso, "the torso among the body parts")
      const filtered = await engine.invoke("armor:part", { row: torso.id })
      assertEquals(filtered.stages.at(-1), "completed", JSON.stringify(filtered))
      rec.command("body part picked", filtered)
      assert(
        part(engine.top(), "armor:tabs").tabs.find((tab: Value) => tab.id === torso.id).selected,
      )
      rec.accept("armor layers screen of the torso", "screen", engine.top())
      const unknownPart = await engine.invoke("armor:part", { row: "part:nowhere" })
      assertEquals(unknownPart.error, "invalid_arguments")
      rec.command("pick of a body part the tabs do not hold", unknownPart)

      const grabbed = await engine.invoke("MOVE_ARMOR")
      assertEquals(grabbed.stages.at(-1), "completed", JSON.stringify(grabbed))
      rec.command("worn item grabbed to be moved", grabbed)
      assert(
        part(engine.top(), "armor:worn:rows").rows.some((row: Value) => row.selected),
        "the grabbed item is marked",
      )
      await engine.invoke("MOVE_ARMOR")
      await close(engine)

      // The examine menu of an item of the inventory.
      assertEquals(
        (await engine.submit({ kind: "action", action_id: "inventory" })).stages.at(-1),
        "completed",
      )
      const list = walk(engine.top().root).find((component) => component.kind === "list")
      const row = list?.rows.find((entry: Value) => !entry.header && entry.selectable)
      assert(list && row, "an item to examine")
      assertEquals((await engine.invoke(list.activate, { row: row.id })).stages.at(-1), "completed")
      const examine = engine.top()
      assertEquals(examine.context, "INVENTORY_ITEM")
      rec.accept("item examine menu", "screen", examine)
      const actions = part(examine, "examine:actions")
      assert(actions && actions.rows.length > 0, "the item's actions")
      assert(part(examine, "examine:info"), "the item's information")
      const pick = examine.affordances.find((entry: Value) => entry.id === "examine:select")
      assert(pick, "the select of an action")
      rec.accept("select affordance of the item examine menu", "affordance", pick)
      const unknownAction = await engine.invoke("examine:run", { row: "NOT_AN_ACTION" })
      assertEquals(unknownAction.error, "invalid_arguments")
      rec.command("run of an action the menu does not hold", unknownAction)
      const next = actions.rows[1]
      const selected = await engine.invoke("examine:select", { row: next.id })
      assertEquals(selected.stages.at(-1), "completed", JSON.stringify(selected))
      rec.command("action selected in the item examine menu", selected)
      assertEquals(engine.top().context, "INVENTORY_ITEM")
      assertEquals(engine.top().focus?.row_id ?? engine.top().focus_row, next.id)
      const favorite = actions.rows.find((entry: Value) => entry.id === "FAVORITE_ADD")
      assert(favorite, "the favorite action")
      const done = await engine.invoke("examine:run", { row: favorite.id })
      assertEquals(done.stages.at(-1), "completed", JSON.stringify(done))
      rec.command("favorite action run in the item examine menu", done)
      await close(engine)
    }),
}
