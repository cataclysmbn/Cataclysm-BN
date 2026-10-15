/**
 * @module
 * The distraction manager: a row per distraction with its switch, the cursor moved and a distraction switched
 * off by its token (and a row the list does not hold, refused). The colour manager: a row per colour with its
 * customs, a column picked by its token, a custom colour set through the editor beside the list (and a colour it
 * does not offer, refused), and the save question answered No. The auto pickup and safe mode managers of the tutorial
 * avatar: rules added through their text entry and edited, moved, copied and swapped by the ids the session gave
 * them, the match lists of a rule, and the save question answered Yes.
 */
import { assert, assertEquals } from "@std/assert"
import { Engine, type Scenario, type Value } from "./driver.ts"
import { walk } from "./inventory.ts"

const part = (engine: Engine, id: string) =>
  walk(engine.top().root).find((entry) => entry.id === id)

/** Settings from the main menu: the tab, then the entry. */
const openSetting = async (engine: Engine, entry: string) => {
  await engine.toMainMenu()
  assertEquals((await engine.invoke("menu:tab", { row: "settings" })).stages.at(-1), "completed")
  assertEquals((await engine.invoke("menu:entry", { row: entry })).stages.at(-1), "completed")
}

export const distractions: Scenario = {
  area: "distractions",
  run: (rec) =>
    Engine.run(rec, async (engine) => {
      await openSetting(engine, "settings:distractions")
      const screen = engine.top()
      assertEquals(screen.context, "DISTRACTION_MANAGER")
      rec.accept("distraction manager", "screen", screen)
      const rows = part(engine, "distractions:items").rows
      assert(rows.length > 1 && rows.every((row: Value) => row.toggled === true), "all switched on")

      const select = await engine.invoke("distractions:select", { row: rows[1].id })
      assertEquals(select.stages.at(-1), "completed")
      rec.command("distraction picked in the list", select)
      assertEquals(engine.top().focus.row_id, rows[1].id)

      const toggle = await engine.invoke("distractions:toggle", { row: rows[1].id })
      assertEquals(toggle.stages.at(-1), "completed")
      rec.command("distraction switched off", toggle)
      const switched = part(engine, "distractions:items").rows[1]
      assertEquals(switched.toggled, false)
      rec.accept("distraction manager with a distraction switched off", "screen", engine.top())

      const unknown = await engine.invoke("distractions:toggle", { row: "distraction:Nothing" })
      assertEquals(unknown.error, "invalid_arguments")
      rec.command("distraction that the list does not hold", unknown)

      // Leaving saves at once, as the native screen does.
      assertEquals((await engine.invoke("QUIT")).stages.at(-1), "completed")
      await engine.acknowledge(() => engine.top().role !== "dialog")
    }),
}

export const colors: Scenario = {
  area: "colors",
  run: (rec) =>
    Engine.run(rec, async (engine) => {
      await openSetting(engine, "settings:colors")
      const screen = engine.top()
      assertEquals(screen.context, "COLORS")
      rec.accept("colour manager", "screen", screen)
      const columns = part(engine, "colors:columns")
      assert(columns && columns.kind === "tabs", "the columns the cursor can be in")
      assertEquals(columns.tabs.map((tab: Value) => tab.id), ["normal", "invert"])
      assertEquals(columns.select, "colors:column")
      const list = part(engine, "colors:items")
      assert(list.total > 100, "a row per colour")
      const rows = list.rows

      const column = await engine.invoke("colors:column", { row: "invert" })
      assertEquals(column.stages.at(-1), "completed")
      rec.command("colour column picked by its token", column)
      assert(part(engine, "colors:columns").tabs[1].selected, "the invert column")

      const select = await engine.invoke("colors:select", { row: rows[1].id })
      assertEquals(select.stages.at(-1), "completed")
      rec.command("colour picked in the list", select)
      const editor = part(engine, "colors:editor")
      assertEquals(editor.fields[0].id, rows[1].id)
      assertEquals(editor.fields[0].type, "select")

      const set = await engine.invoke("colors:set", { row: rows[1].id, text: rows[0].label })
      assertEquals(set.stages.at(-1), "completed")
      rec.command("custom colour set through the editor", set)
      assertEquals(part(engine, "colors:editor").fields[0].value, rows[0].label)
      assertEquals(
        part(engine, "colors:items").rows[1].columns.find((entry: Value) =>
          entry.key === "invert_custom"
        ).value,
        rows[0].label,
      )
      rec.accept("colour manager with a custom colour set", "screen", engine.top())

      // The editor offers the colours of the list and nothing else, and only for the colour under the cursor.
      const foreign = await engine.invoke("colors:set", { row: rows[1].id, text: "no such colour" })
      assertEquals(foreign.error, "invalid_arguments")
      rec.command("custom colour that the editor does not offer", foreign)
      const other = await engine.invoke("colors:set", { row: rows[2].id, text: rows[0].label })
      assertEquals(other.error, "invalid_arguments")
      rec.command("custom colour for a colour that is not under the cursor", other)

      // Leaving with a change asks to save it; the answer is No, so nothing reaches the profile.
      assertEquals((await engine.invoke("QUIT")).stages.at(-1), "completed")
      assertEquals((await engine.choose("option:1")).stages.at(-1), "completed")
      await engine.acknowledge(() => engine.top().role !== "dialog")
    }),
}

