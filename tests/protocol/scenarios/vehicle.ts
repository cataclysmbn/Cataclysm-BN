/**
 * @module
 * The vehicle screen of a bicycle the debug menu spawns under the avatar: the figures, the grid around
 * the cursor and the parts on its square; the install list under its tabs with the requirements of the
 * highlighted part; a tab and a row picked by their own affordances; and the way out.
 */
import { assert, assertEquals } from "@std/assert"
import { Engine, type Scenario, type Value } from "./driver.ts"
import { walk } from "./inventory.ts"

const part = (screen: Value, id: string) => walk(screen.root).find((entry) => entry.id === id)

/** The row of the list on top whose label satisfies `match`, chosen through the list's own affordance. */
const chooseRow = async (engine: Engine, match: (label: string) => boolean) => {
  const list = walk(engine.top().root).find((component) => component.kind === "list")
  const row = list?.rows.find((entry: Value) => match(entry.label))
  assert(row, `no row in ${JSON.stringify(list?.rows.map((entry: Value) => entry.label))}`)
  assertEquals((await engine.invoke(list.activate, { row: row.id })).stages.at(-1), "completed")
}

export const vehicle: Scenario = {
  area: "vehicle",
  run: (rec) =>
    Engine.run(rec, async (engine) => {
      await engine.toMainMenu()
      await engine.enterTutorial()
      await engine.submit({ kind: "action", action_id: "wait" })
      await engine.choose("entry:0")
      await engine.acknowledge(() => engine.mirror.interaction.interaction === null)

      assertEquals(
        (await engine.submit({ kind: "action", action_id: "debug" })).stages.at(-1),
        "completed",
      )
      // A lit candle at the feet gives the light parts are worked by.
      await chooseRow(engine, (label) => label === "Spawning…")
      await chooseRow(engine, (label) => label === "Spawn an item")
      assertEquals((await engine.invoke("FILTER")).stages.at(-1), "completed")
      const filter = engine.top().affordances.find((entry: Value) => entry.role === "confirm")
      assert(filter, "the filter prompt confirms")
      assertEquals((await engine.invoke(filter.id, { text: "candle" })).stages.at(-1), "completed")
      await chooseRow(engine, (label) => label === "candle (lit)")
      const amount = engine.top().affordances.find((entry: Value) => entry.role === "confirm")
      assert(amount, "the amount prompt confirms")
      assertEquals((await engine.invoke(amount.id, { text: "1" })).stages.at(-1), "completed")
      for (let step = 0; engine.top().role !== "map" && step < 5; step++) {
        assertEquals((await engine.invoke("QUIT")).stages.at(-1), "completed")
      }
      assertEquals(
        (await engine.submit({ kind: "action", action_id: "debug" })).stages.at(-1),
        "completed",
      )
      await chooseRow(engine, (label) => label === "Spawning…")
      await chooseRow(engine, (label) => label === "Spawn a vehicle")
      await chooseRow(engine, (label) => label.endsWith("(bicycle)"))
      await chooseRow(engine, (label) => label === "Undamaged")
      const direction = engine.top().affordances.find((entry: Value) => entry.role === "confirm")
      assert(direction, "the direction prompt confirms")
      assertEquals(
        (await engine.invoke(direction.id, { text: "0" })).stages.at(-1),
        "completed",
      )
      for (let step = 0; engine.top().role !== "map" && step < 5; step++) {
        const cancel = engine.top().affordances.find((entry: Value) => entry.role === "cancel")
        assert(cancel, "a way out")
        await engine.invoke(cancel.id)
      }
      // The bicycle is two squares long; the square east of the avatar is a part of it.
      assertEquals((await engine.invoke("examine")).stages.at(-1), "completed")
      await chooseRow(engine, (label) => label === "East")
      await chooseRow(engine, (label) => label === "Examine vehicle")
      await engine.follow(() => engine.top().context === "VEH_INTERACT")

      const screen = engine.top()
      assertEquals(screen.role, "screen")
      rec.accept("vehicle screen", "screen", screen)
      const grid = part(screen, "vehicle:grid")
      assert(grid && grid.kind === "grid", "the grid around the cursor")
      assert(
        grid.cells.some((cell: Value) => cell.selected),
        "the cursor square is among the cells",
      )
      assert(part(screen, "vehicle:parts").rows.length > 0, "the parts on the cursor square")
      const overview = part(screen, "vehicle:overview")
      assert(overview.stats.some((stat: Value) => stat.key === "mass"), "the vehicle's figures")
      const repair = screen.affordances.find((entry: Value) => entry.id === "REPAIR")
      assert(repair && !repair.enabled && repair.denial, "an action that cannot run says why")
      rec.accept("denied repair of an undamaged vehicle", "affordance", repair)

      assertEquals((await engine.invoke("INSTALL")).stages.at(-1), "completed")
      const install = engine.top()
      const tabs = part(install, "vehicle:tabs")
      assert(tabs && tabs.kind === "tabs", "the install tabs")
      assertEquals(tabs.tabs.length, 8)
      rec.accept("install list of the vehicle screen", "screen", install)
      assert(part(install, "vehicle:requirements").rows.length > 0, "the requirements of the part")

      const cargo = await engine.invoke("vehicle:tab", { row: "cargo" })
      assertEquals(cargo.stages.at(-1), "completed")
      rec.command("install tab picked", cargo)
      assert(part(engine.top(), "vehicle:tabs").tabs[1].selected, "the picked tab is selected")
      const unknown = await engine.invoke("vehicle:tab", { row: "nowhere" })
      assertEquals(unknown.error, "invalid_arguments")
      rec.command("install tab the list does not hold", unknown)

      const rows = part(engine.top(), "vehicle:choices").rows
      assert(rows.length > 1, "parts to pick from")
      const picked = await engine.invoke("vehicle:pick", { row: rows[1].id })
      assertEquals(picked.stages.at(-1), "completed")
      rec.command("part picked in the install list", picked)
      assertEquals(engine.top().focus?.row_id ?? engine.top().focus_row, rows[1].id)

      assertEquals((await engine.invoke("QUIT")).stages.at(-1), "completed")
      assertEquals(part(engine.top(), "vehicle:choices"), undefined)
      const left = await engine.invoke("QUIT")
      assertEquals(left.stages.at(-1), "completed")
      rec.command("vehicle screen left", left)
      // The tutorial's lesson about examining opens over the map once the screen is left.
      for (let step = 0; engine.top().role !== "map" && step < 3; step++) {
        assertEquals((await engine.invoke("QUIT")).stages.at(-1), "completed")
      }
      assertEquals(engine.top().role, "map")
    }),
}
