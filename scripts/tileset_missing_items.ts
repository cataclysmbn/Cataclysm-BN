#!/usr/bin/env -S deno run --allow-read

/**
 * @module
 *
 * Reports which items do not have a direct sprite in a tileset, and which only
 * render through an item `looks_like` fallback.
 *
 * example usage:
 *   `deno run -R scripts/tileset_missing_items.ts --limit 50`
 *   `deno run -R scripts/tileset_missing_items.ts --looks-like --limit 50`
 *   `deno run -R scripts/tileset_missing_items.ts --tileset gfx/RetroDaysTileset --tileset gfx/BrownLikeBears --limit 50`
 */

import { Command } from "@cliffy/command"
import { walk } from "@std/fs"
import { isAbsolute, join, relative, resolve } from "@std/path"
import * as jsonMap from "@scarf/json-map"

const REPO_ROOT = resolve(import.meta.dirname!, "..")
const DEFAULT_TILESETS = [
  "gfx/MSX++UnDeadPeopleEdition",
  "data/json/external_tileset",
  "data/mods",
]
const DEFAULT_INPUTS = ["data/json"]
const SKIP_INPUT_JSON = [/(modinfo|default|replacements|mod_tileset)\.json/]

export const getJsonPathSkips = (source: "input" | "tileset"): RegExp[] =>
  source === "tileset" ? [] : Array.from(SKIP_INPUT_JSON)

const ITEM_TYPES = new Set([
  "AMMO",
  "ARMOR",
  "BATTERY",
  "BIONIC_ITEM",
  "BOOK",
  "COMESTIBLE",
  "CONTAINER",
  "ENGINE",
  "GENERIC",
  "GUN",
  "GUNMOD",
  "MAGAZINE",
  "PET_ARMOR",
  "SPECIES",
  "TOOL",
  "TOOLMOD",
  "TOOL_ARMOR",
  "WHEEL",
])

type DefinitionKind = "item" | "abstract"

interface ItemDefinition {
  id: string
  kind: DefinitionKind
  type: string
  path: string
  copyFrom?: string
  looksLike?: string
  flags: string[]
}

interface ResolvedItemDefinition extends ItemDefinition {
  resolvedLooksLike?: string
  isFake: boolean
}

interface TileLookup {
  tileId: string
  chain: string[]
}

const TILE_ID_PREFIXES = ["overlay_wielded_", "overlay_worn_"]
const TILE_ID_SUFFIX_PATTERNS = [
  /^(.+)_season_(spring|summer|autumn|winter)$/,
  /^(.+)_harvested$/,
  /^(.+)_on$/,
  /^(.+)_off$/,
  /^(.+)_open$/,
  /^(.+)_closed$/,
  /^(.+)_full$/,
  /^(.+)_empty$/,
]

const isRecord = (value: unknown): value is Record<string, unknown> =>
  typeof value === "object" && value !== null && !Array.isArray(value)

const mapToObject = (value: unknown): unknown =>
  value instanceof Map
    ? Object.fromEntries(Array.from(value, ([key, child]) => [key, mapToObject(child)]))
    : Array.isArray(value)
    ? value.map(mapToObject)
    : value

const toDisplayPath = (path: string): string =>
  path.startsWith(REPO_ROOT) ? relative(REPO_ROOT, path) : path

const resolveRepoPath = (path: string): string => isAbsolute(path) ? path : resolve(REPO_ROOT, path)

const parseProperties = (content: string): Record<string, string> => {
  const pairs: Record<string, string> = {}

  for (const line of content.split("\n")) {
    const trimmed = line.trim()
    if (!trimmed || trimmed.startsWith("#")) {
      continue
    }

    const separator = trimmed.indexOf(":")
    if (separator === -1) {
      continue
    }

    const key = trimmed.slice(0, separator).trim()
    const value = trimmed.slice(separator + 1).trim()
    if (key && value) {
      pairs[key] = value
    }
  }

  return pairs
}

const pathExists = async (path: string): Promise<boolean> => {
  try {
    await Deno.stat(path)
    return true
  } catch {
    return false
  }
}

const resolveTilesetConfigPath = async (tilesetPath: string): Promise<string> => {
  const resolvedPath = resolveRepoPath(tilesetPath)
  const stat = await Deno.stat(resolvedPath)
  if (stat.isFile) {
    return resolvedPath
  }

  const propertiesPath = join(resolvedPath, "tileset.txt")
  try {
    const properties = parseProperties(await Deno.readTextFile(propertiesPath))
    return join(resolvedPath, properties.JSON ?? "tile_config.json")
  } catch {
    return join(resolvedPath, "tile_config.json")
  }
}

