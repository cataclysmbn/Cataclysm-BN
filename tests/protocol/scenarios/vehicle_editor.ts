/**
 * @module
 * The vehicle editor from the main menu: the Editor tab's vehicle editor entry starts the editor world and opens
 * the vehicle editor at once; it opens a prototype by its id, shows the vehicle the game builds from it, installs
 * a part on a mount from the install tab, refuses one the game would not mount, undoes the install, spawns the
 * prototype, edits the spawned vehicle in place from the world tab, copies and pastes it with its state, and leaves
 * for the map.
 */
import { assert, assertEquals } from "@std/assert"
import { Engine, type Scenario, type Value } from "./driver.ts"
import { walk } from "./inventory.ts"

const part = (screen: Value, id: string) => walk(screen.root).find((entry) => entry.id === id)
const stack = (screen: Value) => part(screen, "vehicle:stack").rows.map((row: Value) => row.label)

export const vehicleEditor: Scenario = {
  area: "vehicle_editor",
  run: (rec) =>
    Engine.run(rec, async (engine) => {
      await engine.toMainMenu()
      assertEquals((await engine.invoke("menu:tab", { row: "editor" })).stages.at(-1), "completed")
      const started = await engine.invoke("menu:entry", { row: "editor:vehicle" })
      rec.command("vehicle editor started from the main menu", started)
      await engine.follow(() => engine.stale)
      await engine.subscribe()
      await engine.acknowledge(() => engine.top().kind === "vehicle_editor")
      assertEquals(engine.top().title, "Vehicle Editor")

      assertEquals(
        (await engine.invoke("vehicle:filter", { text: "bicycle" })).stages.at(-1),
        "completed",
      )
      rec.accept(
        "vehicle editor listing the prototypes that match a filter",
        "screen",
        engine.top(),
      )
      assertEquals(
        (await engine.invoke("vehicle:open", { row: "prototype:bicycle" })).stages.at(-1),
        "completed",
      )
      assertEquals(part(engine.top(), "vehicle:document").title, "Bicycle")
      assertEquals(
        (await engine.invoke("vehicle:mount", { row: "mount:1,0" })).stages.at(-1),
        "completed",
      )
      const before = stack(engine.top())
      assertEquals(
        (await engine.invoke("vehicle:part_filter", { text: "headlight" })).stages.at(-1),
        "completed",
      )
      rec.accept("vehicle editor with a prototype open on a mount", "screen", engine.top())

      const installed = await engine.invoke("vehicle:install", { row: "vpart:headlight" })
      rec.command("part installed on a prototype's mount", installed)
      assertEquals(stack(engine.top()), [...before, "headlight"])
      assertEquals(
        (await engine.invoke("vehicle:part_filter", { text: "xlframe_vertical" })).stages.at(-1),
        "completed",
      )
      const frame = part(engine.top(), "vehicle:parts").rows.find((row: Value) =>
        row.id === "vpart:xlframe_vertical"
      )
      assertEquals(frame.enabled, false)
      assert(frame.denial.length > 0, "the game's mount rule refuses a second frame")
      rec.accept("part the mount does not take", "row", frame)

      assertEquals((await engine.invoke("vehicle:undo")).stages.at(-1), "completed")
      assertEquals(
        (await engine.invoke("vehicle:tool", { row: "tool:mount" })).stages.at(-1),
        "completed",
      )
      assertEquals(stack(engine.top()), before)

      const spawned = await engine.invoke("vehicle:spawn")
      rec.command("prototype spawned beside the avatar", spawned)
      assertEquals(
        (await engine.invoke("vehicle:source", { row: "source:world" })).stages.at(-1),
        "completed",
      )
      const world = () => part(engine.top(), "vehicle:world").rows
      assertEquals(world().length, 1)
      assertEquals(
        (await engine.invoke("vehicle:edit", { row: "world:0" })).stages.at(-1),
        "completed",
      )
      assertEquals(part(engine.top(), "vehicle:live").title, "Bicycle")
      assertEquals(
        (await engine.invoke("vehicle:mount", { row: "mount:1,0" })).stages.at(-1),
        "completed",
      )
      const plain = stack(engine.top())
      assertEquals(
        (await engine.invoke("vehicle:part_filter", { text: "headlight" })).stages.at(-1),
        "completed",
      )
      const edited = await engine.invoke("vehicle:install", { row: "vpart:headlight" })
      rec.command("part installed on a world vehicle in place", edited)
      assertEquals(stack(engine.top()), [...plain, "headlight"])
      rec.accept("vehicle editor editing a world vehicle in place", "screen", engine.top())
      assertEquals((await engine.invoke("vehicle:undo")).stages.at(-1), "completed")
      assertEquals(stack(engine.top()), plain)
      assertEquals((await engine.invoke("vehicle:done")).stages.at(-1), "completed")
      assertEquals(
        (await engine.invoke("vehicle:copy", { row: "world:0" })).stages.at(-1),
        "completed",
      )
      const pasted = await engine.invoke("vehicle:paste")
      rec.command("world vehicle pasted with its state beside the avatar", pasted)
      assertEquals(world().length, 2)
      assertEquals((await engine.invoke("QUIT")).stages.at(-1), "completed")
      assertEquals(engine.top().role, "map")
    }),
}
