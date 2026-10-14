/**
 * @module
 * The information screens of the tutorial avatar: the message log with its filter, the help topics and a topic's
 * text, the morale screen, the mission list and the faction manager with a tab picked, and the scores with a tab
 * picked.
 */
import { assert, assertEquals } from "@std/assert"
import { Engine, type Scenario, type Value } from "./driver.ts"
import { walk } from "./inventory.ts"

const part = (screen: Value, id: string) => walk(screen.root).find((entry) => entry.id === id)

/** Opens the screen of the registered game action `action` and checks its context. */
const open = async (engine: Engine, action: string, context: string) => {
  assertEquals(
    (await engine.submit({ kind: "action", action_id: action })).stages.at(-1),
    "completed",
  )
  assertEquals(engine.top().context, context)
  return engine.top()
}

/** Leaves the screens above the map. */
const close = async (engine: Engine) => {
  for (let step = 0; engine.top().role !== "map" && step < 4; step++) {
    const cancel = engine.top().affordances.find((entry: Value) => entry.role === "cancel")
    assert(cancel, "a way out")
    await engine.invoke(cancel.id)
  }
  assertEquals(engine.top().role, "map")
}

export const info: Scenario = {
  area: "info",
  run: (rec) =>
    Engine.run(rec, async (engine) => {
      await engine.toMainMenu()
      await engine.enterTutorial()
      await engine.submit({ kind: "action", action_id: "wait" })
      await engine.choose("entry:0")
      await engine.acknowledge(() => engine.mirror.interaction.interaction === null)

      const messages = await open(engine, "messages", "MESSAGE_LOG")
      rec.accept("message log", "screen", messages)
      assert(part(messages, "messages:log"), "the log")
      await close(engine)

      const help = await open(engine, "help", "default")
      rec.accept("help topics", "screen", help)
      const topics = part(help, "help:topics")
      assert(topics.rows.length > 1, "the topics")
      const unknown = await engine.invoke("help:topic", { row: "topic:nowhere" })
      rec.command("help topic the list does not hold", unknown)
      assertEquals(unknown.error, "invalid_arguments")
      const topic = await engine.invoke("help:topic", { row: topics.rows[1].id })
      assertEquals(topic.stages.at(-1), "completed")
      rec.command("help topic picked", topic)
      assert(part(engine.top(), "text:body"), "the topic's text")
      await close(engine)

      rec.accept("morale", "screen", await open(engine, "morale", "MORALE"))
      await close(engine)

      const missions = await open(engine, "missions", "MISSIONS")
      rec.accept("mission list", "screen", missions)
      const missionTab = await engine.invoke("missions:tab", { row: "completed" })
      assertEquals(missionTab.stages.at(-1), "completed")
      rec.command("mission tab picked", missionTab)
      await close(engine)

      const factions = await open(engine, "factions", "FACTION MANAGER")
      rec.accept("faction manager", "screen", factions)
      const factionTab = await engine.invoke("factions:tab", { row: "creatures" })
      assertEquals(factionTab.stages.at(-1), "completed")
      rec.command("faction tab picked", factionTab)
      await close(engine)

      const scores = await open(engine, "scores", "SCORES")
      rec.accept("scores", "screen", scores)
      const scoreTab = await engine.invoke("scores:tab", { row: "scores" })
      assertEquals(scoreTab.stages.at(-1), "completed")
      rec.command("scores tab picked", scoreTab)
      await close(engine)
    }),
}
