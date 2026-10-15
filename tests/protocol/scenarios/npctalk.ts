/**
 * @module
 * The NPC talk window: the history log and the responses, each named by a fixed name of the place that makes
 * it or by its topic and its place in the topic's data, the special keys as rows of their own, a response
 * picked through its list and a row the window does not hold refused. The NPC is the shopkeeper template the
 * debug menu spawns by id, so the run does not depend on the seed.
 */
import { assert, assertEquals } from "@std/assert"
import { Engine, type Scenario, type Value } from "./driver.ts"
import { walk } from "./inventory.ts"

const part = (screen: Value, id: string) => walk(screen.root).find((entry) => entry.id === id)

export const npctalk: Scenario = {
  area: "npctalk",
  run: (rec) =>
    Engine.run(rec, async (engine) => {
      await engine.toMainMenu()
      await engine.enterTutorial()
      await engine.submit({ kind: "action", action_id: "wait" })
      await engine.choose("entry:0")
      await engine.acknowledge(() => engine.mirror.interaction.interaction === null)

      // The trade entry of the debug menu places the shopkeeper beside the avatar; the trade is left unmade.
      assertEquals(
        (await engine.submit({ kind: "action", action_id: "debug" })).stages.at(-1),
        "completed",
      )
      assertEquals((await engine.choose("entry:2")).stages.at(-1), "completed")
      assertEquals((await engine.choose("entry:2")).stages.at(-1), "completed")
      await engine.follow(() => engine.top().context === "NPC_TRADE")
      assertEquals((await engine.invoke("trade:cancel")).stages.at(-1), "completed")

      // Chat with the one NPC in sight.
      assertEquals(
        (await engine.submit({ kind: "action", action_id: "chat" })).stages.at(-1),
        "completed",
      )
      const menu = walk(engine.top().root).find((entry) => entry.kind === "list")
      const talk = menu?.rows.find((row: Value) => row.label.startsWith("Talk to"))
      assert(menu && talk, "the chat menu offers a talk")
      assertEquals(
        (await engine.invoke(menu.activate, { row: talk.id })).stages.at(-1),
        "completed",
      )
      await engine.follow(() => engine.top().context === "DIALOGUE_CHOOSE_RESPONSE")

      const dialogue = engine.top()
      assertEquals(dialogue.role, "dialog")
      rec.accept("NPC talk screen", "screen", dialogue)
      const history = part(dialogue, "talk:history:text")
      assert(history && history.markup.length > 0, "the NPC's line is the history")
      const responses = part(dialogue, "talk:responses")
      assert(responses && responses.rows.length > 0, "responses to pick")
      assertEquals(dialogue.focus_row ?? dialogue.focus?.row_id, responses.rows[0].id)
      assertEquals(
        part(dialogue, "talk:special").rows.map((row: Value) => row.id),
        ["special:look_at", "special:size_up", "special:yell", "special:opinion"],
      )
      const respond = dialogue.affordances.find((entry: Value) => entry.id === "talk:respond")
      assert(respond, "the respond affordance")
      rec.accept("respond affordance of the NPC talk screen", "affordance", respond)

      // A response the window does not hold, and a special key named as a response, are refused.
      for (
        const [affordance, row, what] of [
          ["talk:respond", "nothing:0", "a response the window does not hold"],
          ["talk:respond", "special:yell", "a special key named as a response"],
          ["talk:special:run", responses.rows[0].id, "a response named as a special key"],
        ]
      ) {
        const refused = await engine.invoke(affordance, { row })
        assertEquals(refused.error, "invalid_arguments")
        rec.command(`${what} refused`, refused)
      }

      // A special key answers with its own topic, and what was said stays in the history.
      const sized = await engine.invoke("talk:special:run", { row: "special:size_up" })
      assertEquals(sized.stages.at(-1), "completed", JSON.stringify(sized))
      rec.command("special key picked", sized)
      const after = part(engine.top(), "talk:history:text")
      assert(after.markup.length > history.markup.length, "the history grew")

      // Answer until the talk is over.
      for (let step = 0; engine.top().role !== "map" && step < 8; step++) {
        const rows = part(engine.top(), "talk:responses").rows
        const last = rows.at(-1)
        assert(last, "a way out")
        const picked = await engine.invoke("talk:respond", { row: last.id })
        assertEquals(picked.stages.at(-1), "completed", JSON.stringify(picked))
        if (step === 0) rec.command("response picked", picked)
        await engine.follow(() =>
          engine.top().role === "map" || engine.top().context === "DIALOGUE_CHOOSE_RESPONSE"
        )
      }
      assertEquals(engine.top().role, "map")
    }),
}
