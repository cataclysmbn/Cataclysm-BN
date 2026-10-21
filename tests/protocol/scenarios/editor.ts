/**
 * @module
 * The map editor a protocol client draws: offered on the map only with the Debug Editor trait, it opens a mod's
 * mapgen file, draws a rectangle from an anchor square, undoes and redoes it as one step, moves a placement, and saves
 * the file.
 */
import { assert, assertEquals } from "@std/assert"
import { Engine, type Scenario, type Value } from "./driver.ts"
import { walk } from "./inventory.ts"

const part = (screen: Value, id: string) => walk(screen.root).find((entry) => entry.id === id)
const glyphs = (screen: Value) => {
  const grid = part(screen, "editor:canvas")
  const rows = Array.from({ length: grid.rows }, () => Array(grid.cols).fill(" "))
  for (const cell of grid.cells) rows[cell.y][cell.x] = cell.glyph
  return rows.map((row) => row.join(""))
}
const offer = (screen: Value) => screen.affordances.find((entry: Value) => entry.id === "editor")

/** A nested chunk of floor whose legend also maps a wall, with a zombie in it. */
const chunk = [{
  type: "mapgen",
  method: "json",
  nested_mapgen_id: "editor_scenario_chunk",
  object: {
    mapgensize: [4, 4],
    rows: ["....", "....", "....", "...."],
    terrain: { ".": "t_floor", "#": "t_wall" },
    place_monster: [{ monster: "mon_zombie", x: 1, y: 2 }],
  },
}]

