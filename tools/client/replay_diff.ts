#!/usr/bin/env -S deno run --allow-read --allow-write --allow-run --allow-env
/**
 * @module
 * Plays one input recording on two builds and reports where they part: the first logged turn whose RNG,
 * avatar, monsters, NPCs or map hash differs, then every saved record that differs.
 *
 * `deno run -A tools/client/replay_diff.ts --recording <inputs.jsonl> --profile <initial user dir>
 *   --base <binary> --head <binary> <output>` exits 0 when both runs agree, 1 when they part, 2 on error.
 */
import { Command } from "@cliffy/command"
import { copy } from "@std/fs"
import { dirname, join, resolve } from "@std/path"
import { compare } from "./compare_saves.ts"

/** One turn of a `--state-hash-log`. */
export type StateRecord = { step: number; turn: number; parts: Record<string, string> }

/** Where two state logs part: the parts that differ at `step`, or the step one log ends at. */
export type Divergence = {
  step: number
  base_turn: number | null
  head_turn: number | null
  parts: string[]
}

/** The turns of a state hash log; refuses another format or version. */
export function parse_state_log(text: string): StateRecord[] {
  const lines = text.split("\n").filter((line) => line !== "")
  const header = JSON.parse(lines[0] ?? "null")
  if (
    header?.kind !== "header" || header.format !== "cataclysm-bn-state-hash" || header.version !== 1
  ) {
    throw new Error(`not a version 1 state hash log: ${lines[0]}`)
  }
  return lines.slice(1).map((line, index) => {
    const record = JSON.parse(line)
    if (record.kind !== "state" || record.step !== index + 1) {
      throw new Error(`state record ${index + 1} is malformed: ${line}`)
    }
    return { step: record.step, turn: record.turn, parts: record.parts }
  })
}

/** The first step where `base` and `head` differ, if any. */
export function first_divergence(base: StateRecord[], head: StateRecord[]): Divergence | null {
  for (let i = 0; i < Math.max(base.length, head.length); i++) {
    const [a, b] = [base[i], head[i]]
    const names = [...new Set([...Object.keys(a?.parts ?? {}), ...Object.keys(b?.parts ?? {})])]
      .sort()
    const parts = !a || !b ? names : names.filter((name) => a.parts[name] !== b.parts[name])
    if (parts.length || a?.turn !== b?.turn) {
      return { step: i + 1, base_turn: a?.turn ?? null, head_turn: b?.turn ?? null, parts }
    }
  }
  return null
}

type Side = { name: string; binary: string; root: string }

/** Plays `recording` on `side` from a copy of `profile`; the copy, its state log and stderr stay in `dir`. */
async function play(side: Side, recording: string, profile: string, dir: string) {
  const user = join(dir, "user")
  await Deno.mkdir(dir, { recursive: true })
  await copy(profile, user)
  const base = [
    `--basepath`,
    `${side.root}/`,
    "--client=mcp",
    "--userdir",
    `${user}/`,
    "--configdir",
    `${user}/config/`,
  ]
  const paths = await new Deno.Command(side.binary, {
    args: [...base, "--paths"],
    stdout: "piped",
    stderr: "null",
  })
    .output()
  const resolved = new TextDecoder().decode(paths.stdout)
  if (
    !resolved.includes(`User Directory: ${user}/`) ||
    !resolved.includes(`Config Directory: ${user}/config/`)
  ) {
    throw new Error(`${side.name} does not run in its own profile:\n${resolved}`)
  }
  const log = join(dir, "state.jsonl")
  const stderr = await Deno.open(join(dir, "game.stderr"), { write: true, create: true })
  const child = new Deno.Command(side.binary, {
    args: [...base, "--replay-play", recording, "--state-hash-log", log],
    stdin: "null",
    stdout: "null",
    stderr: "piped",
  }).spawn()
  await child.stderr.pipeTo(stderr.writable)
  const status = await child.status
  if (!status.success) {
    throw new Error(`${side.name} playback exited with ${status.code}; see ${dir}/game.stderr`)
  }
  return { user, states: parse_state_log(await Deno.readTextFile(log)) }
}

/** Plays `recording` on `base` and `head` and writes what parts to `output`, which must not exist yet. */
export async function replay_diff(
  options: { recording: string; profile: string; base: Side; head: Side; output: string },
) {
  await Deno.mkdir(options.output)
  const base = await play(
    options.base,
    options.recording,
    options.profile,
    join(options.output, "base"),
  )
  const head = await play(
    options.head,
    options.recording,
    options.profile,
    join(options.output, "head"),
  )
  const divergence = first_divergence(base.states, head.states)
  // The saved worlds; the rest of the profile holds logs stamped with the wall clock.
  const saves = await compare(
    join(base.user, "save"),
    join(head.user, "save"),
    join(options.output, "saves"),
  )
  const summary = {
    recording: options.recording,
    base: options.base,
    head: options.head,
    turns: { base: base.states.length, head: head.states.length },
    first_divergence: divergence,
    saves_equal: saves.all_logical_records_equal,
    save_changes: saves.changes.slice(0, 20),
    agree: divergence === null && saves.all_logical_records_equal,
  }
  await Deno.writeTextFile(
    join(options.output, "summary.json"),
    JSON.stringify(summary, null, 2) + "\n",
  )
  return summary
}

if (import.meta.main) {
  await new Command()
    .name("replay-diff")
    .description(
      "Play one input recording on two builds and report the first turn and the saves that differ.",
    )
    .option("--recording <path:string>", "Input recording (--replay-record).", { required: true })
    .option("--profile <dir:string>", "The user directory the recording started from.", {
      required: true,
    })
    .option("--base <binary:string>", "The build to compare against.", { required: true })
    .option("--head <binary:string>", "The build under test.", { required: true })
    .option("--base-root <dir:string>", "Game root of --base; the binary's directory when absent.")
    .option("--head-root <dir:string>", "Game root of --head; the binary's directory when absent.")
    .arguments("<output:string>")
    .action(async (options, output) => {
      const side = (name: string, binary: string, root?: string) => ({
        name,
        binary: resolve(binary),
        root: resolve(root ?? dirname(binary)),
      })
      try {
        const summary = await replay_diff({
          recording: resolve(options.recording),
          profile: resolve(options.profile),
          base: side("base", options.base, options.baseRoot),
          head: side("head", options.head, options.headRoot),
          output: resolve(output),
        })
        console.log(JSON.stringify(summary, null, 2))
        if (!summary.agree) Deno.exitCode = 1
      } catch (error) {
        console.error(error instanceof Error ? error.message : String(error))
        Deno.exitCode = 2
      }
    })
    .parse(Deno.args)
}
