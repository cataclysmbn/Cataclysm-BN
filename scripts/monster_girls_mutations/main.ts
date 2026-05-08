#!/usr/bin/env -S deno run -RW --allow-env

import { Command } from "@cliffy/command"
import { parseMany, recursivelyReadJSON } from "../utils.ts"
import * as v from "@valibot/valibot"
import { join } from "@std/path"
import { deepMerge } from "@std/collections"

/**
 * @module
 * example usage: `scripts/monster_girls_mutations/main.ts && tools/format/json_formatter.cgi data/mods/Monster_Girls/vanilla_mutations.json`
 */

const THRESHOLD_CONVERSION: Record<string, string> = {
  "THRESH_FELINE": "THRESH_NEKO",
  "THRESH_BEAST": "THRESH_KITSUNE",
  "THRESH_LUPINE": "THRESH_DOGGIRL",
  "THRESH_PLANT": "THRESH_DRYAD",
  "THRESH_BIRD": "THRESH_HARPY",
  "THRESH_URSINE": "THRESH_BEARGIRL",
  "THRESH_SPIDER": "THRESH_SPIDERGIRL",
  "THRESH_SLIME": "THRESH_SLIMEGIRL",
  "THRESH_MOUSE": "THRESH_MOUSEGIRL",
  "THRESH_CATTLE": "THRESH_COWGIRL",
  "THRESH_RABBIT": "THRESH_BUNNYGIRL",
  "THRESH_ELFA": "THRESH_ELF",
  "THRESH_SERPENT": "THRESH_LAMIA",
}

const CUSTOM_THRESHOLDS: Record<string, string[]> = {
  FELINE_FLEXIBILITY: ["THRESH_KITSUNE"],
}

const Category = v.array(v.string())

const Mutation = v.pipe(
  v.looseObject({
    type: v.literal("mutation"),
    id: v.string(),
    category: Category,
    threshreq: v.pipe(
      v.optional(Category),
      v.transform((xs) => xs?.flatMap((t) => THRESHOLD_CONVERSION[t] ?? [])),
    ),
  }),
  v.transform((m) => {
    const custom = CUSTOM_THRESHOLDS[m.id] ?? []
    return custom.length === 0 ? m : deepMerge<typeof m>(m, { threshreq: custom })
  }),
)

const Vanilla = v.object({
  type: v.literal("mutation"),
  id: v.string(),
  extend: v.object({ category: Category }),
})

const dataPath = join(import.meta.dirname!, "../../data")
const vanillaPath = join(dataPath, "mods/Monster_Girls/vanilla_mutations.json")
const mutationPath = join(dataPath, "json/mutations/mutations.json")

if (import.meta.main) {
  const { options: { extend, input, output } } = await new Command()
    .description("Converts mutations by creating copy-from entries that delete categories")
    .option("--input <...path:string>", "Path to the input file", { default: [mutationPath] })
    .option("--output <path:string>", "Path to the output file", { default: vanillaPath })
    .option("--extend <path:string>", "original -> animal girl extends", { default: vanillaPath })
    .help({ hints: false })
    .parse(Deno.args)

  const validMutations = parseMany(Mutation)(await recursivelyReadJSON(...input))
  const vanilla = parseMany(Vanilla)(await recursivelyReadJSON(extend))

  const mapping = new Map(vanilla.map((v) => [v.id, new Set(v.extend.category)]))

  const mutations = validMutations.map(({ type, id, category, ...m }) => {
    const res = mapping.get(id)
    const threshreq = m.threshreq?.filter((t) => res?.has(t.replace("THRESH_", "")))
    const extended = res ? { extend: { category: Array.from(res), threshreq } } : { valid: false }

    return ({ type, id, "copy-from": id, delete: { category }, ...extended })
  })

  await Deno.writeTextFile(output, JSON.stringify(mutations, null, 2))
}