const resolveTilesetSourcePaths = async (tilesetPath: string): Promise<string[]> => {
  const resolvedPath = resolveRepoPath(tilesetPath)
  const stat = await Deno.stat(resolvedPath)
  if (stat.isFile) {
    return [resolvedPath]
  }

  if (await pathExists(join(resolvedPath, "tileset.txt"))) {
    return [await resolveTilesetConfigPath(resolvedPath)]
  }

  const tileConfigPath = join(resolvedPath, "tile_config.json")
  if (await pathExists(tileConfigPath)) {
    return [tileConfigPath]
  }

  return await readJsonPaths([resolvedPath], getJsonPathSkips("tileset"))
}

const collectTileIdsFromEntries = (entries: unknown[], tileIds: Set<string>): void => {
  for (const entry of entries) {
    if (!isRecord(entry)) {
      continue
    }

    const { id, additional_tiles: additionalTiles } = entry
    if (typeof id === "string") {
      tileIds.add(id)
    } else if (Array.isArray(id)) {
      for (const candidate of id) {
        if (typeof candidate === "string") {
          tileIds.add(candidate)
        }
      }
    }

    if (Array.isArray(additionalTiles)) {
      collectTileIdsFromEntries(additionalTiles, tileIds)
    }
  }
}

const collectTileIdsFromConfig = (config: unknown, tileIds: Set<string>): void => {
  if (Array.isArray(config)) {
    for (const child of config) {
      collectTileIdsFromConfig(child, tileIds)
    }
    return
  }

  if (!isRecord(config)) {
    return
  }

  if (Array.isArray(config["tiles-new"])) {
    collectTileIdsFromConfig(config["tiles-new"], tileIds)
  }

  if (Array.isArray(config.tiles)) {
    collectTileIdsFromEntries(config.tiles, tileIds)
  }
}

export const collectTileIds = (config: unknown): Set<string> => {
  const tileIds = new Set<string>()

  collectTileIdsFromConfig(config, tileIds)
  return tileIds
}

export const collectTileIdsFromConfigs = (configs: readonly unknown[]): Set<string> => {
  const tileIds = new Set<string>()

  for (const config of configs) {
    for (const tileId of collectTileIds(config)) {
      tileIds.add(tileId)
    }
  }

  return tileIds
}

const collectTileIdAliases = (tileId: string, aliases: Set<string>): void => {
  if (aliases.has(tileId)) {
    return
  }
  aliases.add(tileId)

  for (const prefix of TILE_ID_PREFIXES) {
    if (tileId.startsWith(prefix)) {
      collectTileIdAliases(tileId.slice(prefix.length), aliases)
    }
  }

  for (const pattern of TILE_ID_SUFFIX_PATTERNS) {
    const match = tileId.match(pattern)
    if (match) {
      collectTileIdAliases(match[1], aliases)
    }
  }
}

export const getTileIdAliases = (tileId: string): Set<string> => {
  const aliases = new Set<string>()
  collectTileIdAliases(tileId, aliases)
  return aliases
}

export const buildVariantTileLookup = (tileIds: Iterable<string>): Map<string, string> => {
  const variantTileLookup = new Map<string, string>()

  for (const tileId of Array.from(tileIds).sort()) {
    for (const alias of getTileIdAliases(tileId)) {
      if (alias !== tileId && !variantTileLookup.has(alias)) {
        variantTileLookup.set(alias, tileId)
      }
    }
  }

  return variantTileLookup
}

const collectExplicitIgnoredItemIdsFromValue = (
  value: unknown,
  ignoredItemIds: Set<string>,
): void => {
  if (Array.isArray(value)) {
    for (const child of value) {
      collectExplicitIgnoredItemIdsFromValue(child, ignoredItemIds)
    }
    return
  }

  if (!isRecord(value)) {
    return
  }

  if (typeof value.fake_item === "string") {
    ignoredItemIds.add(value.fake_item)
  }

  if (value.type === "gun") {
    if (typeof value.gun_type === "string") {
      ignoredItemIds.add(value.gun_type)
    }
    if (typeof value.ammo_type === "string") {
      ignoredItemIds.add(value.ammo_type)
    }
  }

  for (const child of Object.values(value)) {
    collectExplicitIgnoredItemIdsFromValue(child, ignoredItemIds)
  }
}

export const collectExplicitIgnoredItemIds = (entries: readonly unknown[]): Set<string> => {
  const ignoredItemIds = new Set<string>()

  for (const entry of entries) {
    collectExplicitIgnoredItemIdsFromValue(entry, ignoredItemIds)
  }

  return ignoredItemIds
}

const parseJsonMaps = (content: string): unknown[] => {
  const parsed = jsonMap.parse(content)

  if (Array.isArray(parsed)) {
    return parsed
  }

  return [parsed]
}

