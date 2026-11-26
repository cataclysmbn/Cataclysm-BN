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
import { mapEntries } from "$catjazz/deps/std/collection.ts"

/**
 * [PR#3312](https://github.com/cataclysmbn/Cataclysm-BN/pull/3312)
 */

const desc = "Migrates Legacy units into new literal format."

export type Currency = `${number} ${"cent" | "USD" | "kUSD"}`
export type Sound = `${number} dB`

// deno-fmt-ignore
const distVolLoss = [0, 1500, 602, 352, 250, 194, 158, 134, 116, 102, 92, 83, 76, 70, 64, 60, 56, 53, 50, 47, 45, 42, 40, 39, 37, 35, 34, 33, 32, 30, 29, 28, 28, 27, 26, 25, 24, 24, 23, 23, 22, 21, 21, 20, 20, 20, 19, 19, 18, 18, 18, 17, 17, 17, 16, 16, 16, 15, 15, 15, 15, 14, 14, 14, 14, 13, 13, 13, 13, 13, 12, 12, 12, 12, 12, 12, 12, 11, 11, 11, 11, 11, 11, 11, 10, 10, 10, 10, 10, 10, 10, 10, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 7, 7, 7, 7]

const minimumDistanceForSoundPropagation = 1
const maximumDistanceForSoundPropagation = 120
const maximumVolumeAtmosphere = 19100
const soundMinimumVolumeForPropagation = 2000

const mdBsplToDBspl = (mdB: number) => Math.trunc(mdB * 0.01)

const getDistanceForVolumeLoss = (tileDistance: number) =>
  tileDistance < minimumDistanceForSoundPropagation
    ? minimumDistanceForSoundPropagation
    : tileDistance > maximumDistanceForSoundPropagation
    ? maximumDistanceForSoundPropagation
    : tileDistance + 1

export const fromLegacySound = (legacyDistance: number): Sound => {
  if (legacyDistance <= minimumDistanceForSoundPropagation) {
    return `${mdBsplToDBspl(soundMinimumVolumeForPropagation)} dB`
  }

  const totalDistance = legacyDistance + Math.trunc(legacyDistance / 12)
  let approxVolume = soundMinimumVolumeForPropagation
  let checkDistance = minimumDistanceForSoundPropagation
  let approxDistance = minimumDistanceForSoundPropagation

  while (approxDistance < totalDistance) {
    approxDistance++
    checkDistance = getDistanceForVolumeLoss(checkDistance)
    approxVolume += distVolLoss[checkDistance]
  }

  return `${mdBsplToDBspl(Math.min(maximumVolumeAtmosphere, approxVolume))} dB`
}

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
const migrateSound = migrate(fromLegacySound)

const soundFields = {
  reload_noise_volume: migrateSound,
  reload_noise_volume_dB: migrateSound,
  sound_fail_vol: migrateSound,
  sound_vol: migrateSound,
  sound_volume: migrateSound,
  targeting_volume: migrateSound,
  targeting_volume_dB: migrateSound,
}

const soundFieldNames = new Set(Object.keys(soundFields))

const migrateSoundFieldsRecursively = <const T>(value: T): T => {
  if (Array.isArray(value)) {
    return value.map(migrateSoundFieldsRecursively) as T
  }
  if (!isRecord(value)) {
    return value
  }

  return mapEntries(
    value,
    ([key, entry]) => [
      key,
      soundFieldNames.has(key) && Number.isInteger(entry)
        ? fromLegacySound(entry as number)
        : migrateSoundFieldsRecursively(entry),
    ],
  ) as T
}

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

  return (entries: CataEntry[]) =>
    entries.map((x) => migrateSoundFieldsRecursively(matchers.reduce((acc, fn) => fn(acc), x)))
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
