/**
 * @module
 * The modes of the avatar, a mode set to a value however often it is sent, and a value the mode does not take.
 */
import { assert, assertEquals } from "@std/assert"
import { Engine, type Scenario, type Value } from "./driver.ts"

export const modes: Scenario = {
  area: "modes",
  run: (rec) =>
    Engine.run(rec, async (engine) => {
      await engine.toMainMenu()
      await engine.enterTutorial()
      // The first turns of the tutorial open lessons; let them out of the way.
      await engine.submit({ kind: "action", action_id: "wait" })
      await engine.choose("entry:0")
      await engine.acknowledge(() => engine.mirror.interaction.interaction === null)

      const mode = (id: string) =>
        engine.mirror.avatar.modes.find((entry: Value) => entry.id === id)
      const safe = mode("safe_mode")
      assert(safe, "the avatar lists the safe mode")
      rec.accept("safe mode of the avatar", "mode", safe)
      const pulp = mode("auto_pulp_butcher")
      assert(pulp, "the avatar lists a mode of an option with several values")
      rec.accept("mode of an option with several values", "mode", pulp)

      const first = await engine.submit({ kind: "set_mode", mode: "safe_mode", value: "off" })
      assertEquals(first.stages.at(-1), "completed", JSON.stringify(first))
      assertEquals(mode("safe_mode").value, "off")
      rec.command("safe mode set off", first)
      const again = await engine.submit({ kind: "set_mode", mode: "safe_mode", value: "off" })
      assertEquals(again.stages.at(-1), "completed", JSON.stringify(again))
      assertEquals(mode("safe_mode").value, "off", "setting the value it holds changes nothing")
      rec.command("safe mode set off again", again)
      const on = await engine.submit({ kind: "set_mode", mode: "safe_mode", value: "on" })
      assertEquals(on.stages.at(-1), "completed", JSON.stringify(on))
      assertEquals(mode("safe_mode").value, "on")
      rec.command("safe mode set on", on)

      const unknown = await engine.submit({ kind: "set_mode", mode: "safe_mode", value: "maybe" })
      assertEquals(unknown.error, "validation_failed")
      rec.command("mode set to a value it does not take", unknown)
    }),
}
