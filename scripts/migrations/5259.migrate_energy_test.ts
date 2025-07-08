import * as v from "@valibot/valibot"
import { assertEquals } from "@std/assert"
import { GunsMigrateEnergy, ItemMigrateEnergy } from "./5259.migrate_energy.ts"

Deno.test("GunsMigrateEnergy migrates ups_charges -> power_draw", () => {
  assertEquals(v.parse(GunsMigrateEnergy, { ups_charges: 10 }), { power_draw: "10 kJ" })
})

Deno.test("ItemMigrateEnergy migrates fields", () => {
  const given = {
    "id": "talking_doll",
    "type": "TOOL",
    "ammo": "battery",
    "charges_per_use": 1,
    "use_action": "DOLLCHAT",
    "magazines": [
      [
        "battery",
        [
          "light_minus_disposable_cell",
          "light_disposable_cell",
          "light_minus_battery_cell",
          "light_battery_cell",
          "light_plus_battery_cell",
          "light_atomic_battery_cell",
          "light_minus_atomic_battery_cell",
        ],
      ],
    ],
    "magazine_well": "250 ml",
  }
  const expected = {
    "id": "talking_doll",
    "type": "TOOL",
    "use_action": "DOLLCHAT",
    "battery_well": "250 ml",
    "power_draw": "1 kJ",
    "batteries": [
      "light_minus_disposable_cell",
      "light_disposable_cell",
      "light_minus_battery_cell",
      "light_battery_cell",
      "light_plus_battery_cell",
      "light_atomic_battery_cell",
      "light_minus_atomic_battery_cell",
    ],
  }
  assertEquals(v.parse(ItemMigrateEnergy, given), expected)
})

Deno.test("ItemMigrateEnergy migrates TOOL_ARMOR", () => {
  const given = {
    "id": "exosuit_survivor",
    "type": "TOOL_ARMOR",
    "category": "armor",
    "max_charges": 500,
    "ammo": "battery",
    "use_action": {
      "type": "transform",
      "msg": "The %s engages.",
      "target": "exosuit_survivor_on",
      "active": true,
      "need_worn": true,
      "need_charges": 1,
    },
  }
  const expected = {
    "id": "exosuit_survivor",
    "type": "TOOL_ARMOR",
    "category": "armor",
    "max_power": "500 kJ",
    "use_action": {
      "type": "transform",
      "msg": "The %s engages.",
      "target": "exosuit_survivor_on",
      "active": true,
      "need_worn": true,
      "need_power": "1 kJ",
    },
  }
  assertEquals(v.parse(ItemMigrateEnergy, given), expected)
})
