/**
 * @module
 * The character screens of the tutorial avatar: the sheet with its tabs and a tab and a row picked by their
 * own affordances; the bionics list of two installed bionics, one that cannot be activated and says why;
 * the mutation list of a passive and an active trait; and the spell menu with a spell favorited.
 */
import { assert, assertEquals } from "@std/assert"
import { Engine, type Scenario, type Value } from "./driver.ts"
import { walk } from "./inventory.ts"

const part = (screen: Value, id: string) => walk(screen.root).find((entry) => entry.id === id)

/** The row of the list on top whose label satisfies `match`, picked through the list's own affordance. */
const chooseRow = async (engine: Engine, match: (label: string) => boolean) => {
  const list = walk(engine.top().root).find((component) => component.kind === "list")
  const row = list?.rows.find((entry: Value) => match(entry.label))
  assert(row, `no row in ${JSON.stringify(list?.rows.map((entry: Value) => entry.label))}`)
  assertEquals((await engine.invoke(list.activate, { row: row.id })).stages.at(-1), "completed")
}

/** Types `text` into the filter of the list on top. */
const filter = async (engine: Engine, text: string) => {
  assertEquals((await engine.invoke("FILTER")).stages.at(-1), "completed")
  const confirm = engine.top().affordances.find((entry: Value) => entry.role === "confirm")
  assert(confirm, "the filter prompt confirms")
  assertEquals((await engine.invoke(confirm.id, { text })).stages.at(-1), "completed")
}

const toMap = async (engine: Engine) => {
  for (let step = 0; engine.top().role !== "map" && step < 8; step++) {
    const cancel = engine.top().affordances.find((entry: Value) => entry.role === "cancel")
    assert(cancel, "a way out")
    await engine.invoke(cancel.id)
  }
  assertEquals(engine.top().role, "map")
}

/** The debug menu's character editor, the entry `edit` of it chosen. */
const editCharacter = async (engine: Engine, edit: string) => {
  assertEquals(
    (await engine.submit({ kind: "action", action_id: "debug" })).stages.at(-1),
    "completed",
  )
  await chooseRow(engine, (label) => label === "Player…")
  await chooseRow(engine, (label) => label === edit)
}

