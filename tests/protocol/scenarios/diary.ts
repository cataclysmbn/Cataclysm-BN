/**
 * @module
 * The diary and its text editor: pages named by a serial of the page, two pages made in the same turn told
 * apart, one opened by its row and a row the diary does not hold refused, a page's text set through the editor
 * as a whole and confirmed, and a page deleted without moving the id of the page after it.
 */
import { assert, assertEquals } from "@std/assert"
import { Engine, type Scenario, type Value } from "./driver.ts"
import { walk } from "./inventory.ts"

const part = (screen: Value, id: string) => walk(screen.root).find((entry) => entry.id === id)

export const diary: Scenario = {
  area: "diary",
  run: (rec) =>
    Engine.run(rec, async (engine) => {
      await engine.toMainMenu()
      await engine.enterTutorial()
      await engine.submit({ kind: "action", action_id: "wait" })
      await engine.choose("entry:0")
      await engine.acknowledge(() => engine.mirror.interaction.interaction === null)

      assertEquals(
        (await engine.submit({ kind: "action", action_id: "diary" })).stages.at(-1),
        "completed",
      )
      await engine.follow(() => engine.top().context === "DIARY")
      const empty = engine.top()
      assertEquals(part(empty, "diary:pages:list").rows.length, 0)
      rec.accept("diary screen without pages", "screen", empty)

      // Two pages made in the same turn: nothing in the diary tells them apart but their serials.
      for (let page = 0; page < 2; page++) {
        const made = await engine.invoke("NEW_PAGE")
        assertEquals(made.stages.at(-1), "completed", JSON.stringify(made))
      }
      const diary = engine.top()
      const pages = part(diary, "diary:pages:list").rows
      assertEquals(pages.length, 2)
      assertEquals(pages[0].label, pages[1].label)
      assert(pages[0].id !== pages[1].id, "equal pages have different ids")
      rec.accept("diary screen with two pages", "screen", diary)
      const select = diary.affordances.find((entry: Value) => entry.id === "diary:page")
      assert(select, "the page affordance")
      rec.accept("page affordance of the diary", "affordance", select)

      for (const row of ["page:0", "nothing", pages[0].id + "0"]) {
        const refused = await engine.invoke("diary:page", { row })
        assertEquals(refused.error, "invalid_arguments")
      }
      rec.command(
        "page the diary does not hold refused",
        await engine.invoke("diary:page", {
          row: "page:0",
        }),
      )
      const opened = await engine.invoke("diary:page", { row: pages[0].id })
      assertEquals(opened.stages.at(-1), "completed", JSON.stringify(opened))
      rec.command("page picked", opened)
      assert(part(engine.top(), "diary:pages:list").rows[0].selected, "the first page is open")

      // The editor takes the whole text at once.
      assertEquals((await engine.invoke("CONFIRM")).stages.at(-1), "completed")
      await engine.follow(() => engine.top().context === "STRING_EDITOR")
      const editor = engine.top()
      rec.accept("text editor screen", "screen", editor)
      const field = part(editor, "editor:form").fields[0]
      assertEquals([field.multiline, field.cursor, field.value], [true, 0, ""])
      for (const args of [{}, { row: "editor:text", text: "x" }]) {
        assertEquals((await engine.invoke("editor:set", args)).error, "invalid_arguments")
      }
      const set = await engine.invoke("editor:set", { text: "Found a rifle.\nSafe here." })
      assertEquals(set.stages.at(-1), "completed", JSON.stringify(set))
      rec.command("text set", set)
      const typed = part(engine.top(), "editor:form").fields[0]
      assertEquals([typed.value, typed.cursor], ["Found a rifle.\nSafe here.", 25])
      assertEquals((await engine.invoke("TEXT.CONFIRM")).stages.at(-1), "completed")
      await engine.follow(() => engine.top().context === "DIARY")
      assertEquals(part(engine.top(), "diary:page:text").markup, "Found a rifle.\nSafe here.")

      // Deleting the first page leaves the second under the id it had.
      assertEquals((await engine.invoke("DELETE PAGE")).stages.at(-1), "completed")
      await engine.follow(() => engine.top().context === "YESNO")
      assertEquals((await engine.invoke("YES")).stages.at(-1), "completed")
      await engine.follow(() => engine.top().context === "DIARY")
      assertEquals(part(engine.top(), "diary:pages:list").rows.map((row: Value) => row.id), [
        pages[1].id,
      ])
      assertEquals((await engine.invoke("QUIT")).stages.at(-1), "completed")
      assertEquals(engine.top().role, "map")
    }),
}