export const editor: Scenario = {
  area: "editor",
  run: (rec) =>
    Engine.run(rec, async (engine) => {
      rec.mask(engine.profile, "profile")
      const file = `${engine.profile}/mods/editor_scenario/chunk.json`
      await Deno.mkdir(`${engine.profile}/mods/editor_scenario`, { recursive: true })
      await Deno.writeTextFile(file, JSON.stringify(chunk, null, 2) + "\n")

      await engine.toMainMenu()
      await engine.enterTutorial()
      await engine.submit({ kind: "action", action_id: "wait" })
      await engine.choose("entry:0")
      await engine.acknowledge(() => engine.mirror.interaction.interaction === null)

      const denied = offer(engine.top())
      assertEquals([denied.enabled, denied.denial], [false, "You need the Debug Editor trait."])
      rec.accept("editor offered on the map without the trait", "affordance", denied)

      // The trait, given through the Lua console's code editor.
      assertEquals(
        (await engine.submit({ kind: "action", action_id: "lua_console" })).stages.at(-1),
        "completed",
      )
      await engine.follow(() => engine.top().kind === "lua_console")
      assertEquals((await engine.invoke("EDIT")).stages.at(-1), "completed")
      await engine.follow(() => engine.top().kind === "text_editor")
      const code = 'gapi.get_avatar():set_mutation(MutationBranchId.new("DEBUG_EDITOR"))'
      assertEquals((await engine.invoke("editor:set", { text: code })).stages.at(-1), "completed")
      assertEquals((await engine.invoke("TEXT.CONFIRM")).stages.at(-1), "completed")
      await engine.follow(() => engine.top().kind === "lua_console")
      assertEquals((await engine.invoke("QUIT")).stages.at(-1), "completed")
      await engine.follow(() => engine.top().role === "map")
      const offered = offer(engine.top())
      assert(offered.enabled, "the trait offers the editor")
      rec.accept("editor offered on the map with the trait", "affordance", offered)

      const opening = await engine.submit({ kind: "action", action_id: "editor" })
      assertEquals(opening.stages.at(-1), "completed")
      await engine.follow(() => engine.top().kind === "editor")
      assertEquals(engine.top().title, "Map Editor")
      rec.accept("map editor with nothing open", "screen", engine.top())

      const outside = await engine.invoke("editor:open", { text: "/etc/hostname.json" })
      assertEquals(outside.stages.at(-1), "completed")
      assert(part(engine.top(), "editor:notice").markup.includes("/etc/hostname.json"))
      const opened = await engine.invoke("editor:open", { text: file })
      assertEquals(opened.stages.at(-1), "completed")
      rec.command("mod file opened in the map editor", opened)
      const open = engine.top()
      assertEquals(part(open, "editor:objects").rows.map((row: Value) => row.label), [
        "editor_scenario_chunk",
      ])
      assertEquals(glyphs(open), ["....", "....", "....", "...."])
      assertEquals(part(open, "editor:legend").rows.map((row: Value) => row.id), [
        "symbol:#",
        "symbol:.",
      ])
      rec.accept("map editor with a nested chunk open", "screen", open)
      assertEquals(
        (await engine.invoke("editor:paint", { row: "square:4,0" })).error,
        "invalid_arguments",
      )

      assertEquals(
        (await engine.invoke("editor:tool", { row: "tool:rect" })).stages.at(-1),
        "completed",
      )
      assertEquals(
        (await engine.invoke("editor:paint", { row: "square:0,0" })).stages.at(-1),
        "completed",
      )
      assertEquals(part(engine.top(), "editor:canvas").cursor, { x: 0, y: 0 })
      const drawn = await engine.invoke("editor:paint", { row: "square:3,3" })
      assertEquals(drawn.stages.at(-1), "completed")
      rec.command("rectangle drawn in the map editor", drawn)
      const walled = ["####", "#..#", "#..#", "####"]
      assertEquals(glyphs(engine.top()), walled)

      assertEquals((await engine.invoke("editor:undo")).stages.at(-1), "completed")
      assertEquals(glyphs(engine.top()), ["....", "....", "....", "...."])
      assertEquals((await engine.invoke("editor:redo")).stages.at(-1), "completed")
      assertEquals(glyphs(engine.top()), walled)
      // A new symbol through the legend form, one field at a time, then written as one step.
      for (const [row, text] of [["symbol", "x"], ["value", "t_dirt"]]) {
        assertEquals(
          (await engine.invoke("editor:mapping_set", { row, text })).stages.at(-1),
          "completed",
        )
      }
      const mapped = await engine.invoke("editor:mapping_apply")
      assertEquals(mapped.stages.at(-1), "completed")
      rec.command("legend mapping set in the map editor", mapped)
      assertEquals(part(engine.top(), "editor:legend").rows.map((row: Value) => row.id), [
        "symbol:#",
        "symbol:.",
        "symbol:x",
      ])
      rec.accept("map editor with a drawn room and a new mapping", "screen", engine.top())
      assertEquals((await engine.invoke("editor:undo")).stages.at(-1), "completed")
      assertEquals(glyphs(engine.top()), walled)

      // A picked placement fills the move and field forms; a move is written as one step.
      assertEquals(
        (await engine.invoke("editor:placement", { row: "placement:place_monster:0" })).stages.at(
          -1,
        ),
        "completed",
      )
      rec.accept("map editor with a placement picked", "screen", engine.top())
      assertEquals(
        (await engine.invoke("editor:move_set", { row: "x", text: "2" })).stages.at(-1),
        "completed",
      )
      const moved = await engine.invoke("editor:move_apply")
      assertEquals(moved.stages.at(-1), "completed")
      rec.command("placement moved in the map editor", moved)
      assertEquals(part(engine.top(), "editor:canvas").cursor, { x: 2, y: 2 })

      // A nested chunk is generated only inside another mapgen, so it has no preview of its own.
      const preview = await engine.invoke("editor:preview", { count: 3 })
      assertEquals(preview.stages.at(-1), "completed")
      assert(part(engine.top(), "editor:notice"), "the preview says why it shows nothing")
      assertEquals(part(engine.top(), "editor:preview"), undefined)
      const saved = await engine.invoke("editor:save")
      assertEquals(saved.stages.at(-1), "completed")
      rec.command("map editor saved", saved)
      const written = JSON.parse(await Deno.readTextFile(file))[0].object
      assertEquals([written.rows, written.place_monster], [walled, [{
        monster: "mon_zombie",
        x: 2,
        y: 2,
      }]])

      assertEquals((await engine.invoke("QUIT")).stages.at(-1), "completed")
      assertEquals(engine.top().role, "map")
    }),
}
