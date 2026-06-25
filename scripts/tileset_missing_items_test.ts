import { assertEquals, assertThrows } from "@std/assert"

import {
  buildVariantTileLookup,
  collectExplicitIgnoredItemIds,
  collectTileIdsFromConfigs,
  getJsonPathSkips,
  getTileIdAliases,
  parseOutputFormat,
  renderJsonReport,
  renderMarkdownReport,
  renderTextReport,
  resolveItemDefinitions,
  shouldIgnoreItemDefinition,
} from "./tileset_missing_items.ts"

Deno.test("collectTileIdsFromConfigs() merges tile ids across tilesets", () => {
  const firstConfig = {
    "tiles-new": [{
      tiles: [
        { id: "item_a" },
        { id: ["item_b", "item_c"] },
      ],
    }],
  }
  const secondConfig = [{
    type: "mod_tileset",
    "tiles-new": [{
      tiles: [
        {
          id: "item_d",
          additional_tiles: [{ id: "item_e" }],
        },
        { id: "item_b" },
      ],
    }],
  }]

  assertEquals(
    Array.from(collectTileIdsFromConfigs([firstConfig, secondConfig])).sort(),
    ["item_a", "item_b", "item_c", "item_d", "item_e"],
  )
})

Deno.test("getJsonPathSkips() keeps mod_tileset for tileset sources", () => {
  assertEquals(getJsonPathSkips("tileset"), [])
  assertEquals(
    getJsonPathSkips("input").some((pattern) => pattern.test("mod_tileset.json")),
    true,
  )
})

Deno.test("getTileIdAliases() strips common item tile variants", () => {
  assertEquals(
    Array.from(getTileIdAliases("fern_harvested_season_winter")).sort(),
    ["fern", "fern_harvested", "fern_harvested_season_winter"],
  )
  assertEquals(
    Array.from(getTileIdAliases("overlay_wielded_bullet_vibrator_on")).sort(),
    [
      "bullet_vibrator",
      "bullet_vibrator_on",
      "overlay_wielded_bullet_vibrator",
      "overlay_wielded_bullet_vibrator_on",
    ],
  )
})

Deno.test("buildVariantTileLookup() resolves base ids from variant-only tiles", () => {
  const variantTileLookup = buildVariantTileLookup([
    "fern_harvested_season_winter",
    "overlay_wielded_bullet_vibrator_on",
  ])

  assertEquals(variantTileLookup.get("fern"), "fern_harvested_season_winter")
  assertEquals(variantTileLookup.get("bullet_vibrator"), "overlay_wielded_bullet_vibrator_on")
})

Deno.test("collectExplicitIgnoredItemIds() collects explicit fake item references", () => {
  const ignoredItemIds = collectExplicitIgnoredItemIds([
    { fake_item: "bio_wire_weapon" },
    {
      type: "gun",
      gun_type: "acid_artillery",
      ammo_type: "acid_artillery_shell",
    },
  ])

  assertEquals(Array.from(ignoredItemIds).sort(), [
    "acid_artillery",
    "acid_artillery_shell",
    "bio_wire_weapon",
  ])
})

Deno.test("shouldIgnoreItemDefinition() ignores fake_item inheritance and explicit ids", () => {
  const items = new Map<string, {
    id: string
    kind: "item"
    type: string
    path: string
    copyFrom?: string
    looksLike?: string
    flags: string[]
  }>([
    ["fake_tool", {
      id: "fake_tool",
      kind: "item",
      type: "TOOL",
      path: "items/fake.json",
      copyFrom: "fake_item",
      looksLike: undefined,
      flags: [],
    }],
    ["acid_artillery", {
      id: "acid_artillery",
      kind: "item",
      type: "GUN",
      path: "monster_attacks.json",
      looksLike: undefined,
      flags: [],
    }],
  ])
  const abstracts = new Map<string, {
    id: string
    kind: "abstract"
    type: string
    path: string
    copyFrom?: string
    looksLike?: string
    flags: string[]
  }>([
    ["fake_item", {
      id: "fake_item",
      kind: "abstract",
      type: "GENERIC",
      path: "items/fake.json",
      looksLike: undefined,
      flags: [],
    }],
  ])
  const resolvedItems = resolveItemDefinitions(items, abstracts)

  assertEquals(shouldIgnoreItemDefinition(resolvedItems.get("fake_tool")!, new Set()), true)
  assertEquals(
    shouldIgnoreItemDefinition(resolvedItems.get("acid_artillery")!, new Set(["acid_artillery"])),
    true,
  )
})

