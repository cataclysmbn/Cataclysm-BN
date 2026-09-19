#!/usr/bin/env -S deno run --allow-read

/**
 * @module
 *
 * Selects all non-blacklisted mods whose dependencies are available.
 */

import { Command } from "@cliffy/command"
import { walk } from "@std/fs"
import { parse as parseJsonc } from "@std/jsonc"

type ModInfo = {
  type: string
  id: string
  dependencies?: string[]
  obsolete?: boolean
}

/** Returns mods and their dependencies in metadata discovery order. */
export const getAllMods = async (
  modsRoot: string,
  blacklist: ReadonlySet<string>,
): Promise<string[]> => {
  const allModDependencies = new Map<string, string[]>()
  for await (
    const entry of walk(modsRoot, {
      maxDepth: 2,
      includeDirs: false,
      exts: [".json", ".jsonc"],
      match: [/(^|[/\\])modinfo\.jsonc?$/],
    })
  ) {
    const modInfo = parseJsonc(await Deno.readTextFile(entry.path)) as ModInfo[]
    for (const mod of modInfo) {
      if (mod.type !== "MOD_INFO" || mod.obsolete || blacklist.has(mod.id)) continue
      allModDependencies.set(mod.id, mod.dependencies ?? [])
    }
  }

  const modsToKeep: string[] = []
  const addMods = (mods: readonly string[]) => {
    if (mods.some((mod) => !allModDependencies.has(mod))) return false
    for (const mod of mods) {
      if (!modsToKeep.includes(mod)) modsToKeep.push(mod)
    }
    return true
  }

  for (const [mod, dependencies] of allModDependencies) {
    if (!modsToKeep.includes(mod) && addMods(dependencies)) modsToKeep.push(mod)
  }
  return modsToKeep
}

if (import.meta.main) {
  await new Command()
    .arguments("<blacklist:string>")
    .description("List available mods and dependencies, excluding blacklisted IDs.")
    .action(async (_, path) => {
      const blacklist = new Set((await Deno.readTextFile(path)).split(/\r?\n/))
      console.log((await getAllMods("data/mods", blacklist)).join(","))
    })
    .parse(Deno.args)
}
