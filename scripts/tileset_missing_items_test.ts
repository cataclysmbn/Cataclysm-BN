import { assertEquals } from "@std/assert"

import {
  buildVariantTileLookup,
  collectExplicitIgnoredItemIds,
  collectTileIdsFromConfigs,
  getJsonPathSkips,
  getTileIdAliases,
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
