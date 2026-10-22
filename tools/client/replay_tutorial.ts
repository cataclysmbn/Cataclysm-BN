/**
 * @module
 * The tutorial session `replay_record.ts --scenario tutorial` records: an exact count dropped, the advanced
 * inventory filtered and used, a backpack worn, a machete picked up, cargo pants salvaged and their rags picked
 * up, a batch of two makeshift bandages crafted, the refusals of a missing recipe and construction, waits and
 * moves. Items are wished to the avatar's feet instead of walked to, so the recording is the same every time.
 */
import { assert, assertEquals } from "@std/assert"
import { Engine, type Value } from "../../tests/protocol/scenarios/driver.ts"
import { chooseRow, walk, wish } from "../../tests/protocol/scenarios/inventory.ts"
import { open } from "../../tests/protocol/scenarios/aim.ts"
import { find } from "../../tests/protocol/scenarios/crafting.ts"
import { wear } from "../../tests/protocol/scenarios/armor.ts"

/** Closes the pop-ups over the map, by their cancel, until the map is on top. */
export async function dismiss(engine: Engine) {
  for (let n = 0; engine.mirror.interaction.interaction !== null && n < 20; n++) {
    const cancel = engine.top().affordances.find((entry: Value) => entry.role === "cancel")
    if (!cancel) throw new Error(`a pop-up without a cancel: ${engine.top().context}`)
    await engine.invoke(cancel.id)
  }
}

/** Waits the first length of the wait menu once. */
export async function waitOnce(engine: Engine) {
  await engine.submit({ kind: "action", action_id: "wait" })
  await engine.choose("entry:0")
  await dismiss(engine)
}

const completed = async (outcome: Promise<{ stages: string[]; error?: string }>) => {
  const done = await outcome
  assertEquals(done.stages.at(-1), "completed", JSON.stringify(done))
}

/** Opens the pickup list at the feet, marks the first row matching `item` with `count` and confirms. */
const pickUp = async (engine: Engine, item: RegExp, count: number) => {
  await completed(engine.invoke("pickup_feet"))
  const items = walk(engine.top().root).find((component) => component.id === "pickup:items")
  const row = items?.rows.find((entry: Value) => item.test(entry.label))
  assert(items && row, `the pickup list holds ${item}`)
  await completed(engine.invoke(items.activate, { row: row.id }))
  await completed(engine.invoke("pickup:count", { row: row.id, count }))
  await completed(engine.invoke("CONFIRM"))
}

/** Runs the tutorial scenario on an engine at the tutorial map; returns the names of the steps that ran. */
export async function tutorialScenario(engine: Engine) {
  const steps: string[] = []
  const step = async (name: string, body: () => Promise<void>) => {
    await body()
    await dismiss(engine)
    assertEquals(engine.top().role, "map", `${name} left the map`)
    steps.push(name)
  }

  await step("drop an exact count", async () => {
    await wish(engine, "rock")
    await completed(engine.invoke("wield"))
    await chooseRow(engine, /^rock/)
    await completed(engine.invoke("drop"))
    const list = walk(engine.top().root).find((component) => component.kind === "list")
    const stack = list?.rows.find((row: Value) => /^rock/.test(row.label))
    assert(list && stack, "the drop selector lists the rock")
    await completed(engine.invoke(list.activate, { row: stack.id }))
    await completed(engine.invoke(`${list.id}:count`, { row: stack.id, count: 1 }))
    await completed(engine.invoke("CONFIRM"))
  })

  await step("advanced inventory filter and move", async () => {
    await open(engine)
    // The feet on the left, the carried things on the right.
    for (const [pane, row] of [["left", "area:CENTER"], ["right", "area:INVENTORY"]]) {
      await completed(
        engine.invoke("aim:set_area", { row, pane: `aim:pane:${pane}` }),
      )
    }
    for (const text of ["zzzz", ""]) {
      await completed(engine.invoke("aim:filter", { text, pane: "aim:pane:left" }))
    }
    const item = find(engine.top().root, "aim:pane:left:items").rows.find((row: Value) =>
      !row.header
    )
    assert(item, "an item in the left pane")
    await completed(
      engine.invoke("aim:move", { rows: [{ id: item.id, count: 1 }], pane: "aim:pane:right" }),
    )
    // A tutorial lesson can open over the screen.
    while (engine.top().context === "POPUP_WAIT") await engine.invoke("QUIT")
    await completed(engine.invoke("aim:close"))
  })

  await step("pick up a machete", async () => {
    await wish(engine, "machete")
    // Alone on the tile it is taken at once; the pants beside it make a list.
    await wish(engine, "cargo pants")
    await pickUp(engine, /^machete/, 1)
    await chooseRow(engine, /^Wield machete/)
  })

  await step("salvage cargo pants", async () => {
    await completed(engine.submit({ kind: "action", action_id: "butcher" }))
    await chooseRow(engine, /Salvage.*cargo pants/)
    await dismiss(engine)
  })

  await step("pick up the rags", async () => {
    await pickUp(engine, /^rag/, 2)
  })

  await step("craft a batch of two makeshift bandages", async () => {
    await completed(engine.submit({ kind: "action", action_id: "craft" }))
    await completed(engine.invoke("craft:search", { text: "makeshift bandage" }))
    const row = find(engine.top().root, "crafting:recipes").rows.find((entry: Value) =>
      entry.enabled && /makeshift bandage/.test(entry.label)
    )
    assert(row, "the makeshift bandage recipe is craftable")
    await completed(engine.invoke("craft:batch", { row: row.id, count: 2 }))
    // The batch list is of recipes of one to many; the row of two is the one chosen.
    const two = find(engine.top().root, "crafting:recipes").rows.find((entry: Value) =>
      entry.selected
    )
    assert(two?.enabled && /^ 2x/.test(two.label), "a batch of two is craftable")
    await completed(engine.invoke("craft:craft", { row: two.id }))
    // The command ends with the activity; the avatar has no room, so the bandages lie at its feet.
    await dismiss(engine)
    assert(
      engine.mirror.avatar.ground.some((item: Value) => /^makeshift bandage/.test(item.name)),
      "the bandages were crafted",
    )
  })

  // Once worn, a backpack takes what is wished for; the wishes above want the floor.
  await step("wear a backpack", () => wear(engine, "backpack"))

  await step("crafting and construction denials", async () => {
    const rejected = async (outcome: Promise<{ stages: string[] }>) => {
      const done = await outcome
      assertEquals(done.stages.at(-1), "rejected", JSON.stringify(done))
    }
    await completed(engine.submit({ kind: "action", action_id: "craft" }))
    await rejected(engine.invoke("craft:craft", { row: "no_such_recipe" }))
    await dismiss(engine)
    await completed(engine.submit({ kind: "action", action_id: "construct" }))
    await completed(engine.invoke("construct:search", { text: "wall" }))
    await rejected(engine.invoke("construct:construct", { row: "no_such_group" }))
    await dismiss(engine)
  })

  await step("waits and moves", async () => {
    for (let n = 0; n < 3; n++) await waitOnce(engine)
    for (const direction of ["RIGHT", "RIGHT", "LEFT", "LEFT"]) {
      await completed(engine.invoke(direction))
    }
  })
  return steps
}
