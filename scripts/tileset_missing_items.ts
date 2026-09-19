#!/usr/bin/env -S deno run --allow-read

/**
 * @module
 *
 * Reports which items and monsters do not have a direct sprite in a tileset,
 * and which only render through a `looks_like` fallback.
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
import { partition } from "jsr:@std/collections@^1.1.3"
import * as v from "jsr:@valibot/valibot@^1.1.0"

const REPO_ROOT = resolve(import.meta.dirname!, "..")
const DEFAULT_TILESETS = [
  "gfx/MSX++UnDeadPeopleEdition",
  "data/json/external_tileset",
  "data/mods",
]
const DEFAULT_INPUTS = ["data/json"]
const SKIP_INPUT_JSON = [/(modinfo|default|replacements|mod_tileset)\.json/]
const REPORTABLE_TYPES = [
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
  "MONSTER",
  "PET_ARMOR",
  "TOOL",
  "TOOLMOD",
  "TOOL_ARMOR",
  "WHEEL",
] as const
const OUTPUT_FORMATS = ["text", "json", "md"] as const

export const getJsonPathSkips = (source: "input" | "tileset"): RegExp[] =>
  source === "tileset" ? [] : Array.from(SKIP_INPUT_JSON)

type DefinitionKind = "item" | "abstract"
type OutputFormat = (typeof OUTPUT_FORMATS)[number]

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

interface VariantOnlyEntry {
  id: string
  tileId: string
  path: string
}

interface MissingEntry {
  id: string
  path: string
}

interface LooksLikeEntry {
  id: string
  tileId: string
  chain: string[]
  path: string
}

interface ReportSection {
  title: string
  rows: string[]
}

interface RenderedReportSection extends ReportSection {
  shownRows: string[]
  hiddenCount: number
}

interface MissingTilesetReport {
  tilesets: string[]
  inputs: string[]
  checkedIds: number
  directSprites: number
  variantSpritesOnly: number
  looksLikeFallbackOnly: number
  missingEntirely: number
  variantOnly: VariantOnlyEntry[]
  missing: MissingEntry[]
  looksLikeOnly?: LooksLikeEntry[]
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
const OutputFormatSchema = v.picklist(OUTPUT_FORMATS)
const TileEntrySchema = v.looseObject({
  id: v.optional(v.union([v.string(), v.array(v.unknown())])),
  additional_tiles: v.optional(v.array(v.unknown())),
})
const TileConfigSchema = v.looseObject({
  "tiles-new": v.optional(v.array(v.unknown())),
  tiles: v.optional(v.array(v.unknown())),
})
const DefinitionSchema = v.looseObject({
  type: v.picklist(REPORTABLE_TYPES),
  id: v.optional(v.string()),
  abstract: v.optional(v.string()),
  "copy-from": v.optional(v.string()),
  looks_like: v.optional(v.string()),
  flags: v.optional(v.union([v.string(), v.array(v.unknown())])),
})
const IgnoredItemValueSchema = v.looseObject({
  fake_item: v.optional(v.string()),
  type: v.optional(v.string()),
  gun_type: v.optional(v.string()),
  ammo_type: v.optional(v.string()),
})

const isRecord = (value: unknown): value is Record<string, unknown> =>
  typeof value === "object" && value !== null && !Array.isArray(value)

const isDefined = <T>(value: T | undefined): value is T => value !== undefined

const asStringArray = (value: unknown): string[] =>
  Array.isArray(value)
    ? value.filter((entry): entry is string => typeof entry === "string")
    : typeof value === "string"
    ? [value]
    : []

const mapToObject = (value: unknown): unknown =>
  value instanceof Map
    ? Object.fromEntries(Array.from(value, ([key, child]) => [key, mapToObject(child)]))
    : Array.isArray(value)
    ? value.map(mapToObject)
    : value

const toDisplayPath = (path: string): string =>
  path.startsWith(REPO_ROOT) ? relative(REPO_ROOT, path) : path

const resolveRepoPath = (path: string): string => isAbsolute(path) ? path : resolve(REPO_ROOT, path)

const parseProperties = (content: string): Record<string, string> =>
  Object.fromEntries(
    content.split("\n").flatMap((line) => {
      const trimmed = line.trim()
      if (!trimmed || trimmed.startsWith("#")) {
        return []
      }

      const separator = trimmed.indexOf(":")
      if (separator === -1) {
        return []
      }

      const key = trimmed.slice(0, separator).trim()
      const value = trimmed.slice(separator + 1).trim()
      return key && value ? [[key, value] as const] : []
    }),
  )

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

const tileIdsFromEntry = (entry: unknown): string[] => {
  const parsedEntry = v.safeParse(TileEntrySchema, entry)
  if (!parsedEntry.success) {
    return []
  }

  const { id, additional_tiles: additionalTiles } = parsedEntry.output
  return [
    ...asStringArray(id),
    ...(additionalTiles?.flatMap(tileIdsFromEntry) ?? []),
  ]
}

const tileIdsFromConfig = (config: unknown): string[] => {
  if (Array.isArray(config)) {
    return config.flatMap(tileIdsFromConfig)
  }

  const parsedConfig = v.safeParse(TileConfigSchema, config)
  if (!parsedConfig.success) {
    return []
  }

  return [
    ...(parsedConfig.output["tiles-new"]
      ? tileIdsFromConfig(parsedConfig.output["tiles-new"])
      : []),
    ...(parsedConfig.output.tiles?.flatMap(tileIdsFromEntry) ?? []),
  ]
}

export const collectTileIds = (config: unknown): Set<string> => new Set(tileIdsFromConfig(config))

export const collectTileIdsFromConfigs = (configs: readonly unknown[]): Set<string> =>
  new Set(configs.flatMap(tileIdsFromConfig))

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

export const parseOutputFormat = (format: string): OutputFormat => {
  const parsedFormat = v.safeParse(OutputFormatSchema, format)
  if (parsedFormat.success) {
    return parsedFormat.output
  }

  throw new Error(`Unsupported --format '${format}'. Use one of: text, json, md.`)
}

const explicitIgnoredItemIdsFromValue = (value: unknown): string[] => {
  if (Array.isArray(value)) {
    return value.flatMap(explicitIgnoredItemIdsFromValue)
  }

  if (!isRecord(value)) {
    return []
  }

  const parsedValue = v.safeParse(IgnoredItemValueSchema, value)
  const explicitItemIds = !parsedValue.success ? [] : [
    parsedValue.output.fake_item,
    ...(parsedValue.output.type === "gun"
      ? [parsedValue.output.gun_type, parsedValue.output.ammo_type]
      : []),
  ].filter(isDefined)

  return [
    ...explicitItemIds,
    ...Object.values(value).flatMap(explicitIgnoredItemIdsFromValue),
  ]
}

export const collectExplicitIgnoredItemIds = (entries: readonly unknown[]): Set<string> =>
  new Set(entries.flatMap(explicitIgnoredItemIdsFromValue))

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
  const parsedDefinition = v.safeParse(DefinitionSchema, value)
  if (!parsedDefinition.success) {
    return null
  }

  const { abstract, flags, id, looks_like: looksLike, type } = parsedDefinition.output
  const definitionId = id ?? abstract
  if (!definitionId) {
    return null
  }

  return {
    id: definitionId,
    kind: id ? "item" : "abstract",
    type,
    path,
    copyFrom: parsedDefinition.output["copy-from"],
    looksLike,
    flags: asStringArray(flags),
  }
}

const loadDefinitions = async (inputs: readonly string[]) => {
  const loaded = await Promise.all(
    (await readJsonPaths(inputs)).map(async (path) => {
      const entries = parseJsonMaps(await Deno.readTextFile(path)).map(mapToObject)
      return {
        ignoredItemIds: Array.from(collectExplicitIgnoredItemIds(entries)),
        definitions: entries.flatMap((rawEntry) => {
          const definition = toDefinition(rawEntry, path)
          return definition ? [definition] : []
        }),
      }
    }),
  )

  const definitions = loaded.flatMap(({ definitions }) => definitions)
  const [itemDefinitions, abstractDefinitions] = partition(
    definitions,
    (definition) => definition.kind === "item",
  )

  return {
    items: new Map(itemDefinitions.map((definition) => [definition.id, definition] as const)),
    abstracts: new Map(
      abstractDefinitions.map((definition) => [definition.id, definition] as const),
    ),
    ignoredItemIds: new Set(loaded.flatMap(({ ignoredItemIds }) => ignoredItemIds)),
  }
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

const renderSection = (section: ReportSection, limit: number): RenderedReportSection => {
  const shownRows = limit > 0 ? section.rows.slice(0, limit) : section.rows
  return {
    ...section,
    shownRows,
    hiddenCount: section.rows.length - shownRows.length,
  }
}

const formatVariantOnlyEntry = (entry: VariantOnlyEntry): string =>
  `${entry.id}\t${entry.tileId}\t${entry.path}`

const formatMissingEntry = (entry: MissingEntry): string => `${entry.id}\t${entry.path}`

const formatLooksLikeEntry = (entry: LooksLikeEntry): string =>
  `${entry.chain.join(" -> ")}\t${entry.path}`

const renderTextSectionLines = (section: RenderedReportSection): string[] => [
  "",
  `${section.title} (${section.rows.length})`,
  ...(section.rows.length === 0 ? ["  (none)"] : [
    ...section.shownRows,
    ...(section.hiddenCount > 0 ? [`... ${section.hiddenCount} more`] : []),
  ]),
]

const renderMarkdownSectionLines = (section: RenderedReportSection): string[] => [
  "",
  `## ${section.title} (${section.rows.length})`,
  "",
  ...(section.rows.length === 0 ? ["(none)"] : [
    "```text",
    ...section.shownRows,
    ...(section.hiddenCount > 0 ? [`... ${section.hiddenCount} more`] : []),
    "```",
  ]),
]

const getReportSections = (report: MissingTilesetReport): ReportSection[] => [
  {
    title: "Missing id (but variant tile exists)",
    rows: report.variantOnly.map(formatVariantOnlyEntry),
  },
  {
    title: "Missing id",
    rows: report.missing.map(formatMissingEntry),
  },
  ...(report.looksLikeOnly
    ? [{
      title: "Missing id (but looks_like tile exists)",
      rows: report.looksLikeOnly.map(formatLooksLikeEntry),
    }]
    : []),
]

const limitEntries = <T>(entries: readonly T[], limit: number, total: number = entries.length) => {
  const shownEntries = limit > 0 ? entries.slice(0, limit) : entries
  return {
    total,
    shown: shownEntries.length,
    hidden: total - shownEntries.length,
    entries: shownEntries,
  }
}

export const renderTextReport = (report: MissingTilesetReport, limit: number): string => {
  return [
    `Tilesets: ${report.tilesets.join(", ")}`,
    `Inputs: ${report.inputs.join(", ")}`,
    `Checked ids: ${report.checkedIds}`,
    `Direct sprites: ${report.directSprites}`,
    `Variant sprites only: ${report.variantSpritesOnly}`,
    ...(report.looksLikeOnly !== undefined
      ? [`Looks_like fallback only: ${report.looksLikeFallbackOnly}`]
      : []),
    `Missing entirely: ${report.missingEntirely}`,
    ...getReportSections(report)
      .map((section) => renderSection(section, limit))
      .flatMap(renderTextSectionLines),
  ].join("\n")
}

export const renderJsonReport = (report: MissingTilesetReport, limit: number): string => {
  const looksLikeOnlyEntries = report.looksLikeOnly?.map((entry) => ({
    id: entry.id,
    tile_id: entry.tileId,
    chain: entry.chain,
    path: entry.path,
  })) ?? []

  return JSON.stringify(
    {
      tilesets: report.tilesets,
      inputs: report.inputs,
      counts: {
        checked_ids: report.checkedIds,
        direct_sprites: report.directSprites,
        variant_sprites_only: report.variantSpritesOnly,
        looks_like_fallback_only: report.looksLikeFallbackOnly,
        missing_entirely: report.missingEntirely,
      },
      variant_only: limitEntries(
        report.variantOnly.map((entry) => ({
          id: entry.id,
          tile_id: entry.tileId,
          path: entry.path,
        })),
        limit,
      ),
      missing: limitEntries(
        report.missing.map((entry) => ({
          id: entry.id,
          path: entry.path,
        })),
        limit,
      ),
      looks_like_only: limitEntries(
        looksLikeOnlyEntries,
        limit,
        report.looksLikeFallbackOnly,
      ),
    },
    null,
    2,
  )
}

export const renderMarkdownReport = (report: MissingTilesetReport, limit: number): string => {
  return [
    "# Tileset Missing Sprites",
    "",
    `- Tilesets: ${report.tilesets.map((path) => `\`${path}\``).join(", ")}`,
    `- Inputs: ${report.inputs.map((path) => `\`${path}\``).join(", ")}`,
    `- Checked ids: ${report.checkedIds}`,
    `- Direct sprites: ${report.directSprites}`,
    `- Variant sprites only: ${report.variantSpritesOnly}`,
    ...(report.looksLikeOnly !== undefined
      ? [`- Looks_like fallback only: ${report.looksLikeFallbackOnly}`]
      : []),
    `- Missing entirely: ${report.missingEntirely}`,
    ...getReportSections(report)
      .map((section) => renderSection(section, limit))
      .flatMap(renderMarkdownSectionLines),
  ].join("\n")
}

export const renderReport = (
  report: MissingTilesetReport,
  format: OutputFormat,
  limit: number,
): string => {
  switch (format) {
    case "json":
      return renderJsonReport(report, limit)
    case "md":
      return renderMarkdownReport(report, limit)
    default:
      return renderTextReport(report, limit)
  }
}

const main = new Command()
  .description("Report items and monsters missing sprites in one or more tilesets")
  .option("--tileset <path:string>", "Tileset path(s) to inspect", {
    collect: true,
    default: DEFAULT_TILESETS,
  })
  .option("--input <path:string>", "JSON path(s) with items and monsters to inspect", {
    collect: true,
    default: DEFAULT_INPUTS,
  })
  .option("--looks-like", "Report items that are only covered by looks_like", {
    default: false,
  })
  .option("--format <format:string>", "Output format: text, json, or md", {
    default: "text",
    value: parseOutputFormat,
  })
  .option("--limit <count:number>", "Max rows to print per section", { default: 0 })
  .action(async ({ format, input, limit, looksLike, tileset }) => {
    const tilesets = Array.from((tileset ?? DEFAULT_TILESETS) as string[])
    const inputs = Array.from((input ?? DEFAULT_INPUTS) as string[])
    const showLooksLike = Boolean(looksLike ?? false)
    const outputFormat = format as OutputFormat
    const rowLimit = Number(limit ?? 0)

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
    const { items, abstracts, ignoredItemIds } = await loadDefinitions(inputs)
    const resolvedItems = resolveItemDefinitions(items, abstracts)
    const reportableItems = Array.from(resolvedItems.values())
      .filter((item) => !shouldIgnoreItemDefinition(item, ignoredItemIds))
      .sort((left, right) => left.id.localeCompare(right.id))
    const [directItems, unresolvedItems] = partition(
      reportableItems,
      (item) => tileIds.has(item.id),
    )

    const variantOnly: VariantOnlyEntry[] = []
    const fallbackOnly: LooksLikeEntry[] = []
    const missing: MissingEntry[] = []

    for (const item of unresolvedItems) {
      const variantTileId = variantTileLookup.get(item.id)
      if (variantTileId) {
        variantOnly.push({
          id: item.id,
          tileId: variantTileId,
          path: toDisplayPath(item.path),
        })
        continue
      }

      const lookup = findTileByLooksLike(item.id, resolvedItems, tileIds, variantTileLookup)
      if (lookup) {
        fallbackOnly.push({
          id: item.id,
          tileId: lookup.tileId,
          chain: lookup.chain,
          path: toDisplayPath(item.path),
        })
        continue
      }

      missing.push({ id: item.id, path: toDisplayPath(item.path) })
    }

    const report: MissingTilesetReport = {
      tilesets: tilesets.map(toDisplayPath),
      inputs: inputs.map(toDisplayPath),
      checkedIds: reportableItems.length,
      directSprites: directItems.length,
      variantSpritesOnly: variantOnly.length,
      looksLikeFallbackOnly: fallbackOnly.length,
      missingEntirely: missing.length,
      variantOnly,
      missing,
      looksLikeOnly: showLooksLike ? fallbackOnly : undefined,
    }

    console.log(renderReport(report, outputFormat, rowLimit))
  })

if (import.meta.main) {
  try {
    await main.parse(Deno.args)
  } catch (error) {
    console.error(error instanceof Error ? error.message : String(error))
    Deno.exit(1)
  }
}
