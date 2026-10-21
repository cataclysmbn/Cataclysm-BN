/**
 * @module
 * The map editor's marks: the selection on the editor, the border of the overmap terrain square the mapgen
 * retarget moves, and the stamp the editor screen keeps under a mapgen preview.
 */
import { assert, assertEquals } from "@std/assert"
import { Engine, type Scenario, type Value } from "./driver.ts"
import { walk } from "./inventory.ts"

const target = (screen: Value) => walk(screen.root).find((c) => c.kind === "map")!.target
const roles = (screen: Value) => (target(screen).marks ?? []).map((mark: Value) => mark.role)

export const editmap: Scenario = {
  area: "editmap",
  run: (rec) =>
    Engine.run(rec, async (engine) => {
      await engine.toMainMenu()
      await engine.enterTutorial()
      await engine.submit({ kind: "action", action_id: "wait" })
      await engine.choose("entry:0")
      await engine.acknowledge(() => engine.mirror.interaction.interaction === null)

      // Entries are numbered by position: Map is the seventh of the debug menu, the editor the sixth of Map.
      assertEquals(
        (await engine.submit({ kind: "action", action_id: "debug" })).stages.at(-1),
        "completed",
      )
      assertEquals((await engine.choose("entry:6")).stages.at(-1), "completed")
      assertEquals((await engine.choose("entry:5")).stages.at(-1), "completed")
      await engine.follow(() => engine.top().kind === "editmap")
      const editor = engine.top()
      assertEquals(roles(editor), ["selection"])
      assertEquals(target(editor).marks[0].squares, [target(editor).current])
      rec.accept("map editor with its selection", "screen", editor)

      assertEquals((await engine.invoke("EDIT_OVERMAP")).stages.at(-1), "completed")
      await engine.follow(() => engine.top().context === "EDIT_MAPGEN")
      assertEquals((await engine.invoke("EDITMAP_MOVE")).stages.at(-1), "completed")
      await engine.follow(() => engine.top().kind === "editmap_mapgen")
      const retarget = engine.top()
      assertEquals(roles(retarget), ["omt_border"])
      const { min, max } = target(retarget).marks[0].rect
      assertEquals([max.x - min.x + 1, max.y - min.y + 1], [24, 24])
      rec.accept(
        "mapgen retarget with the border of its overmap terrain square",
        "screen",
        retarget,
      )
      assertEquals((await engine.invoke("CONFIRM")).stages.at(-1), "completed")
      await engine.follow(() => engine.top().context === "EDIT_MAPGEN")

      // The first overmap terrain the list offers; the null terrain before it is disabled.
      const oter = engine.mirror.interaction.interaction.choices.find((entry: Value) =>
        entry.enabled
      )
      assert(oter, "the mapgen list offers an overmap terrain")
      assertEquals((await engine.choose(oter.id)).stages.at(-1), "completed")
      await engine.follow(() => engine.top().context === "MAPGEN_PREVIEW")
      const under = engine.mirror.interaction.stack.find((screen: Value) =>
        screen.kind === "editmap"
      )
      assertEquals(roles(under).sort(), ["omt_border", "stamp"])
      rec.accept("map editor under a mapgen preview with the stamp", "screen", under)
      rec.reject(
        "mark with both squares and a rect",
        "mark",
        { role: "selection", squares: [min], rect: { min, max } },
        "a mark is its squares or its rectangle, not both",
      )
      rec.reject(
        "mark of an unknown role",
        "mark",
        { role: "cursor", squares: [min] },
        "the cursor is `current`; marks are the other highlights",
      )

      // Leaving the preview restores the map under it, which may start the client over from a snapshot.
      for (const context of ["MAPGEN_PREVIEW", "EDIT_MAPGEN"]) {
        assertEquals(engine.top().context, context)
        assertEquals((await engine.invoke("QUIT")).stages.at(-1), "completed")
        await engine.follow(() => engine.stale || engine.top().context !== context)
        if (engine.stale) await engine.subscribe()
      }
      await engine.follow(() => engine.top().kind === "editmap")
      const left = await engine.invoke("QUIT")
      assertEquals(left.stages.at(-1), "completed")
      rec.command("map editor left", left)
    }),
}
