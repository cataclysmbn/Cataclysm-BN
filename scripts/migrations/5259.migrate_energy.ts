#!/usr/bin/env -S deno run -RW --allow-env

/**
 * @module
 * [PR#5259](https://github.com/cataclysmbnteam/Cataclysm-BN/pull/5259)
 */

import { bgBrightYellow } from "@std/fmt/colors"
import { Command } from "@cliffy/command"
import { alias, mapMany, omitEmpty, recursivelyReadJSON, writeMany } from "../utils.ts"
import * as v from "@valibot/valibot"
import { omit, partition } from "@std/collections"

const fmtKJ = (x: number) => `${x} kJ`
const Nat = v.pipe(v.number(), v.integer())
const NatToKJ = v.pipe(Nat, v.transform(fmtKJ))

export const ItemTransformMigrateEnergy = v.pipe(
  v.looseObject({
    type: v.literal("transform"),
    need_charges: v.optional(NatToKJ),
    transform_charges: v.optional(NatToKJ),
    target_charges: v.optional(NatToKJ),
    rand_target_charges: v.optional(v.tuple([NatToKJ, NatToKJ])),
  }),
  v.transform((x) =>
    alias(x, {
      need_charges: "need_power",
      transform_charges: "transform_power",
      target_charges: "target_power",
      rand_target_charges: "rand_target_power",
    })
  ),
)

export const GunsMigrateEnergy = v.pipe(
  v.looseObject({ ups_charges: NatToKJ }),
  v.transform((x) => alias(x, { ups_charges: "power_draw" })),
)

export const ItemMigrateEnergy = v.pipe(
  v.looseObject({
    type: v.pipe(v.string(), v.transform((type) => type === "MAGAZINE" ? "BATTERY" : type)),
    ammo: v.optional(v.literal("battery")),
    ammo_type: v.optional(v.literal("battery")),
    charges_per_use: v.optional(NatToKJ),
    initial_charges: v.optional(NatToKJ),
    count: v.optional(NatToKJ),
    capacity: v.optional(NatToKJ),
    max_charges: v.optional(NatToKJ),
    power_draw: v.optional(v.pipe(Nat, v.transform((x) => fmtKJ(x / 1000)))),
    magazines: v.optional(
      v.pipe(
        v.array(v.tuple([v.string(), v.array(v.string())])),
        v.transform((entries) => partition(entries, ([key]) => key === "battery")),
      ),
    ),
    use_action: v.optional(v.union([v.string(), ItemTransformMigrateEnergy])),
    magazine_well: v.optional(v.string()),
  }),
  v.check((x) => (x.ammo === "battery" || x.ammo_type === "battery"), "missing ammo or ammo_type"),
  v.transform((x) => {
    const omitted = omit(x, ["ammo", "ammo_type"])
    const aliased = alias(omitted, {
      magazine_well: "battery_well",
      charges_per_use: "power_draw",
      initial_charges: "initial_power",
      count: "initial_power",
      capacity: "max_power",
      max_charges: "max_power",
    })
    const magazines = x.magazines
    if (!magazines) return aliased
    const [batteries, others] = magazines
    return omitEmpty({
      ...aliased,
      magazines: others,
      batteries: batteries.flatMap(([_, values]) => values),
    })
  }),
)

if (import.meta.main) {
  const { args } = await new Command()
    .description(`removes ${bgBrightYellow("FILTHY")} flags from items`)
    .arguments("<path...:string>")
    .parse(Deno.args)

  const paths = await recursivelyReadJSON(...args)
  const mapped = mapMany(
    v.union([
      GunsMigrateEnergy,
      ItemMigrateEnergy,
    ]),
    paths,
  )
  writeMany(mapped)
}
