/**
 * @module
 * The wire before a game exists: the hello and its refusal, the first snapshot, a startup prompt
 * answered, and the main menu with the errors an invoke meets there.
 */
import { assert, assertEquals } from "@std/assert"
import { Engine, helloClient, type Scenario, schemaHash, type Value, without } from "./driver.ts"
import { atMainMenu } from "../mirror.ts"

export const handshake: Scenario = {
  area: "handshake",
  run: (rec) =>
    Engine.run(rec, async (engine) => {
      const wrong = "sha256:0000000000000000"
      const refused = await engine.request("bn.hello", {
        versions: ["0.2"],
        client: helloClient,
        schema: wrong,
      })
      assertEquals(refused.error.data.kind, "negotiation_failed")
      rec.accept(
        "hello refused for a schema hash the engine was not built with",
        "application_error",
        refused.error.data,
      )

      const { params, result } = await engine.hello()
      assertEquals(result.schema, schemaHash)
      rec.accept(
        "hello of a 0.2 client that sends its schema hash and a viewport",
        "hello_params",
        params,
      )
      rec.accept("hello result of a 0.2 controller", "hello_result", result)
      rec.reject(
        "0.2 hello without the schema hash",
        "hello_params",
        without(params, "schema"),
        "a client that offers 0.2 names the schema it was built with",
      )
      rec.reject(
        "0.2 hello result without the role",
        "hello_result",
        without(result, "role"),
        "the connection learns whether it controls the game",
      )

      const { header } = await engine.subscribe()
      rec.accept("subscribe result before the main menu", "subscribe_result", header)

      await engine.acknowledge(() => atMainMenu(engine.mirror))

      const menu = engine.mirror.interaction
      rec.accept("boundary of the main menu", "interaction_boundary", menu)
      for (const [n, screen] of menu.stack.entries()) {
        rec.accept(`screen ${n + 1} of the main menu stack`, "screen", screen)
      }

      // An affordance the screen does not offer is refused.
      const unknown = await engine.invoke("no-such-affordance")
      assertEquals(unknown.error, "unknown_affordance")
      rec.command("invoke of an affordance the screen does not offer", unknown)
    }),
}