export const readJsonPaths = async (
  inputs: readonly string[],
  skip: readonly RegExp[] = getJsonPathSkips("input"),
): Promise<string[]> => {
  const jsonPaths = new Set<string>()
  const skipPatterns = Array.from(skip)

  for (const input of inputs) {
    const resolvedInput = resolveRepoPath(input)
    const stat = await Deno.lstat(resolvedInput)

    if (stat.isFile) {
      jsonPaths.add(resolvedInput)
      continue
    }

    for await (
      const entry of walk(resolvedInput, {
        includeDirs: false,
        exts: [".json"],
        skip: skipPatterns,
      })
    ) {
      jsonPaths.add(entry.path)
    }
  }

  return Array.from(jsonPaths)
}

const toDefinition = (value: unknown, path: string): ItemDefinition | null => {
  if (!isRecord(value) || typeof value.type !== "string" || !ITEM_TYPES.has(value.type)) {
    return null
  }

  const id = typeof value.id === "string"
    ? value.id
    : typeof value.abstract === "string"
    ? value.abstract
    : undefined
  if (!id) {
    return null
  }

  return {
    id,
    kind: typeof value.id === "string" ? "item" : "abstract",
    type: value.type,
    path,
    copyFrom: typeof value["copy-from"] === "string" ? value["copy-from"] : undefined,
    looksLike: typeof value.looks_like === "string" ? value.looks_like : undefined,
    flags: Array.isArray(value.flags)
      ? value.flags.filter((flag): flag is string => typeof flag === "string")
      : typeof value.flags === "string"
      ? [value.flags]
      : [],
  }
}

const loadDefinitions = async (inputs: readonly string[]) => {
  const items = new Map<string, ItemDefinition>()
  const abstracts = new Map<string, ItemDefinition>()
  const ignoredItemIds = new Set<string>()

  for (const path of await readJsonPaths(inputs)) {
    const entries = parseJsonMaps(await Deno.readTextFile(path)).map(mapToObject)
    for (const ignoredItemId of collectExplicitIgnoredItemIds(entries)) {
      ignoredItemIds.add(ignoredItemId)
    }
    for (const rawEntry of entries) {
      const definition = toDefinition(rawEntry, path)
      if (!definition) {
        continue
      }

      if (definition.kind === "item") {
        items.set(definition.id, definition)
      } else {
        abstracts.set(definition.id, definition)
      }
    }
  }

  return { items, abstracts, ignoredItemIds }
}

export const resolveItemDefinitions = (
  items: Map<string, ItemDefinition>,
  abstracts: Map<string, ItemDefinition>,
): Map<string, ResolvedItemDefinition> => {
  const cache = new Map<string, ResolvedItemDefinition>()

  const findDefinition = (id: string): ItemDefinition | undefined =>
    items.get(id) ?? abstracts.get(id)

  const resolveDefinition = (
    definition: ItemDefinition,
    visiting: Set<string> = new Set(),
  ): ResolvedItemDefinition => {
    const cacheKey = `${definition.kind}:${definition.id}`
    const cached = cache.get(cacheKey)
    if (cached) {
      return cached
    }

    if (visiting.has(cacheKey)) {
      return {
        ...definition,
        resolvedLooksLike: definition.looksLike,
        isFake: definition.id === "fake_item" || definition.flags.includes("PSEUDO"),
      }
    }

    visiting.add(cacheKey)

    let resolvedLooksLike = definition.looksLike
    let isFake = definition.id === "fake_item" || definition.flags.includes("PSEUDO")
    if (!resolvedLooksLike && definition.copyFrom) {
      const parent = findDefinition(definition.copyFrom)
      if (parent?.kind === "item") {
        resolvedLooksLike = definition.copyFrom
        isFake = isFake || resolveDefinition(parent, visiting).isFake
      } else if (parent?.kind === "abstract") {
        const resolvedParent = resolveDefinition(parent, visiting)
        resolvedLooksLike = resolvedParent.resolvedLooksLike ?? definition.copyFrom
        isFake = isFake || resolvedParent.isFake
      }
    }

    visiting.delete(cacheKey)

    const resolved = { ...definition, resolvedLooksLike, isFake }
    cache.set(cacheKey, resolved)
    return resolved
  }

  return new Map(
    Array.from(items.values(), (definition) => [definition.id, resolveDefinition(definition)]),
  )
}

export const shouldIgnoreItemDefinition = (
  item: ResolvedItemDefinition,
  ignoredItemIds: ReadonlySet<string>,
): boolean => item.isFake || ignoredItemIds.has(item.id)