Deno.test("parseOutputFormat() accepts text, json, and md", () => {
  assertEquals(parseOutputFormat("text"), "text")
  assertEquals(parseOutputFormat("json"), "json")
  assertEquals(parseOutputFormat("md"), "md")
})

Deno.test("parseOutputFormat() rejects unsupported formats", () => {
  assertThrows(
    () => parseOutputFormat("yaml"),
    Error,
    "Unsupported --format 'yaml'. Use one of: text, json, md.",
  )
})

Deno.test("renderers format the report consistently", () => {
  const report = {
    tilesets: ["gfx/MSX++UnDeadPeopleEdition", "data/json/external_tileset"],
    inputs: ["data/json"],
    checkedIds: 10,
    directSprites: 7,
    variantSpritesOnly: 1,
    looksLikeFallbackOnly: 1,
    missingEntirely: 1,
    variantOnly: [
      {
        id: "seatbelt",
        tileId: "overlay_worn_seatbelt",
        path: "data/json/items/generic/string.json",
      },
    ],
    missing: [{ id: "missing_thing", path: "data/json/items/generic.json" }],
    looksLikeOnly: [{
      id: "manual_luty",
      tileId: "pocket_firearms",
      chain: ["manual_luty", "pocket_firearms"],
      path: "data/json/items/book/gun.json",
    }],
  }

  assertEquals(
    renderTextReport(report, 0).includes("Looks_like fallback only: 1"),
    true,
  )
  assertEquals(
    renderMarkdownReport(report, 1).includes("# Tileset Missing Sprites"),
    true,
  )

  const jsonReport = JSON.parse(renderJsonReport(report, 1)) as {
    counts: { looks_like_fallback_only: number }
    variant_only: { entries: Array<{ id: string; tile_id: string }>; shown: number; hidden: number }
    looks_like_only: { entries: Array<{ id: string; tile_id: string; chain: string[] }> }
  }
  assertEquals(jsonReport.counts.looks_like_fallback_only, 1)
  assertEquals(jsonReport.variant_only.entries[0].id, "seatbelt")
  assertEquals(jsonReport.variant_only.entries[0].tile_id, "overlay_worn_seatbelt")
  assertEquals(jsonReport.variant_only.shown, 1)
  assertEquals(jsonReport.variant_only.hidden, 0)
  assertEquals(jsonReport.looks_like_only.entries[0].id, "manual_luty")
  assertEquals(jsonReport.looks_like_only.entries[0].tile_id, "pocket_firearms")
  assertEquals(jsonReport.looks_like_only.entries[0].chain[0], "manual_luty")
})

Deno.test("renderJsonReport() keeps looks_like schema without detailed entries", () => {
  const report = {
    tilesets: ["gfx/MSX++UnDeadPeopleEdition"],
    inputs: ["data/json"],
    checkedIds: 4,
    directSprites: 2,
    variantSpritesOnly: 1,
    looksLikeFallbackOnly: 3,
    missingEntirely: 1,
    variantOnly: [],
    missing: [{ id: "missing_thing", path: "data/json/items/generic.json" }],
  }

  const jsonReport = JSON.parse(renderJsonReport(report, 0)) as {
    counts: { looks_like_fallback_only: number }
    looks_like_only: { total: number; shown: number; hidden: number; entries: unknown[] }
  }

  assertEquals(jsonReport.counts.looks_like_fallback_only, 3)
  assertEquals(jsonReport.looks_like_only.total, 3)
  assertEquals(jsonReport.looks_like_only.shown, 0)
  assertEquals(jsonReport.looks_like_only.hidden, 3)
  assertEquals(jsonReport.looks_like_only.entries, [])
})