export const character: Scenario = {
  area: "character",
  run: (rec) =>
    Engine.run(rec, async (engine) => {
      await engine.toMainMenu()
      await engine.enterTutorial()
      await engine.submit({ kind: "action", action_id: "wait" })
      await engine.choose("entry:0")
      await engine.acknowledge(() => engine.mirror.interaction.interaction === null)

      // The sheet.
      assertEquals(
        (await engine.submit({ kind: "action", action_id: "player_data" })).stages.at(-1),
        "completed",
      )
      const sheet = engine.top()
      assertEquals(sheet.context, "PLAYER_INFO")
      rec.accept("character sheet", "screen", sheet)
      const tabs = part(sheet, "character:tabs")
      assert(tabs && tabs.kind === "tabs", "the sheet's tabs")
      assertEquals(tabs.tabs.length, 6)
      assert(part(sheet, "character:stats:rows").rows.length >= 4, "the stat rows")
      assert(part(sheet, "character:speed").stats.length > 0, "the speed breakdown")
      const tab = await engine.invoke("character:tab", { row: "skills" })
      assertEquals(tab.stages.at(-1), "completed")
      rec.command("sheet tab picked", tab)
      assert(part(engine.top(), "character:tabs").tabs[2].selected, "the picked tab is selected")
      const unknown = await engine.invoke("character:tab", { row: "nowhere" })
      assertEquals(unknown.error, "invalid_arguments")
      rec.command("sheet tab the sheet does not hold", unknown)
      const skills = part(engine.top(), "character:skills:rows").rows
      const skill = skills.find((row: Value) => !row.header)
      assert(skill, "a skill row")
      const focus = await engine.invoke("character:focus:skills", { row: skill.id })
      assertEquals(focus.stages.at(-1), "completed")
      rec.command("sheet row picked", focus)
      assertEquals(engine.top().focus?.row_id ?? engine.top().focus_row, skill.id)
      assertEquals((await engine.invoke("QUIT")).stages.at(-1), "completed")
      assertEquals(engine.top().role, "map")

      // Two bionics: one to switch on, one that is always on.
      for (const name of ["flashlight", "power storage"]) {
        await editCharacter(engine, "Edit [b]ionics")
        await chooseRow(engine, (label) => label === "Install from CBM…")
        await filter(engine, name)
        await chooseRow(engine, (label) => label.toLowerCase().includes(name))
        await toMap(engine)
      }
      assertEquals(
        (await engine.submit({ kind: "action", action_id: "bionics" })).stages.at(-1),
        "completed",
      )
      const bionics = engine.top()
      assertEquals(bionics.context, "BIONICS")
      rec.accept("bionics screen", "screen", bionics)
      const bionicRows = part(bionics, "bionics:list").rows
      assert(bionicRows.length > 0, "an active bionic")
      const denied = bionics.denials?.find?.((entry: Value) => entry.id === "CONFIRM")
      const confirm = bionics.affordances.find((entry: Value) => entry.id === "CONFIRM")
      assert(confirm, "the confirm of the cursor row")
      rec.accept("confirm of a bionic that cannot be switched on", "affordance", confirm)
      assert(!confirm.enabled && confirm.denial || denied === undefined, "a denial says why")
      const passive = await engine.invoke("bionics:tab", { row: "passive" })
      assertEquals(passive.stages.at(-1), "completed")
      rec.command("bionics tab picked", passive)
      const passiveRows = part(engine.top(), "bionics:list").rows
      assert(passiveRows.length > 0, "a passive bionic")
      const pickedBionic = await engine.invoke("bionics:pick", { row: passiveRows[0].id })
      assertEquals(pickedBionic.stages.at(-1), "completed")
      rec.command("bionic picked", pickedBionic)
      // A bionic that is always on says so in a popup, which is closed.
      if (engine.top().context !== "BIONICS") {
        const key = engine.top().affordances.find((entry: Value) => entry.role === "cancel")
        assert(key, "the popup is closed")
        await engine.invoke(key.id)
      }
      assertEquals(engine.top().context, "BIONICS")
      const unknownBionic = await engine.invoke("bionics:pick", { row: "bionic:nothing" })
      assertEquals(unknownBionic.error, "invalid_arguments")
      rec.command("pick of a bionic the list does not hold", unknownBionic)
      // Reassigning a letter asks for a key in a screen of its own.
      assertEquals((await engine.invoke("REASSIGN")).stages.at(-1), "completed")
      assertEquals(
        (await engine.invoke("bionics:pick", { row: passiveRows[0].id })).stages.at(-1),
        "completed",
      )
      const letter = engine.top()
      assertEquals(letter.context, "POPUP_WAIT")
      rec.accept("bionic letter popup", "screen", letter)
      const setLetter = letter.affordances.find((entry: Value) => entry.id === "bionics:letter:set")
      assert(setLetter && setLetter.role === "confirm", "the letter is set by a confirm")
      const noLetter = await engine.invoke(setLetter.id)
      assertEquals(noLetter.error, "invalid_arguments")
      rec.command("letter set without a key", noLetter)
      assertEquals((await engine.invoke("QUIT")).stages.at(-1), "completed")
      assertEquals(engine.top().context, "BIONICS")
      for (let step = 0; engine.top().role !== "map" && step < 4; step++) {
        await engine.invoke("QUIT")
      }
      assertEquals(engine.top().role, "map")

      // A passive and an active trait.
      await editCharacter(engine, "M[u]tate")
      for (const name of ["night vision", "weak photophore"]) {
        await filter(engine, name)
        await chooseRow(engine, (label) => label.toLowerCase().includes(name))
      }
      await toMap(engine)
      assertEquals(
        (await engine.submit({ kind: "action", action_id: "mutations" })).stages.at(-1),
        "completed",
      )
      const mutations = engine.top()
      assertEquals(mutations.context, "MUTATIONS")
      rec.accept("mutation screen", "screen", mutations)
      const passiveTraits = part(mutations, "mutations:passive:rows").rows
      const activeTraits = part(mutations, "mutations:active:rows").rows
      assert(passiveTraits.length > 0 && activeTraits.length > 0, "a trait of each kind")
      const traitPick = await engine.invoke("mutations:pick:active", { row: activeTraits[0].id })
      assertEquals(traitPick.stages.at(-1), "completed")
      rec.command("mutation picked", traitPick)
      await toMap(engine)

      // The spell menu.
      await editCharacter(engine, "[L]earn all Spells")
      await toMap(engine)
      assertEquals(
        (await engine.submit({ kind: "action", action_id: "cast_spell" })).stages.at(-1),
        "completed",
      )
      const spells = engine.top()
      assertEquals(spells.context, "SPELL_SELECT")
      rec.accept("spell menu", "screen", spells)
      const spellRows = part(spells, "list:choices").rows.filter((row: Value) =>
        row.id.startsWith("spell:")
      )
      assert(spellRows.length > 0, "known spells")
      const favorite = await engine.invoke("TOGGLE_FAVORITE")
      assertEquals(favorite.stages.at(-1), "completed")
      rec.command("spell favorited", favorite)
      assert(
        part(engine.top(), "list:choices").rows.some((row: Value) => row.favorite),
        "the favorite spell is marked",
      )
      rec.accept("spell menu with a spell favorited", "screen", engine.top())
      await toMap(engine)
    }),
}