/** The tutorial avatar on the map, with its first lessons out of the way. */
const intoTheTutorial = async (engine: Engine) => {
  await engine.toMainMenu()
  await engine.enterTutorial()
  await engine.submit({ kind: "action", action_id: "wait" })
  await engine.choose("entry:0")
  await engine.acknowledge(() => engine.mirror.interaction.interaction === null)
}

/** Opens the manager the `action` of the map names. */
const openManager = async (engine: Engine, action: string, context: string) => {
  assertEquals(
    (await engine.submit({ kind: "action", action_id: action })).stages.at(-1),
    "completed",
  )
  assertEquals(engine.top().context, context)
}

/** Answers the text entry on top with `text`, through its own confirming affordance. */
const typeText = async (engine: Engine, text: string) => {
  const confirm = engine.top().affordances.find((entry: Value) => entry.role === "confirm")
  assert(confirm, "the text entry confirms")
  return await engine.invoke(confirm.id, { text })
}

/** The ids of the rows of the list `id` on top, in order. */
const ids = (engine: Engine, id: string) => part(engine, id).rows.map((row: Value) => row.id)

/** Leaves the manager on top and answers the question to save its changes Yes. */
const leave = async (engine: Engine) => {
  assertEquals((await engine.invoke("QUIT")).stages.at(-1), "completed")
  assertEquals((await engine.choose("option:0")).stages.at(-1), "completed")
  await engine.acknowledge(() => engine.top().role === "map")
}

