#!/usr/bin/env -S deno run --allow-read --allow-run

/**
 * @module
 * Check LLVM's unrounded region, line and branch counts for the rot subsystem.
 * The manifest includes nested lambdas and fails if a required function is absent.
 * Complete source files are automatically included; the explicit manifest covers only rot methods
 * that remain in larger shared source files.  Unrelated item/map behavior is not the denominator.
 */
import { Command } from "@cliffy/command"
import * as v from "@valibot/valibot"

export const wholeFiles = ["rot.cpp", "rot/item_rot.cpp", "rot/rot_calculation.cpp"] as const

export const scope: Record<string, readonly string[]> = {
  "crafting.cpp": ["highest_component_relative_rot"],
  "item.cpp": [
    "item::prepare_for_location_removal",
    "get_most_rotten_component",
    "item::process_rot",
    "item::actualize_rot",
    "(anonymous namespace)::weighted_averaged_rot",
    "get_freshness_description",
  ],
  "map/mapbuffer.cpp": [
    "mapbuffer::handle_rotten_away_item",
    "(anonymous namespace)::rotten_item_spawn",
    "(anonymous namespace)::add_spawn_to_submap",
    "(anonymous namespace)::handle_decayed_corpse",
    "(anonymous namespace)::add_item_to_actualized_tile",
    "(anonymous namespace)::remove_rotten_items",
    "(anonymous namespace)::temperature_flag_at_tile",
  ],
  "location_vector.cpp": ["location_vector<item>::remove_with"],
}

const Export = v.object({
  type: v.literal("llvm.coverage.json.export"),
  data: v.array(v.object({
    functions: v.array(v.object({ name: v.string(), filenames: v.array(v.string()) })),
  })),
})

export type Counts = {
  regions: number
  missedRegions: number
  lines: number
  missedLines: number
  branches: number
  missedBranches: number
}
export type FunctionCoverage = { file: string; name: string; counts?: Counts }

/** Keep source-relative directories for both local and compiler-remapped paths. */
export const sourcePath = (filename: string, root: string): string => {
  const normalized = filename.replaceAll("\\", "/")
  const localSource = `${root.replaceAll("\\", "/")}/src/`
  if (normalized.startsWith(localSource)) return normalized.slice(localSource.length)
  if (normalized.startsWith("/src/")) return normalized.slice("/src/".length)
  return normalized
}

/** Parse the native LLVM function report, including its line-coverage accounting. */
export const parseReport = (text: string): Map<string, Counts> => {
  const result = new Map<string, Counts>()
  const row = /^(\S+)\s+(\d+)\s+(\d+)\s+\S+\s+(\d+)\s+(\d+)\s+\S+\s+(\d+)\s+(\d+)\s+\S+$/
  for (const line of text.split("\n")) {
    const match = row.exec(line.trim())
    if (!match) continue
    const [, name, regions, missedRegions, lines, missedLines, branches, missedBranches] = match
    result.set(name, {
      regions: Number(regions),
      missedRegions: Number(missedRegions),
      lines: Number(lines),
      missedLines: Number(missedLines),
      branches: Number(branches),
      missedBranches: Number(missedBranches),
    })
  }
  return result
}

const functionMatches = (name: string, stem: string) =>
  name.startsWith(`${stem}(`) || name.startsWith(`${stem}[`)

export const checkCoverage = (functions: readonly FunctionCoverage[]) => {
  const selected = functions.filter(({ file, name }) =>
    wholeFiles.includes(file as (typeof wholeFiles)[number]) ||
    scope[file]?.some((stem) => functionMatches(name, stem))
  )
  const missing = [
    ...wholeFiles
      .filter((file) => !functions.some((function_coverage) => function_coverage.file === file))
      .map((file) => `whole file: ${file}`),
    ...Object.entries(scope).flatMap(([file, names]) =>
      names.filter((stem) =>
        !selected.some((f) => f.file === file && functionMatches(f.name, stem))
      )
        .map((stem) => `${file}: ${stem}`)
    ),
  ]
  const failed = selected.filter(({ counts }) =>
    !counts || !counts.regions || !counts.lines || counts.missedRegions || counts.missedLines ||
    counts.missedBranches
  )
  return { selected, missing, failed, passed: missing.length === 0 && failed.length === 0 }
}

const run = async (
  { command, args, input }: { command: string; args: string[]; input?: string },
): Promise<string> => {
  const process = new Deno.Command(command, {
    args,
    stdin: input === undefined ? "null" : "piped",
    stdout: "piped",
    stderr: "piped",
  }).spawn()
  const output = process.output()
  if (input !== undefined) {
    const writer = process.stdin.getWriter()
    await writer.write(new TextEncoder().encode(input))
    await writer.close()
  }
  const { success, stdout, stderr } = await output
  if (!success) throw new Error(`${command}: ${new TextDecoder().decode(stderr)}`)
  return new TextDecoder().decode(stdout)
}

if (import.meta.main) {
  await new Command()
    .name("check-rot-coverage")
    .description(
      "Require 100% LLVM region, line and branch coverage for the explicit rot subsystem manifest.",
    )
    .arguments("<binary:string> <profile:string>")
    .action(async (_options, binary, profile) => {
      const args = [binary, `-instr-profile=${profile}`]
      const exported = v.parse(
        Export,
        JSON.parse(await run({ command: "llvm-cov", args: ["export", ...args] })),
      )
      const functions = exported.data.flatMap((entry) => entry.functions)
      const names = (await run({
        command: "llvm-cxxfilt",
        args: [],
        input: functions.map((f) => f.name.replace(/^.*\.cpp:/, "")).join("\n") + "\n",
      })).trimEnd().split("\n")
      if (names.length !== functions.length) {
        throw new Error("Demangler returned a different function count")
      }
      const root = Deno.cwd()
      const report = parseReport(
        await run({
          command: "llvm-cov",
          args: [
            "report",
            ...args,
            `-path-equivalence=/src,${root}/src`,
            "--show-functions",
            ...[...new Set([...wholeFiles, ...Object.keys(scope)])].map((file) =>
              `${root}/src/${file}`
            ),
          ],
        }),
      )
      const result = checkCoverage(functions.map((f, index) => ({
        file: sourcePath(f.filenames[0], root),
        name: names[index],
        counts: report.get(f.name),
      })))
      for (const f of result.selected) {
        const c = f.counts
        console.log(
          `${f.file}: ${f.name}\n  ${
            c
              ? `regions ${c.regions - c.missedRegions}/${c.regions}; lines ${
                c.lines - c.missedLines
              }/${c.lines}; branches ${c.branches - c.missedBranches}/${c.branches}`
              : "MISSING LLVM SUMMARY"
          }`,
        )
      }
      for (const missing of result.missing) {
        console.error(`Missing function: ${missing}`)
      }
      if (!result.passed) {
        console.error(
          `FAIL: ${result.failed.length} incomplete summaries, ${result.missing.length} missing functions`,
        )
        Deno.exitCode = 1
      } else {
        console.log(
          `PASS: ${result.selected.length} functions/lambdas have 100% region, line and branch coverage`,
        )
      }
    }).parse(Deno.args)
}
