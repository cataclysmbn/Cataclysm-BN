#!/usr/bin/env -S deno run -RW

/**
 * mod file organizer script
 *
 * 1. reads all JSON files under given path recursively
 * 2. saves them into separate files by type to given output directory
 *
 * e.g generates files like `generic.json`, `magazine.json`, etc.
 *
 * @module
 */

import { parseMany, recursivelyReadJSON } from "./utils.ts"
import { Command } from "@cliffy/command"
import { join } from "@std/path"
import * as jsonMap from "@scarf/json-map"
import * as v from "@valibot/valibot"

const Item = v.looseObject({ type: v.string() })

if (import.meta.main) {
  const { options: { input, output } } = await new Command()
    .description("Group JSON files by type into separate files")
    .option("--input <path>", "Path to input directory containing JSON files", { required: true })
    .option("--output <path>", "Path to output directory for grouped JSON files", {
      required: true,
    })
    .parse(Deno.args)

  const jsons = await recursivelyReadJSON(input)
  const items = parseMany(Item)(jsons)
  const grouped = Object.groupBy(items, (item) => item.type)

  await Deno.mkdir(output, { recursive: true })
  await Promise.all(
    Object.entries(grouped)
      .map(([group, items]) =>
        Deno.writeTextFile(
          join(output!, `${group.toLocaleLowerCase()}.json`),
          jsonMap.prettyPrint(items),
        )
      ),
  )
}
