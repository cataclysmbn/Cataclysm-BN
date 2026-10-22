#!/usr/bin/env -S deno run --allow-read --allow-write --allow-run --allow-env
/**
 * @module
 * Records an input recording that `replay_diff.ts` plays: a new game of the main menu's `--entry`, `--waits`
 * waits of the first length the wait menu offers, then a save and a clean quit.
 *
 * `deno run -A tools/client/replay_record.ts --binary <engine> <output>` writes `<output>/profile` (the user
 * directory the recording starts from, untouched), `<output>/record` (the one it played in) and
 * `<output>/inputs.jsonl`.
 */
import { Command } from "@cliffy/command"
import { copy } from "@std/fs"
import { dirname, join, resolve } from "@std/path"
import { Engine, makeProfile, Recorder, type Value } from "../../tests/protocol/scenarios/driver.ts"
import { atMainMenu } from "../../tests/protocol/mirror.ts"

/** Closes the pop-ups over the map, by their cancel, until the map is on top. */
async function dismiss(engine: Engine) {
  for (let n = 0; engine.mirror.interaction.interaction !== null && n < 20; n++) {
    const cancel = engine.top().affordances.find((entry: Value) => entry.role === "cancel")
    if (!cancel) throw new Error(`a pop-up without a cancel: ${engine.top().context}`)
    await engine.invoke(cancel.id)
  }
}

export async function record(
  options: { binary: string; root: string; entry: string; waits: number; output: string },
) {
  await Deno.mkdir(options.output)
  const initial = await makeProfile()
  const profile = join(options.output, "profile")
  const played = join(options.output, "record")
  await copy(initial, profile)
  await copy(initial, played)
  await Deno.remove(initial, { recursive: true })
  const recording = join(options.output, "inputs.jsonl")
  const engine = new Engine(new Recorder(), played, options.binary, [
    "--basepath",
    `${options.root}/`,
    "--replay-record",
    recording,
  ])
  try {
    await engine.toMainMenu()
    await engine.invoke("menu:tab", { row: "new_game" })
    await engine.invoke("menu:entry", { row: options.entry })
    await engine.follow(() => engine.stale)
    await engine.subscribe()
    await dismiss(engine)
    for (let n = 0; n < options.waits; n++) {
      await engine.submit({ kind: "action", action_id: "wait" })
      await engine.choose("entry:0")
      await dismiss(engine)
    }
    await engine.submit({ kind: "action", action_id: "save" })
    // Lessons and warnings come before the question whether to save and quit.
    while (engine.top().context !== "YESNO") {
      await engine.invoke(
        engine.top().affordances.find((entry: Value) => entry.role === "cancel").id,
      )
    }
    await engine.invoke("YES")
    await engine.follow(() => engine.stale)
    await engine.subscribe()
    await engine.acknowledge(() => atMainMenu(engine.mirror))
    await engine.invoke("QUIT")
    await engine.invoke("YES")
    const status = await engine.exited()
    if (!status.success) throw new Error(`the recording engine exited with ${status.code}`)
  } finally {
    engine.kill()
  }
  return { recording, profile }
}

if (import.meta.main) {
  await new Command()
    .name("replay-record")
    .description("Record a new game, some waits, a save and a quit for replay_diff.ts.")
    .option("--binary <path:string>", "Engine to record with.", { required: true })
    .option("--root <dir:string>", "Game root; the binary's directory when absent.")
    .option("--entry <id:string>", "Main menu new game entry.", { default: "new_game:tutorial" })
    .option("--waits <n:integer>", "Waits before saving.", { default: 10 })
    .arguments("<output:string>")
    .action(async (options, output) => {
      const binary = resolve(options.binary)
      const result = await record({
        binary,
        root: resolve(options.root ?? dirname(binary)),
        entry: options.entry,
        waits: options.waits,
        output: resolve(output),
      })
      console.log(JSON.stringify(result, null, 2))
    })
    .parse(Deno.args)
}
