/**
 * @module
 * The advanced inventory: two panes as panels with the areas beside them, an area picked, a filter typed
 * and cleared, the screen closed, and a stack moved between the panes by the rows an affordance names.
 */
import { assert, assertEquals } from "@std/assert"
import { Engine, type Scenario, type Value } from "./driver.ts"

const find = (component: Value, id: string): Value | undefined =>
  component.id === id
    ? component
    : (component.children ?? (component.child ? [component.child] : []))
      .map((child: Value) => find(child, id)).find(Boolean)

export const open = async (engine: Engine) => {
  assertEquals(
    (await engine.submit({ kind: "action", action_id: "advinv" })).stages.at(-1),
    "completed",
  )
  const screen = engine.top()
  assertEquals(screen.context, "ADVANCED_INVENTORY")
  return screen
}

/** The squares around the player: areas that need no container or grabbed vehicle. */
const squares = [
  "area:SW",
  "area:S",
  "area:SE",
  "area:W",
  "area:E",
  "area:NW",
  "area:N",
  "area:NE",
]

export const aim: Scenario = {
  area: "aim",
  run: (rec) =>
    Engine.run(rec, async (engine) => {
      await engine.toMainMenu()
      await engine.enterTutorial()
      await engine.submit({ kind: "action", action_id: "wait" })
      await engine.choose("entry:0")
      await engine.acknowledge(() => engine.mirror.interaction.interaction === null)

      const screen = await open(engine)
      assertEquals(find(screen.root, "aim:panes").kind, "split")
      for (const pane of ["aim:pane:left", "aim:pane:right"]) {
        assertEquals(find(screen.root, pane).kind, "panel")
        assertEquals(find(screen.root, `${pane}:items`).kind, "list")
      }
      rec.accept("advanced inventory screen", "screen", screen)

      const pane = find(screen.root, "aim:pane:left")
      assert(pane.stats.length > 0, "the pane states its totals")
      rec.reject(
        "pane stat with a colour",
        "panel",
        { ...pane, child: undefined, stats: [{ ...pane.stats[0], color: "c_red" }] },
        "tone says over capacity; colours are the client's",
      )
      const { value: _value, ...bare } = pane.stats[0]
      rec.reject(
        "pane stat without its value",
        "panel",
        { ...pane, child: undefined, stats: [bare] },
        "a stat states its value",
      )

      const area = find(screen.root, "aim:pane:right:areas").cells.find((cell: Value) =>
        cell.enabled && !cell.selected && squares.includes(cell.id)
      )
      assert(area, "an area to show")
      const picked = await engine.invoke("aim:set_area", { row: area.id, pane: "aim:pane:right" })
      assertEquals(picked.stages.at(-1), "completed")
      rec.command("area shown in the right pane", picked)
      assert(
        find(engine.top().root, "aim:pane:right:areas").cells.find((cell: Value) =>
          cell.id === area.id
        ).selected,
        "the picked area is the one shown",
      )

      const filtered = await engine.invoke("aim:filter", { text: "zzzz", pane: "aim:pane:left" })
      assertEquals(filtered.stages.at(-1), "completed")
      rec.command("left pane filtered", filtered)
      assertEquals(find(engine.top().root, "aim:pane:left").filter, "zzzz")
      assertEquals(find(engine.top().root, "aim:pane:left:items").rows.length, 0)
      assertEquals(
        (await engine.invoke("aim:filter", { text: "", pane: "aim:pane:left" })).stages.at(-1),
        "completed",
      )

      const closed = await engine.invoke("aim:close")
      assertEquals(closed.stages.at(-1), "completed")
      rec.command("advanced inventory closed", closed)
      assertEquals(engine.top().role, "map")

      // One of the first row of the left pane to the right pane.
      const again = await open(engine)
      const row = find(again.root, "aim:pane:left:items").rows.find((entry: Value) => !entry.header)
      if (row) {
        const moved = await engine.invoke("aim:move", {
          rows: [{ id: row.id, count: 1 }],
          pane: "aim:pane:right",
        })
        assertEquals(moved.stages.at(-1), "completed")
        rec.command("one item moved to the right pane", moved)
      }
    }),
}
