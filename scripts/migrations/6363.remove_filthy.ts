#!/usr/bin/env -S deno run -RW --allow-env

/**
 * @module
 * [PR#6363](https://github.com/cataclysmbnteam/Cataclysm-BN/pull/6363)
 */

import { bgBrightYellow } from "@std/fmt/colors"
import { Command } from "@cliffy/command"
import { mapMany, recursivelyReadJSON, writeMany } from "../utils.ts"
import { asUndefined, looseObjectWithout } from "../valibot_utils.ts"
import * as v from "@valibot/valibot"

export const ArrayWithoutFilthy = v.pipe(
  v.array(v.string()),
  v.transform((xs) => xs.filter((x) => x !== "FILTHY")),
)

export const UseActionWithoutFilthy = v.union([
  v.array(v.unknown()),
  asUndefined(v.picklist(["WASH_SOFT_ITEMS", "WASH_HARD_ITEMS"])),
  looseObjectWithout({}, ["filthy_volume_threshold"]),
])

export const HarvestEntryWithoutFilthy = v.looseObject({
  type: v.literal("harvest"),
  entries: v.array(v.looseObject({
    flags: v.optional(ArrayWithoutFilthy),
  })),
})

export const ItemWithoutFilthy = looseObjectWithout({
  use_action: v.optional(UseActionWithoutFilthy),
  flags: v.optional(ArrayWithoutFilthy),
}, ["squeamish_penalty"])

if (import.meta.main) {
  const { args: [path] } = await new Command()
    .description(`removes ${bgBrightYellow("FILTHY")} flags from items`)
    .arguments("<path:string>")
    .parse(Deno.args)

  const paths = await recursivelyReadJSON(path)
  const mapped = mapMany(
    v.union([
      HarvestEntryWithoutFilthy,
      ItemWithoutFilthy,
    ]),
    paths,
  )
  await writeMany(mapped)
}
