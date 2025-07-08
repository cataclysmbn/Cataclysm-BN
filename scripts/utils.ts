import * as v from "@valibot/valibot"

import { walk, WalkEntry } from "@std/fs"
import type { BaseIssue, BaseSchema, InferOutput } from "@valibot/valibot"
import * as jsonMap from "@scarf/json-map"
import { deepSort } from "./deep_sort.ts"
import { MuxAsyncIterator } from "jsr:@std/async/unstable-mux-async-iterator"
import { createWalkEntry } from "https://deno.land/std@0.208.0/fs/_util.ts"
import { mapKeys } from "@std/collections"

/**
 * @module
 *
 * migration utilities to query relevant JSON entry, map it, then save it
 *
 * example usage
 * ```ts
 * const FoodMigrateDoubleCalories = v.looseObject({
 *   calories: v.pipe(v.number(), v.transform((x) => x * 2)),
 * })
 *
 * const paths = await recursivelyReadJSON("/some/path1", "/some/path2")
 * const mapped = mapMany(FoodMigrateDoubleCalories, paths)
 * await writeMany(mapped)
 * ```
 */

type AliasResult<
  T extends object,
  A extends Readonly<Partial<Record<keyof T, string>>>,
> = { [K in keyof T as K extends keyof A ? (A[K] extends string ? A[K] : K) : K]: T[K] }
// deno-lint-ignore ban-types
type Simplify<T> = { [K in keyof T]: T[K] } & {}

export const alias = <T extends object, A extends Readonly<Partial<Record<keyof T, string>>>>(
  obj: Readonly<T>,
  alias: A,
): Simplify<AliasResult<T, A>> =>
  // deno-lint-ignore no-explicit-any
  mapKeys(obj, (key) => (alias as any)[key] ?? key) as AliasResult<T, A>

/**
 * removes `null`, `undefined` or empty array values from an object
 * needed because {@link jsonMap.stringify} doesn't remove `undefined` values
 */
export const omitEmpty = <T>(obj: Record<string, T>): Record<string, T> =>
  Object.fromEntries(
    Object.entries(obj).filter(([_, value]) =>
      !(value == null || (Array.isArray(value) && value.length === 0))
    ),
  )

/**
 * create a parser from given schema that parses an array of objects
 */
export const parseMany = <const TSchema extends BaseSchema<unknown, unknown, BaseIssue<unknown>>>(
  schema: TSchema,
) => {
  const parser = v.safeParser(schema)
  return (xs: JSONFileEntry[]): InferOutput<TSchema>[] =>
    xs
      .flatMap((x) => x.data)
      .map((x) => parser(map_to_object(x)))
      .filter((x) => x.success)
      .map((x) => x.output)
}

const excludedType = new Set([
  "speech",
  "sound_effect",
  "mapgen",
  "palette",
  "faction",
  "talk_topic",
  "mod_tileset",
  "MOD_INFO",
])

// deno-lint-ignore no-explicit-any
const convert_value = (value: any): any =>
  value instanceof Map
    ? map_to_object(value)
    : Array.isArray(value)
    ? value.map(convert_value)
    : value

// deno-lint-ignore no-explicit-any
function map_to_object(map: Map<any, any>): object {
  const out = Object.create(null)
  map.forEach((value, key) => {
    // Convert each value using the recursive helper function
    out[key] = convert_value(value)
  })
  return out
}

export const mapMany = <const TSchema extends BaseSchema<unknown, unknown, BaseIssue<unknown>>>(
  schema: TSchema,
  xs: JSONFileEntry[],
) => {
  const parser = v.safeParser(schema)
  return xs.map(({ data, ...rest }) => {
    const result = data.map((map) => {
      try {
        const type = map.get("type")
        if (type && typeof type === "string" && excludedType.has(type)) return map

        const obj = map_to_object(map)
        const parsed = parser(obj)
        if (!parsed.success) return map

        return deepSort(map, parsed.output as object)
      } catch (e) {
        console.error("map:", map_to_object(map), "error:", e)
        throw e
      }
    })
    return { ...rest, data: result }
  })
}

export const writeMany = (xs: (WalkEntry & { data: unknown[] })[]) =>
  Promise.all(
    xs.map((x) => Deno.writeTextFile(x.path, jsonMap.prettyPrint(x.data))),
  )

export interface JSONFileEntry extends WalkEntry {
  data: Map<string, unknown>[]
}

async function* walkFileOrDirectory(path: string): AsyncIterableIterator<WalkEntry> {
  const isFile = await Deno.lstat(path).then((x) => x.isFile)

  if (isFile) {
    yield await createWalkEntry(path)
  } else {
    yield* walk(path, {
      exts: [".json"],
      skip: [/(modinfo|default|replacements|mod_tileset)\.json/],
    })
  }
}

/**
 * @param paths paths to recursively read JSON files from
 * @returns an array of {@link WalkEntry} with unverified JSON object array
 */
export const recursivelyReadJSON = async (...paths: string[]): Promise<JSONFileEntry[]> => {
  const mux = new MuxAsyncIterator(...paths.map(walkFileOrDirectory))
  const jsons = await Array.fromAsync(mux)

  const res = jsons.map(async (entry) => {
    try {
      return ({
        ...entry,
        data: jsonMap.parse(await Deno.readTextFile(entry.path)) as Map<string, unknown>[],
      })
    } catch (e) {
      console.error(entry.path, e)
      throw e
    }
  })

  return (await Promise.all(res))
}