export const autopickup: Scenario = {
  area: "autopickup",
  run: (rec) =>
    Engine.run(rec, async (engine) => {
      await intoTheTutorial(engine)
      await openManager(engine, "open_autopickup", "AUTO_PICKUP")
      rec.accept("auto pickup manager without rules", "screen", engine.top())
      const tabs = part(engine, "autopickup:tabs")
      assertEquals(tabs.tabs.map((tab: Value) => tab.id), ["global", "character"])
      assertEquals(tabs.select, "autopickup:tab")
      assertEquals(part(engine, "autopickup:rules").rows.length, 0)

      // A rule is added with the registered action, whose text entry opens over the manager.
      const add = await engine.invoke("ADD_RULE")
      assertEquals(add.stages.at(-1), "completed")
      rec.command("rule added, its text asked for", add)
      const typed = await typeText(engine, "sledge hammer")
      assertEquals(typed.stages.at(-1), "completed", JSON.stringify(typed))
      rec.command("rule text entered", typed)
      assertEquals((await engine.invoke("ADD_RULE")).stages.at(-1), "completed")
      assertEquals((await typeText(engine, "*axe*")).stages.at(-1), "completed")
      const rows = part(engine, "autopickup:rules").rows
      assertEquals(rows.map((row: Value) => row.label), ["sledge hammer", "*axe*"])
      assert(
        rows[0].id !== rows[1].id && rows.every((row: Value) => /^rule:[1-9]\d*$/.test(row.id)),
      )
      rec.accept("auto pickup manager with two rules", "screen", engine.top())

      const select = await engine.invoke("autopickup:select", { row: rows[0].id })
      assertEquals(select.stages.at(-1), "completed")
      rec.command("rule picked in the list", select)
      assertEquals(engine.top().focus.row_id, rows[0].id)
      const mode = await engine.invoke("autopickup:mode", { row: rows[0].id })
      assertEquals(mode.stages.at(-1), "completed")
      rec.command("rule switched between include and exclude", mode)
      const excluded = part(engine, "autopickup:rules").rows[0].columns.find((entry: Value) =>
        entry.key === "i_e"
      )
      assertEquals(excluded.value, "Exclude")

      const edit = await engine.invoke("autopickup:edit", { row: rows[1].id })
      assertEquals(edit.stages.at(-1), "completed")
      rec.command("rule text asked for again", edit)
      assertEquals((await typeText(engine, "*saw*")).stages.at(-1), "completed")
      assertEquals(part(engine, "autopickup:rules").rows[1].label, "*saw*")

      // A moved rule keeps its id, a copy has an id of its own.
      const moved = await engine.invoke("MOVE_RULE_DOWN")
      assertEquals(moved.stages.at(-1), "completed")
      rec.command("rule moved up", moved)
      assertEquals(ids(engine, "autopickup:rules"), [rows[1].id, rows[0].id])
      const copied = await engine.invoke("COPY_RULE")
      assertEquals(copied.stages.at(-1), "completed")
      rec.command("rule copied", copied)
      const copy = ids(engine, "autopickup:rules")
      assertEquals(copy.length, 3)
      assert(!rows.some((row: Value) => row.id === copy[2]), "the copy has a new id")
      assertEquals((await engine.invoke("REMOVE_RULE")).stages.at(-1), "completed")
      assertEquals(ids(engine, "autopickup:rules"), [rows[1].id, rows[0].id])

      // The items a rule matches.
      assertEquals(
        (await engine.invoke("autopickup:select", { row: rows[0].id })).stages.at(-1),
        "completed",
      )
      assertEquals((await engine.invoke("TEST_RULE")).stages.at(-1), "completed")
      const matches = engine.top()
      assertEquals(matches.context, "AUTO_PICKUP_TEST")
      rec.accept("items that an auto pickup rule matches", "screen", matches)
      assertEquals(part(engine, "autopickup_test:matches").rows.map((row: Value) => row.id), [
        "item:hammer_sledge",
      ])
      const foreignMatch = await engine.invoke("autopickup_test:select", { row: rows[0].id })
      assertEquals(foreignMatch.error, "invalid_arguments")
      rec.command("match that the list does not hold", foreignMatch)
      assertEquals((await engine.invoke("QUIT")).stages.at(-1), "completed")

      // Pages are named by token; the swap moves the rule to the other page with an id of its own.
      const unknownRule = await engine.invoke("autopickup:select", { row: "rule:0" })
      assertEquals(unknownRule.error, "invalid_arguments")
      rec.command("rule that the list does not hold", unknownRule)
      const unknownTab = await engine.invoke("autopickup:tab", { row: "middle" })
      assertEquals(unknownTab.error, "invalid_arguments")
      rec.command("page that the tabs do not hold", unknownTab)
      const swap = await engine.invoke("SWAP_RULE_GLOBAL_CHAR")
      assertEquals(swap.stages.at(-1), "completed")
      rec.command("rule swapped to the other page", swap)
      assert(part(engine, "autopickup:tabs").tabs[1].selected, "the character page")
      const swapped = ids(engine, "autopickup:rules")
      assertEquals(swapped.length, 1)
      assert(!rows.some((row: Value) => row.id === swapped[0]), "the swapped rule has a new id")
      rec.accept("auto pickup manager on the character page", "screen", engine.top())
      const back = await engine.invoke("autopickup:tab", { row: "global" })
      assertEquals(back.stages.at(-1), "completed")
      rec.command("page picked by its token", back)
      await leave(engine)
    }),
}

