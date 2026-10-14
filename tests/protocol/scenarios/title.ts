/**
 * @module
 * The title screen: a banner and a strip of tabs over the entries of the selected one, a tab picked by id,
 * a tab that does not exist, and the tab whose content is text.
 */
import { assert, assertEquals } from "@std/assert"
import { Engine, type Scenario, type Value } from "./driver.ts"

export const title: Scenario = {
  area: "title",
  run: (rec) =>
    Engine.run(rec, async (engine) => {
      await engine.toMainMenu()
      const menu = engine.top()
      const banner = menu.root.children[0]
      assertEquals(banner.role, "banner")
      rec.accept("title screen with its banner and tabs", "screen", menu)
      const pick = menu.affordances.find((entry: Value) => entry.id === "menu:tab")
      assert(pick, "a tab affordance")
      rec.accept("tab affordance of the title screen", "affordance", pick)

      const unknown = await engine.invoke("menu:tab", { row: "tab:nothing" })
      assertEquals(unknown.error, "invalid_arguments")
      rec.command("tab invoked that the title screen does not have", unknown)

      const credits = await engine.invoke("menu:tab", { row: "credits" })
      assertEquals(credits.stages.at(-1), "completed")
      rec.command("credits tab invoked", credits)
      const text = engine.top()
      assert(
        !text.affordances.some((entry: Value) => entry.id === "menu:entry"),
        "the credits are text, so no entry can be picked",
      )
      rec.accept("title screen on the credits tab", "screen", text)
    }),
}
