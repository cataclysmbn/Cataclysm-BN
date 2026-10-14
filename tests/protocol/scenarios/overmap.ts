/**
 * @module
 * The overmap screen in the tutorial: the squares around the cursor with the facts of the cursor square and
 * the toggles; the cursor moved by its own affordance, and a square beyond its bounds refused; a toggle
 * flipped by its registered action; the read-only query of squares elsewhere, which leaves the cursor
 * and the screen as they are; the tile ids a client that draws tiles is given; and the way out.
 */
import { assert, assertEquals } from "@std/assert"
import { atMainMenu } from "../mirror.ts"
import { Engine, type Recorder, type Scenario, type Value } from "./driver.ts"
import { walk } from "./inventory.ts"

const part = (screen: Value, id: string) => walk(screen.root).find((entry) => entry.id === id)

/** The tutorial with the overmap open over the map; `hello` says what the client can draw. */
const openOvermap = async (engine: Engine, hello: Value = {}) => {
  await engine.hello(hello)
  await engine.subscribe()
  await engine.acknowledge(() => atMainMenu(engine.mirror))
  await engine.enterTutorial()
  assertEquals(
    (await engine.submit({ kind: "action", action_id: "map" })).stages.at(-1),
    "completed",
  )
  await engine.follow(() => engine.top().context === "OVERMAP")
  return engine.top()
}
export const overmap: Scenario = {
  area: "overmap",
  run: async (rec: Recorder) => {
    await Engine.run(rec, async (engine) => {
      const screen = await openOvermap(engine)
      assertEquals(screen.role, "screen")
      rec.accept("overmap screen", "screen", screen)
      const map = part(screen, "map:overmap")
      assert(map && map.kind === "map" && map.mode === "overmap", "the overmap is a map component")
      const here = map.target.current
      const squares = map.overmap
      assertEquals(squares.cells.length, squares.rows)
      assertEquals(squares.tiles, undefined)
      assert(squares.legend.length > 0, "the legend says where the cursor is")
      assert(
        part(screen, "overmap:toggles").stats.some((stat: Value) =>
          stat.key === "TOGGLE_MAP_NOTES"
        ),
        "the toggles say whether they are on",
      )
      const move = screen.affordances.find((entry: Value) => entry.id === "overmap:cursor")
      assert(move && move.args.pos.unit === "overmap", "the cursor has an affordance of its own")

      const moved = await engine.invoke("overmap:cursor", {
        pos: { dim: "", x: here.x + 3, y: here.y - 2, z: here.z },
      })
      assertEquals(moved.stages.at(-1), "completed")
      rec.command("overmap cursor moved to a square", moved)
      const after = part(engine.top(), "map:overmap")
      assertEquals(after.target.current, { dim: "", x: here.x + 3, y: here.y - 2, z: here.z })
      assertEquals(after.overmap.player, squares.player)
      const refused = await engine.invoke("overmap:cursor", {
        pos: { dim: "", x: 1 << 30, y: 0, z: 0 },
      })
      assertEquals(refused.error, "invalid_arguments")
      rec.command("overmap cursor moved beyond the bounds", refused)

      const flipped = await engine.invoke("TOGGLE_MAP_NOTES")
      assertEquals(flipped.stages.at(-1), "completed")
      rec.command("overmap map notes toggled", flipped)
      const stat = (key: string) =>
        part(engine.top(), "overmap:toggles").stats.find((entry: Value) => entry.key === key)
      assertEquals(stat("TOGGLE_MAP_NOTES").value === "Off", true)
      assertEquals(typeof part(engine.top(), "map:overmap").overmap.notes_title, "string")

      const before = stat("TOGGLE_CITY_LABELS").value
      const labelled = await engine.invoke("TOGGLE_CITY_LABELS")
      assertEquals(labelled.stages.at(-1), "completed")
      rec.command("overmap city labels toggled", labelled)
      assertEquals(stat("TOGGLE_CITY_LABELS").value, before === "On" ? "Off" : "On")
      for (const label of part(engine.top(), "map:overmap").overmap.labels ?? []) {
        assertEquals(typeof label.text, "string")
      }

      // Squares far from the cursor are read without moving it: a spectator's way to pan.
      const stack = JSON.stringify(engine.mirror.interaction.stack)
      const box = {
        epoch: engine.mirror.at.epoch,
        min: { dim: "", x: here.x + 40, y: here.y + 30, z: here.z },
        max: { dim: "", x: here.x + 47, y: here.y + 32, z: here.z },
      }
      const read = await engine.result("bn.overmap.cells", box)
      rec.accept("overmap cells query", "world_cells_params", box)
      rec.accept("overmap squares read away from the cursor", "overmap", read)
      // The members the tutorial has nothing to put in: a name over a square and the mission's target.
      rec.accept("overmap squares with a label and a mission target", "overmap", {
        ...read,
        labels: [{ pos: box.min, text: "Label", foreground: 11, background: 0 }],
        mission_target: box.max,
      })
      assertEquals([read.cols, read.rows], [8, 3])
      assertEquals(read.origin, box.min)
      assertEquals(read.legend, [])
      assertEquals(JSON.stringify(engine.mirror.interaction.stack), stack)
      const inverted = await engine.request("bn.overmap.cells", {
        ...box,
        min: box.max,
        max: box.min,
      })
      assertEquals(inverted.error.data.kind, "validation_failed")
      const huge = await engine.request("bn.overmap.cells", {
        ...box,
        max: { ...box.max, x: here.x + 4000 },
      })
      assertEquals(huge.error.data.kind, "resource_limit")

      assertEquals((await engine.invoke("QUIT")).stages.at(-1), "completed")
      assertEquals(engine.top().role, "map")
    })

    await Engine.run(rec, async (engine) => {
      const screen = await openOvermap(engine, {
        capabilities: { viewport: { cols: 40, rows: 25 }, tiles: true },
      })
      const squares = part(screen, "map:overmap").overmap
      assert(squares.tiles, "a client that draws tiles is given tile ids")
      assertEquals(squares.tiles.cells.length, squares.rows)
      assert(Object.keys(squares.tiles.looks_like).length > 0, "what to try when a tile is missing")
      rec.accept("overmap screen for a client that draws tiles", "screen", screen)
    })
  },
}