export const safemode: Scenario = {
  area: "safemode",
  run: (rec) =>
    Engine.run(rec, async (engine) => {
      await intoTheTutorial(engine)
      await openManager(engine, "open_safemode", "SAFEMODE")
      rec.accept("safe mode manager without rules", "screen", engine.top())
      const tabs = part(engine, "safemode:tabs")
      assertEquals(tabs.tabs.map((tab: Value) => tab.id), ["global", "character"])
      assertEquals(tabs.select, "safemode:tab")
      assert(part(engine, "safemode:notice"), "the page says why it lists no rule")

      // A rule is added empty, and its text entered through its own affordance.
      const add = await engine.invoke("ADD_RULE")
      assertEquals(add.stages.at(-1), "completed")
      rec.command("safe mode rule added", add)
      const first = ids(engine, "safemode:rules")[0]
      const edit = await engine.invoke("safemode:edit", { row: first })
      assertEquals(edit.stages.at(-1), "completed")
      rec.command("rule text asked for", edit)
      assertEquals((await typeText(engine, "zombie")).stages.at(-1), "completed")
      assertEquals(part(engine, "safemode:rules").rows[0].label, "zombie")
      rec.accept("safe mode manager with a rule", "screen", engine.top())

      for (
        const [affordance, what] of [
          ["safemode:attitude", "attitude of a rule moved on"],
          ["safemode:category", "rule switched between monsters and sounds"],
          ["safemode:list", "rule switched between blacklist and whitelist"],
        ]
      ) {
        const done = await engine.invoke(affordance, { row: first })
        assertEquals(done.stages.at(-1), "completed", JSON.stringify(done))
        rec.command(what, done)
      }
      const rule = part(engine, "safemode:rules").rows[0]
      assertEquals(
        ["category", "whitelist"].map((key) =>
          rule.columns.find((c: Value) => c.key === key).value
        ),
        ["Sound", "Whitelist"],
      )
      assertEquals(ids(engine, "safemode:rules"), [first])

      // The default ruleset adds two rules; a copy has an id of its own.
      assertEquals((await engine.invoke("ADD_DEFAULT_RULESET")).stages.at(-1), "completed")
      const all = ids(engine, "safemode:rules")
      assertEquals(all.length, 3)
      assertEquals(all[0], first)
      assert(new Set(all).size === 3 && all.every((id: string) => /^rule:[1-9]\d*$/.test(id)))
      rec.accept("safe mode manager with the default ruleset", "screen", engine.top())
      const copied = await engine.invoke("COPY_RULE")
      assertEquals(copied.stages.at(-1), "completed")
      rec.command("safe mode rule copied", copied)
      const copy = ids(engine, "safemode:rules")
      assert(!all.includes(copy[3]), "the copy has a new id")
      assertEquals((await engine.invoke("REMOVE_RULE")).stages.at(-1), "completed")

      // The monsters a rule matches.
      const hostile = await engine.invoke("safemode:attitude", { row: first })
      assertEquals(hostile.stages.at(-1), "completed")
      assertEquals(
        (await engine.invoke("safemode:category", { row: first })).stages.at(-1),
        "completed",
      )
      assertEquals((await engine.invoke("TEST_RULE")).stages.at(-1), "completed")
      const matches = engine.top()
      assertEquals(matches.context, "SAFEMODE_TEST")
      rec.accept("monsters that a safe mode rule matches", "screen", matches)
      const found = part(engine, "safemode_test:matches").rows
      assert(found.length > 0 && found.every((row: Value) => row.id.startsWith("monster:")))
      const foreignMatch = await engine.invoke("safemode_test:select", { row: first })
      assertEquals(foreignMatch.error, "invalid_arguments")
      rec.command("safe mode match that the list does not hold", foreignMatch)
      assertEquals((await engine.invoke("QUIT")).stages.at(-1), "completed")

      const unknownRule = await engine.invoke("safemode:attitude", { row: "rule:0" })
      assertEquals(unknownRule.error, "invalid_arguments")
      rec.command("safe mode rule that the list does not hold", unknownRule)
      const character = await engine.invoke("safemode:tab", { row: "character" })
      assertEquals(character.stages.at(-1), "completed")
      rec.command("safe mode page picked by its token", character)
      assertEquals(part(engine, "safemode:rules").rows.length, 0)
      await leave(engine)
    }),
}
