/**
 * @module
 * The auto notes manager of the tutorial avatar: its notice before any map extra is discovered and the option
 * switched by its registered action; three map extras spawned through the debug menu, a row per extra named by its
 * data name, a row picked and a note toggled by its affordances and by the registered actions, a row the list does
 * not hold refused, and the save question answered Yes.
 */
import { assert, assertEquals } from "@std/assert"
import { Engine, type Scenario, type Value } from "./driver.ts"
import { walk } from "./inventory.ts"
import { ids, intoTheTutorial, leave, openManager, part } from "./rules.ts"

/** The row of the list on top whose label is `label`, chosen through the list's own affordance. */
const chooseRow = async (engine: Engine, label: string) => {
  const list = walk(engine.top().root).find((component) => component.kind === "list")
  const row = list?.rows.find((entry: Value) => entry.label === label)
  assert(row, `no row "${label}"`)
  assertEquals((await engine.invoke(list.activate, { row: row.id })).stages.at(-1), "completed")
}

/** Spawns `extra` at the avatar through the debug menu, which makes the player discover it, and leaves the menu. */
const discover = async (engine: Engine, extra: string) => {
  assertEquals(
    (await engine.submit({ kind: "action", action_id: "debug" })).stages.at(-1),
    "completed",
  )
  await chooseRow(engine, "Map…")
  await chooseRow(engine, "Spawn map extra")
  await chooseRow(engine, extra)
  assertEquals(engine.top().context, "OVERMAP")
  assertEquals((await engine.invoke("CONFIRM")).stages.at(-1), "completed")
  for (let step = 0; engine.top().role !== "map" && step < 5; step++) {
    const cancel = engine.top().affordances.find((entry: Value) => entry.role === "cancel")
    assert(cancel, "a way out of the debug menu")
    assertEquals((await engine.invoke(cancel.id)).stages.at(-1), "completed")
  }
  assertEquals(engine.top().role, "map")
}

const noted = (engine: Engine) =>
  Object.fromEntries(
    part(engine, "autonote:extras").rows.map((row: Value) => [row.id, row.toggled]),
  )

export const autonote: Scenario = {
  area: "autonote",
  run: (rec) =>
    Engine.run(rec, async (engine) => {
      await intoTheTutorial(engine)
      await openManager(engine, "open_autonotes", "AUTO_NOTES")
      rec.accept("auto notes manager before any discovery", "screen", engine.top())
      assertEquals(part(engine, "autonote:extras").rows.length, 0)
      assert(part(engine, "autonote:notice"), "the screen says why it lists nothing")
      const affordances = engine.top().affordances.map((entry: Value) => entry.id)
      assert(affordances.includes("SWITCH_AUTO_NOTE_OPTION") && affordances.includes("QUIT"))
      assert(!affordances.includes("autonote:select") && !affordances.includes("autonote:toggle"))

      const stat = () => part(engine, "autonote:status").stats[0]
      const before = stat().value
      const switched = await engine.invoke("SWITCH_AUTO_NOTE_OPTION")
      assertEquals(switched.stages.at(-1), "completed")
      rec.command("auto notes option switched", switched)
      assertEquals(stat().value, before === "True" ? "False" : "True")
      assertEquals(stat().tone, before === "True" ? "bad" : "good")
      assertEquals(
        (await engine.invoke("SWITCH_AUTO_NOTE_OPTION")).stages.at(-1),
        "completed",
      )
      assertEquals(stat().value, before)
      assertEquals((await engine.invoke("QUIT")).stages.at(-1), "completed")
      await engine.acknowledge(() => engine.top().role === "map")

      for (const extra of ["mx_crater", "mx_collegekids", "mx_helicopter"]) {
        await discover(engine, extra)
      }
      await openManager(engine, "open_autonotes", "AUTO_NOTES")
      const [crater, campers, helicopter] = ids(engine, "autonote:extras")
      assertEquals(
        [crater, campers, helicopter],
        ["extra:mx_crater", "extra:mx_collegekids", "extra:mx_helicopter"],
      )
      assertEquals(engine.top().focus.row_id, crater)
      // The tutorial starts with every note off; two are switched on so that the screen shows both states.
      for (const row of [crater, helicopter]) {
        assertEquals(
          (await engine.invoke("autonote:toggle", { row })).stages.at(-1),
          "completed",
        )
      }
      assertEquals(
        noted(engine),
        { [crater]: true, [campers]: false, [helicopter]: true },
      )
      rec.accept("auto notes manager with three discovered map extras", "screen", engine.top())

      const initial = noted(engine)
      const select = await engine.invoke("autonote:select", { row: campers })
      assertEquals(select.stages.at(-1), "completed")
      rec.command("map extra picked in the list", select)
      assertEquals(engine.top().focus.row_id, campers)
      const toggle = await engine.invoke("autonote:toggle", { row: campers })
      assertEquals(toggle.stages.at(-1), "completed")
      rec.command("map extra note switched", toggle)
      const switchedOnce = { ...initial, [campers]: !initial[campers] }
      assertEquals(noted(engine), switchedOnce)
      const enable = await engine.invoke("ENABLE_MAPEXTRA_NOTE")
      assertEquals(enable.stages.at(-1), "completed")
      rec.command("map extra note enabled at the cursor", enable)
      assertEquals(noted(engine)[campers], true)
      assertEquals((await engine.invoke("DISABLE_MAPEXTRA_NOTE")).stages.at(-1), "completed")
      assertEquals(noted(engine)[campers], false)

      const byPlace = await engine.invoke("autonote:toggle", { row: "extra:0" })
      assertEquals(byPlace.error, "invalid_arguments")
      rec.command("map extra that the list does not hold", byPlace)
      const unknown = await engine.invoke("autonote:select", { row: "extra:mx_nothing" })
      assertEquals(unknown.error, "invalid_arguments")

      // Leaving with a change asks to save it; the answer is Yes, so the next visit shows it.
      await leave(engine)
      await openManager(engine, "open_autonotes", "AUTO_NOTES")
      assertEquals(noted(engine), { ...initial, [campers]: false })
      assertEquals((await engine.invoke("QUIT")).stages.at(-1), "completed")
      await engine.acknowledge(() => engine.top().role === "map")
    }),
}