export const findTileByLooksLike = (
  itemId: string,
  resolvedItems: Map<string, ResolvedItemDefinition>,
  tileIds: Set<string>,
  variantTileLookup: ReadonlyMap<string, string>,
  depth: number = 10,
  visited: Set<string> = new Set(),
): TileLookup | null => {
  if (!itemId || depth <= 0) {
    return null
  }

  if (tileIds.has(itemId)) {
    return { tileId: itemId, chain: [itemId] }
  }

  const variantTileId = variantTileLookup.get(itemId)
  if (variantTileId) {
    return { tileId: variantTileId, chain: [itemId] }
  }

  if (visited.has(itemId)) {
    return null
  }
  visited.add(itemId)

  const looksLike = resolvedItems.get(itemId)?.resolvedLooksLike
  if (!looksLike) {
    return null
  }

  const lookup = findTileByLooksLike(
    looksLike,
    resolvedItems,
    tileIds,
    variantTileLookup,
    depth - 1,
    visited,
  )
  if (!lookup) {
    return null
  }

  return {
    tileId: lookup.tileId,
    chain: [itemId, ...lookup.chain],
  }
}

const printSection = (
  title: string,
  rows: string[],
  limit: number,
): void => {
  console.log(`\n${title} (${rows.length})`)
  if (rows.length === 0) {
    console.log("  (none)")
    return
  }

  const shownRows = limit > 0 ? rows.slice(0, limit) : rows
  for (const row of shownRows) {
    console.log(row)
  }

  if (limit > 0 && rows.length > limit) {
    console.log(`... ${rows.length - limit} more`)
  }
}

if (import.meta.main) {
  const parsed = await new Command()
    .description("Report items missing sprites in one or more tilesets")
    .option("--tileset <path:string>", "Tileset path(s) to inspect", {
      collect: true,
      default: DEFAULT_TILESETS,
    })
    .option("--input <path:string>", "Item JSON path(s) to inspect", {
      collect: true,
      default: DEFAULT_INPUTS,
    })
    .option("--looks-like", "Report items that are only covered by looks_like", {
      default: false,
    })
    .option("--limit <count:number>", "Max rows to print per section", { default: 0 })
    .parse(Deno.args)

  const tilesets = Array.from((parsed.options.tileset ?? DEFAULT_TILESETS) as string[])
  const input = Array.from((parsed.options.input ?? DEFAULT_INPUTS) as string[])
  const showLooksLike = Boolean(parsed.options.looksLike ?? false)
  const limit = Number(parsed.options.limit ?? 0)

  const tilesetSourcePaths = Array.from(
    new Set((await Promise.all(tilesets.map(resolveTilesetSourcePaths))).flat()),
  )
  const tileIds = collectTileIdsFromConfigs(
    await Promise.all(
      tilesetSourcePaths.map(async (tilesetSourcePath) => {
        return mapToObject(jsonMap.parse(await Deno.readTextFile(tilesetSourcePath)))
      }),
    ),
  )
  const variantTileLookup = buildVariantTileLookup(tileIds)
  const { items, abstracts, ignoredItemIds } = await loadDefinitions(input)
  const resolvedItems = resolveItemDefinitions(items, abstracts)
  const reportableItems = Array.from(resolvedItems.values())
    .filter((item) => !shouldIgnoreItemDefinition(item, ignoredItemIds))
    .sort((left, right) => left.id.localeCompare(right.id))

  const direct: string[] = []
  const variantOnly: string[] = []
  const fallbackOnly: string[] = []
  const missing: string[] = []

  for (const item of reportableItems) {
    if (tileIds.has(item.id)) {
      direct.push(item.id)
      continue
    }

    const variantTileId = variantTileLookup.get(item.id)
    if (variantTileId) {
      variantOnly.push(`${item.id}\t${variantTileId}\t${toDisplayPath(item.path)}`)
      continue
    }

    const lookup = findTileByLooksLike(item.id, resolvedItems, tileIds, variantTileLookup)
    if (lookup) {
      fallbackOnly.push(`${lookup.chain.join(" -> ")}\t${toDisplayPath(item.path)}`)
      continue
    }

    missing.push(`${item.id}\t${toDisplayPath(item.path)}`)
  }

  console.log(`Tilesets: ${tilesets.map(toDisplayPath).join(", ")}`)
  console.log(`Inputs: ${input.map(toDisplayPath).join(", ")}`)
  console.log(`Checked items: ${reportableItems.length}`)
  console.log(`Direct sprites: ${direct.length}`)
  console.log(`Variant sprites only: ${variantOnly.length}`)
  if (showLooksLike) {
    console.log(`Looks_like fallback only: ${fallbackOnly.length}`)
  }
  console.log(`Missing entirely: ${missing.length}`)

  printSection("Missing ITEM id (but variant tile exists)", variantOnly, limit)
  printSection("Missing ITEM", missing, limit)
  if (showLooksLike) {
    printSection("Missing ITEM (but looks_like tile exists)", fallbackOnly, limit)
  }
}
