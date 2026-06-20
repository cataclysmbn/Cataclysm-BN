#!/usr/bin/env -S deno run --allow-read --allow-write --allow-run=git --allow-env

/**
 * @module
 *
 * gets list of all affected files
 */

import { MuxAsyncIterator } from "jsr:@std/async"
import { partition } from "jsr:@std/collections"
import { walk, type WalkEntry, type WalkOptions } from "jsr:@std/fs"
import { Command } from "@cliffy/command"
import { changedFilesFromGit, type PullFileStatus } from "./git_changed_files.ts"

const paths = ["src", "tests"]

const ACMRT: Set<PullFileStatus> = new Set(["added", "copied", "modified", "renamed", "changed"])
const getDiffs = async (base: string, head: string) => {
  const pathsFilter = new RegExp(`^(?:${paths.join("|")})/.*\\.(?:cpp|h|hpp)$`)

  return (await changedFilesFromGit({ base, head }))
    .filter((x) => ACMRT.has(x.status))
    .map((x) => x.filename)
    .filter((x) => pathsFilter.test(x))
}

const includeRegex = /#include\s+"([^"]+)"/g
const getIncludes = (content: string) => Array.from(content.matchAll(includeRegex)).map((x) => x[1])

const getSourceDependencies = (nameToPath: Map<string, string>) => async ({ path }: WalkEntry) => {
  const text = await Deno.readTextFile(path)
  const includes = getIncludes(text).map((x) => nameToPath.get(x)!)
  return [path, includes] as const
}

const getAllSourceFiles = async (): Promise<WalkEntry[]> => {
  const walkOption: WalkOptions = { includeDirs: false, exts: [".cpp", ".h"], skip: [/\.tmp/] }

  const reader = new MuxAsyncIterator<WalkEntry>()
  paths.forEach((path) => reader.add(walk(path, walkOption)))

  return await Array.fromAsync(reader)
}

type Deps = Map<string, Set<string>>

/**
 * creates mapping between header file and all source files that include it
 */
const getAllDependencies = async (xs: WalkEntry[]): Promise<Deps> => {
  const nameToPath = new Map(xs.map((x) => [x.name, x.path]))
  const ys = await Promise.all(xs.map(getSourceDependencies(nameToPath)))

  const deps: Deps = new Map()
  for (const [path, includes] of ys) {
    for (const include of includes) {
      deps.set(include, (deps.get(include) ?? new Set()).add(path))
    }
  }
  return deps
}

const main = new Command()
  .description("gets list of all affected files for use in clang-tidy.")
  .arguments("[pr:number]")
  .option("--base <rev:string>", "Base git revision", { default: "origin/main" })
  .option("--head <rev:string>", "Head git revision", { default: "HEAD" })
  .option("-o, --output <file>", "Output file to save template to")
  .action(async ({ base, head, output }) => {
    const [deps, [headers, src]] = await Promise.all([
      getAllSourceFiles().then(getAllDependencies),
      getDiffs(base, head).then((xs) => partition(xs, (x) => x.endsWith(".h"))),
    ])
    // console.log(deps)
    console.log({ src, headers })

    const affected = new Set(src.concat(headers.flatMap((x) => Array.from(deps.get(x) ?? []))))
    console.log(affected)

    if (output) await Deno.writeTextFile(output, Array.from(affected).join("\n"))
  })

if (import.meta.main) {
  await main.parse(Deno.args)
}
