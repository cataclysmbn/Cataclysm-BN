/**
 * @module
 * New Game from the main menu with a custom character: the world creation tabs (mods, options, finalize)
 * with a jump over a tab, then every tab of the character creation, the name popup of the overview, and the
 * finish into the new world.
 */
import { assert, assertEquals } from "@std/assert"
import { Engine, type Scenario, type Value } from "./driver.ts"

const completed = (outcome: { stages: string[] }) =>
  assertEquals(outcome.stages.at(-1), "completed")

export const newgame: Scenario = {
  area: "newgame",
  run: (rec) =>
    Engine.run(rec, async (engine) => {
      await engine.toMainMenu()
      completed(await engine.invoke("menu:tab", { row: "new_game" }))
      completed(await engine.invoke("menu:entry", { row: "new_game:custom" }))

      // World creation: the mods page, a tab strip over the mod lists.
      const mods = engine.top()
      assertEquals(mods.title, "World creation")
      rec.accept("world creation on the mods page", "screen", mods)
      const category = mods.affordances.find((entry: Value) =>
        entry.id === "worldgen:mods:category"
      )
      assert(category, "a category affordance")
      rec.accept("category affordance of the mod lists", "affordance", category)
      const nowhere = await engine.invoke("worldgen:tab", { row: "nowhere" })
      assertEquals(nowhere.error, "invalid_arguments")
      rec.command("world creation tab invoked that does not exist", nowhere)
      const unknownMod = await engine.invoke("worldgen:mods:add", { row: "mod:no_such_mod" })
      assertEquals(unknownMod.error, "invalid_arguments")
      rec.command("mod added that the list does not hold", unknownMod)

      // The options page is skipped by the jump to the last page and entered by the next-tab key.
      const options = await engine.invoke("worldgen:tab", { row: "options" })
      completed(options)
      rec.command("options page of the world creation invoked by its tab", options)
      completed(await engine.invoke("NEXT_TAB"))

      const confirm = engine.top()
      assertEquals(confirm.title, "World creation")
      rec.accept("world creation on the finalize page", "screen", confirm)
      const name = confirm.affordances.find((entry: Value) => entry.id === "worldgen:confirm:name")
      assert(name, "a name affordance")
      rec.accept("world name affordance", "affordance", name)
      const typed = await engine.invoke("worldgen:confirm:name", { text: "Ada" })
      completed(typed)
      rec.command("world name typed", typed)
      completed(await engine.invoke("NEXT_TAB"))
      completed(await engine.invoke("YES"))

      // Character creation: every tab, by id.
      const points = engine.top()
      assertEquals(points.title, "Character creation")
      rec.accept("character creation on the points page", "screen", points)
      const tabs: string[] = points.root.tabs.map((entry: Value) => entry.id)
      assertEquals(tabs[0], "points")
      assertEquals(tabs.at(-1), "overview")
      const select = points.affordances.find((entry: Value) => entry.id === "newchar:tab")
      assert(select, "a tab affordance")
      rec.accept("tab affordance of the character creation", "affordance", select)
      const unknown = await engine.invoke("newchar:tab", { row: "nowhere" })
      assertEquals(unknown.error, "invalid_arguments")
      rec.command("character creation tab invoked that does not exist", unknown)

      for (const tab of tabs.slice(1)) {
        const jump = await engine.invoke("newchar:tab", { row: tab })
        completed(jump)
        if (tab === "traits") rec.command("traits tab invoked from the points page", jump)
        const screen = engine.top()
        assertEquals(screen.root.tabs.find((entry: Value) => entry.selected).id, tab)
        rec.accept(`character creation on the ${tab} page`, "screen", screen)
      }

      // The overview names the character through the popup of its name row.
      const field = await engine.invoke("bn:activate:newchar:overview:pick", { row: "name" })
      completed(field)
      rec.command("name row picked on the overview", field)
      completed(await engine.invoke("bn:submit:form:field", { text: "Ada" }))
      completed(await engine.invoke("NEXT_TAB"))
      // Finishing asks twice; the answer to the second starts the new world, so the old one is gone.
      completed(await engine.invoke("YES"))
      const start = await engine.invoke("YES")
      assertEquals(start.error, "stale_epoch")
      rec.command("character creation finished into a new world", start)

      await engine.follow(() => engine.stale)
      await engine.subscribe()
      await engine.acknowledge(() => engine.mirror.interaction.interaction === null)
      assert(engine.loading.at(-1)?.done, "loading ends with done")
      assertEquals(engine.top().role, "map")
      rec.accept("loading of the created world done", "loading_notification", engine.loading.at(-1))
    }),
}
