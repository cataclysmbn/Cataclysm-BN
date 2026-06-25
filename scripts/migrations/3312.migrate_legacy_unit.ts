#!/usr/bin/env -S deno run --allow-read --allow-write --allow-run --allow-env --allow-net
/**
 * @module
 *
 * Migrates legacy integer units in JSON data to explicit unit strings.
 *
 * Handles unit fields from PR#3312 and relative overrides that use the same
 * loader paths as their top-level counterparts.
 */

import { cliOptions } from "$catjazz/utils/cli.ts"
import {
  fromLegacyCurrency,
  fromLegacyEnergy,
  fromLegacyVolume,
  fromLegacyWeight,
} from "$catjazz/units/mod.ts"
import { Command } from "@cliffy/command"
import { deepMerge } from "@std/collections"
import * as v from "@valibot/valibot"
import { timeit } from "$catjazz/utils/timeit.ts"
import { fmtJsonRecursively } from "$catjazz/utils/json_fmt.ts"
import { parseCataJson, readJSONsRec } from "$catjazz/utils/parse.ts"
import type { CataEntry, Entry } from "$catjazz/utils/parse.ts"
import { match, P } from "$catjazz/deps/ts_pattern.ts"
import { id } from "$catjazz/utils/id.ts"

/**
 * [PR#3312](https://github.com/cataclysmbn/Cataclysm-BN/pull/3312)
 */

const desc = "Migrates Legacy units into new literal format."

export type Currency = `${number} ${"cent" | "USD" | "kUSD"}`

export const migrate = <const T>(fromLegacy: (x: number) => T) =>
  v.optional(v.union([v.pipe(v.number(), v.integer(), v.transform(fromLegacy)), v.string()]))

const isRecord = (input: unknown): input is Record<string, unknown> =>
  typeof input === "object" && input !== null && !Array.isArray(input)

const object = <const T extends v.ObjectEntries>(entries: T) =>
  v.pipe(v.custom<Record<string, unknown>>(isRecord), v.looseObject(entries))

const unpack = (xs: string[] | Entry[]) =>
  match(xs)
    .with(P.array(P.string), id)
    .otherwise((xs) => xs.map(({ path }) => path))

const applyChangedRecursively = (fn: (text: string) => string) => async (entries: Entry[]) => {
  await Promise.all(
    entries.map(async ({ path, text }) => {
      const transformed = fn(text)
      if (transformed === text) {
        return
      }
      await Deno.writeTextFile(path, transformed)
    }),
  )
}

const migrateWeight = migrate(fromLegacyWeight)
const migrateVolume = migrate(fromLegacyVolume)
const migrateCurrency = migrate(fromLegacyCurrency)
const migrateEnergy = migrate(fromLegacyEnergy)

const volumeFields = {
  barrel_length: migrateVolume,
  barrel_volume: migrateVolume,
  storage: migrateVolume,
  max_pet_vol: migrateVolume,
  min_pet_vol: migrateVolume,
  contains: migrateVolume,
  volume: migrateVolume,
  integral_volume: migrateVolume,
  magazine_well: migrateVolume,
  max_volume: migrateVolume,
  min_volume: migrateVolume,
  size: migrateVolume,
  // TODO: burn_data[].volume_per_turn
}

const weightFields = {
  weight: migrateWeight,
  integral_weight: migrateWeight,
  weight_capacity_bonus: migrateWeight,
  max_weight: migrateWeight,
  trigger_weight: migrateWeight,
}

const currencyFields = {
  price: migrateCurrency,
  price_postapoc: migrateCurrency,
}

const unitFields = {
  ...volumeFields,
  ...weightFields,
  ...currencyFields,
}

export const base = object({
  ...unitFields,

  relative: v.optional(object(unitFields)),

  armor_data: v.optional(object({ storage: migrateVolume })),

  use_action: v.optional(
    v.union([
      v.string(),
      object({
        max_volume: migrateVolume,
        min_volume: migrateVolume,
        max_weight: migrateWeight,
      }),
      v.unknown(),
    ]),
  ),

  workbench: v.optional(object({
    volume: migrateVolume,
    mass: migrateWeight,
  })),
})

export const vehiclePart = object({
  type: v.literal("vehicle_part"),
  folded_volume: migrateVolume,
  size: migrateVolume,
  relative: v.optional(object(volumeFields)),
})

export const bionic = object({
  type: v.literal("bionic"),

  act_cost: migrateEnergy,
  deact_cost: migrateEnergy,
  react_cost: migrateEnergy,
  trigger_cost: migrateEnergy,
  capacity: migrateEnergy,
})

const schemas = [base, vehiclePart, bionic]

const attempt = (schema: v.GenericSchema) => (x: CataEntry): CataEntry =>
  match(v.safeParse(schema, x))
    .with(
      { success: true, output: P.select() },
      (parsed) =>
        deepMerge(x, parsed as Record<PropertyKey, unknown>, {
          arrays: "replace",
        }) as unknown as CataEntry,
    )
    .otherwise(() => x)

export const schemasTransformer = (schemas: v.GenericSchema[]) => {
  const matchers = schemas.map((schema) => attempt(schema))

  return (entries: CataEntry[]) => entries.map((x) => matchers.reduce((acc, fn) => fn(acc), x))
}
const main = () =>
  new Command()
    .option(...cliOptions.paths)
    .option(...cliOptions.quiet)
    .option(...cliOptions.format)
    .description(desc)
    .action(async ({ paths, format, quiet = false }) => {
      const timeIt = timeit(quiet)

      const transformer = schemasTransformer(schemas)
      const ignore = (entries: CataEntry[]) =>
        entries.find(({ type }) =>
          ["speech", "sound_effect", "mapgen", "palette", "faction", "mod_tileset"].includes(type)
        )

      const mapgenIgnoringTransformer = (text: string) => {
        const entries = parseCataJson(text)
        if (ignore(entries)) {
          return text
        }

        const transformed = transformer(entries)
        return JSON.stringify(transformed) === JSON.stringify(entries)
          ? text
          : JSON.stringify(transformed, null, 2)
      }

      const recursiveTransformer = applyChangedRecursively(mapgenIgnoringTransformer)

      const entries = await timeIt({ name: "reading JSON", val: readJSONsRec(paths) })

      await timeIt({ name: "transform", val: recursiveTransformer(entries) })

      if (!format) return
      await timeIt({
        name: "formatting",
        val: fmtJsonRecursively({ formatterPath: format, quiet: true })(unpack(entries)),
      })
    })

if (import.meta.main) {
  await main().parse(Deno.args)
}
