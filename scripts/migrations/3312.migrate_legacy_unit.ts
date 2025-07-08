import { cliOptions } from "$catjazz/utils/cli.ts"
import {
  fromLegacyCurrency,
  fromLegacyEnergy,
  fromLegacyVolume,
  fromLegacyWeight,
} from "$catjazz/units/mod.ts"
import { z } from "$catjazz/deps/zod.ts"
import { Command } from "@cliffy/command"
import { timeit } from "$catjazz/utils/timeit.ts"
import { applyRecursively } from "$catjazz/utils/transform.ts"
import { fmtJsonRecursively } from "$catjazz/utils/json_fmt.ts"
import { CataEntry, Entry, parseCataJson, readJSONsRec } from "$catjazz/utils/parse.ts"
import { match, P } from "$catjazz/deps/ts_pattern.ts"
import { id } from "$catjazz/utils/id.ts"
import { deepMerge } from "@std/collections"

/**
 * [PR#3312](https://github.com/cataclysmbnteam/Cataclysm-BN/pull/3312)
 */

const desc = "Migrates Legacy units into new literal format."

const int = z.number().int()

export type Currency = `${number} ${"cent" | "USD" | "kUSD"}`

export const migrate = <const T>(fromLegacy: (x: number) => T) =>
  z.union([int.transform(fromLegacy), z.string()]).optional()

const unpack = (xs: string[] | Entry[]) =>
  match(xs)
    .with(P.array(P.string), id)
    .otherwise((xs) => xs.map(({ path }) => path))

const migrateWeight = migrate(fromLegacyWeight)
const migrateVolume = migrate(fromLegacyVolume)
const migrateCurrency = migrate(fromLegacyCurrency)
const migrateEnergy = migrate(fromLegacyEnergy)

export const base = z
  .object({
    // GUN
    barrel_length: migrateVolume,

    armor_data: z.object({ storage: migrateVolume }).passthrough().optional(),

    storage: migrateVolume,

    max_pet_vol: migrateVolume,
    min_pet_vol: migrateVolume,

    contains: migrateVolume,

    volume: migrateVolume,
    integral_volume: migrateVolume,
    magazine_well: migrateVolume,

    max_volume: migrateVolume,
    min_volume: migrateVolume,

    use_action: z.string().or(
      z.object({
        max_volume: migrateVolume,
        min_volume: migrateVolume,

        max_weight: migrateWeight,
      }).passthrough(),
    ).optional(),

    size: migrateVolume,
    // TODO: burn_data[].volume_per_turn

    weight: migrateWeight,
    integral_weight: migrateWeight,
    weight_capacity_bonus: migrateWeight,

    trigger_weight: migrateWeight,

    workbench: z.object({
      volume: migrateVolume,
      mass: migrateWeight,
    }).passthrough().optional(),

    price: migrateCurrency,
    price_postapoc: migrateCurrency,
  })
  .passthrough()

export const vehiclePart = z.object({
  type: z.literal("vehicle_part"),
  folded_volume: migrateVolume,
  size: migrateVolume,
})
  .passthrough()

export const bionic = z.object({
  type: z.literal("bionic"),

  act_cost: migrateEnergy,
  deact_cost: migrateEnergy,
  react_cost: migrateEnergy,
  trigger_cost: migrateEnergy,
  capacity: migrateEnergy,
})

const schemas = [base, vehiclePart, bionic]

const attempt = (schema: z.ZodTypeAny) => (x: CataEntry): CataEntry =>
  match(schema.safeParse(x))
    .with(
      { success: true, data: P.select() },
      (parsed) => deepMerge(x, parsed, { arrays: "replace" }) as unknown as CataEntry,
    )
    .otherwise(() => x)

export const schemasTransformer = (schemas: z.ZodTypeAny[]) => {
  const matchers = schemas.map((schema) => attempt(schema))

  return (entries: CataEntry[]) =>
    entries
      .map((x) => matchers.reduce((acc, fn) => fn(acc), x))
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
        return ignore(entries) ? text : JSON.stringify(transformer(entries), null, 2)
      }

      const recursiveTransformer = applyRecursively(mapgenIgnoringTransformer)

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
