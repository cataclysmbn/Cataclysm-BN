/**
 * @module
 * The inventory selector the drop action opens: a panel per column under category headings, a stack
 * chosen and given a count through the screen's own affordances, a row the list does not hold, and the
 * way out.
 */
import { assert, assertEquals } from "@std/assert"
import { Engine, type Scenario, type Value } from "./driver.ts"

/** The components of a tree, outermost first. */
export const walk = (component: Value): Value[] => [
  component,
  ...[...(component.children ?? []), ...(component.child ? [component.child] : [])].flatMap(walk),
]

/** The row of the list on top whose label is `label`, chosen through the list's own affordance. */
const chooseRow = async (engine: Engine, label: string) => {
  const list = walk(engine.top().root).find((component) => component.kind === "list")
  const row = list?.rows.find((entry: Value) => entry.label === label)
  assert(row, `no row "${label}"`)
  assertEquals((await engine.invoke(list.activate, { row: row.id })).stages.at(-1), "completed")
}

/**
 * Puts one `item` at the avatar's feet through the debug menu's wish: the tutorial avatar starts with
 * nothing to carry or drop. The menu is left for the map afterwards.
 */
export const wish = async (engine: Engine, item: string) => {
  assertEquals(
    (await engine.submit({ kind: "action", action_id: "debug" })).stages.at(-1),
    "completed",
  )
  await chooseRow(engine, "Spawning…")
  await chooseRow(engine, "Spawn an item")
  assertEquals((await engine.invoke("FILTER")).stages.at(-1), "completed")
  const confirm = engine.top().affordances.find((entry: Value) => entry.role === "confirm")
  assert(confirm, "the filter prompt confirms")
  assertEquals((await engine.invoke(confirm.id, { text: item })).stages.at(-1), "completed")
  await chooseRow(engine, item)
  const amount = engine.top().affordances.find((entry: Value) => entry.role === "confirm")
  assert(amount, "the amount prompt confirms")
  assertEquals((await engine.invoke(amount.id, { text: "1" })).stages.at(-1), "completed")
  for (let step = 0; engine.top().role !== "map" && step < 5; step++) {
    const cancel = engine.top().affordances.find((entry: Value) => entry.role === "cancel")
    assert(cancel, "a way out of the wish")
    assertEquals((await engine.invoke(cancel.id)).stages.at(-1), "completed")
  }
  assertEquals(engine.top().role, "map")
}

export const inventory: Scenario = {
  area: "inventory",
  run: (rec) =>
    Engine.run(rec, async (engine) => {
      await engine.toMainMenu()
      await engine.enterTutorial()
      // The first turns of the tutorial open lessons; let them out of the way.
      await engine.submit({ kind: "action", action_id: "wait" })
      await engine.choose("entry:0")
      await engine.acknowledge(() => engine.mirror.interaction.interaction === null)

      // The wish lands the item at the avatar's feet, where the wield selector offers it.
      await wish(engine, "rock")
      assertEquals((await engine.invoke("wield")).stages.at(-1), "completed")
      const picker = walk(engine.top().root).filter((component) => component.kind === "list")
        .find((component) => component.rows.some((row: Value) => row.label.startsWith("rock")))
      assert(picker, "the wield selector lists the rock")
      const rock = picker.rows.find((row: Value) => row.label.startsWith("rock"))
      assertEquals(
        (await engine.invoke(picker.activate, { row: rock.id })).stages.at(-1),
        "completed",
      )
      assertEquals(engine.top().role, "map")
      const opened = await engine.invoke("drop")
      assertEquals(opened.stages.at(-1), "completed", JSON.stringify(opened))
      const screen = engine.top()
      assertEquals(screen.role, "screen", JSON.stringify(screen).slice(0, 1500))
      rec.accept("drop selector screen", "screen", screen)

      const lists = walk(screen.root).filter((component) => component.kind === "list")
      assert(lists.length > 0, "a column")
      const list = lists[0]
      assertEquals(list.rows[0].header, true, "the first row of a column is its category heading")
      rec.accept("category heading of the drop selector", "row", list.rows[0])
      const stack = list.rows.find((row: Value) => !row.header && row.selectable)
      assert(stack, "a stack that can be chosen")
      assertEquals(stack.selected, false)
      rec.accept("stack of the drop selector, not yet chosen", "row", stack)

      const choose = screen.affordances.find((entry: Value) => entry.id === list.activate)
      assert(choose, "the list's choose affordance")
      assertEquals(choose.expect, "boundary")
      rec.accept("choose affordance of the drop selector", "affordance", choose)
      const chosen = await engine.invoke(choose.id, { row: stack.id })
      assertEquals(chosen.stages.at(-1), "completed", JSON.stringify(chosen))
      rec.command("stack chosen in the drop selector", chosen)
      const rowAfter = () =>
        walk(engine.top().root).filter((component) => component.kind === "list")
          .flatMap((component) => component.rows).find((row: Value) => row.id === stack.id)
      assertEquals(rowAfter()?.selected, true, "the chosen stack is marked")

      const count = engine.top().affordances.find((entry: Value) => entry.id === `${list.id}:count`)
      assert(count, "a count affordance")
      const counted = await engine.invoke(count.id, { row: stack.id, count: 1 })
      assertEquals(counted.stages.at(-1), "completed", JSON.stringify(counted))
      rec.command("stack given a count in the drop selector", counted)
      assertEquals(rowAfter()?.selected_count, 1)

      const unknown = await engine.invoke(choose.id, { row: "item:nothing" })
      assertEquals(unknown.error, "invalid_arguments")
      rec.command("choose of a row the list does not hold", unknown)

      const cancel = engine.top().affordances.find((entry: Value) => entry.role === "cancel")
      assert(cancel, "a cancel affordance")
      rec.accept("cancel affordance of the drop selector", "affordance", cancel)
      const left = await engine.invoke(cancel.id)
      assertEquals(left.stages.at(-1), "completed", JSON.stringify(left))
      rec.command("drop selector left", left)
      await engine.acknowledge(() => engine.top().role === "map")
    }),
}
