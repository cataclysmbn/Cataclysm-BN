import { assertEquals } from "@std/assert"
import { schema, spearToStab } from "./4214.spear_stab.ts"

Deno.test("If an item has both SPEAR AND REACH_ATTACK, just add STAB.", () => {
  const pitchfork = {
    id: "pitchfork",
    type: "GENERIC",
    category: "weapons",
    weapon_category: ["SPEARS"],
    name: { str: "pitchfork" },
    description:
      "An agricultural tool with long wooden shaft and four spikes.  Is used to lift hay.",
    weight: "1000 g",
    color: "brown",
    symbol: "/",
    material: ["steel", "wood"],
    techniques: ["WBLOCK_1", "DEF_DISARM"],
    volume: "1750 ml",
    bashing: 5,
    cutting: 23,
    qualities: [["COOK", 1]],
    flags: ["SPEAR", "REACH_ATTACK", "NONCONDUCTIVE", "SHEATH_SPEAR"],
    price: "20 USD",
    price_postapoc: "5 USD",
  }
  const expected = {
    id: "pitchfork",
    type: "GENERIC",
    category: "weapons",
    weapon_category: ["SPEARS"],
    name: { str: "pitchfork" },
    description:
      "An agricultural tool with long wooden shaft and four spikes.  Is used to lift hay.",
    weight: "1000 g",
    color: "brown",
    symbol: "/",
    material: ["steel", "wood"],
    techniques: ["WBLOCK_1", "DEF_DISARM"],
    volume: "1750 ml",
    bashing: 5,
    cutting: 23,
    qualities: [["COOK", 1]],
    flags: ["SPEAR", "REACH_ATTACK", "NONCONDUCTIVE", "SHEATH_SPEAR", "STAB"],
    price: "20 USD",
    price_postapoc: "5 USD",
  }
  assertEquals(schema.parse(pitchfork), expected)
})

Deno.test("If an item has SPEAR but does NOT have REACH_ATTACK, replace SPEAR with STAB", () => {
  const morningstar = {
    id: "morningstar",
    type: "GENERIC",
    category: "weapons",
    weapon_category: ["MORNINGSTARS"],
    name: { "str": "morningstar" },
    description:
      "A medieval weapon consisting of a wood handle with a heavy, spiked iron ball on the end.  It deals devastating crushing damage, with a small amount of piercing to boot.",
    weight: "1400 g",
    to_hit: -1,
    color: "dark_gray",
    symbol: "/",
    material: ["iron", "wood"],
    techniques: ["SWEEP"],
    volume: "1500 ml",
    bashing: 38,
    cutting: 6,
    flags: ["DURABLE_MELEE", "SPEAR", "NONCONDUCTIVE"],
    price: "1200 USD",
    price_postapoc: "80 USD",
    qualities: [["HAMMER", 1]],
  }
  const expected = {
    id: "morningstar",
    type: "GENERIC",
    category: "weapons",
    weapon_category: ["MORNINGSTARS"],
    name: { "str": "morningstar" },
    description:
      "A medieval weapon consisting of a wood handle with a heavy, spiked iron ball on the end.  It deals devastating crushing damage, with a small amount of piercing to boot.",
    weight: "1400 g",
    to_hit: -1,
    color: "dark_gray",
    symbol: "/",
    material: ["iron", "wood"],
    techniques: ["SWEEP"],
    volume: "1500 ml",
    bashing: 38,
    cutting: 6,
    flags: ["DURABLE_MELEE", "STAB", "NONCONDUCTIVE"],
    price: "1200 USD",
    price_postapoc: "80 USD",
    qualities: [["HAMMER", 1]],
  }
  assertEquals(schema.parse(morningstar), expected)
})

Deno.test("spearToStab", async (t) => {
  await t.step("if a flag already has STAB, ignore it", () => {
    assertEquals(spearToStab(["STAB"]), ["STAB"])
    assertEquals(spearToStab(["SPEAR", "STAB"]), ["SPEAR", "STAB"])
  })
  await t.step("if a flag has both SPEAR and REACH_ATTACK, add STAB", () => {
    assertEquals(spearToStab(["SPEAR", "REACH_ATTACK"]), ["SPEAR", "REACH_ATTACK", "STAB"])
  })

  await t.step(
    "if a flag has SPEAR but not REACH_ATTACK, replace SPEAR with STAB with its position kept",
    () => {
      assertEquals(spearToStab(["SPEAR"]), ["STAB"])
      assertEquals(spearToStab(["FOO", "SPEAR", "BAR"]), ["FOO", "STAB", "BAR"])
    },
  )
})
