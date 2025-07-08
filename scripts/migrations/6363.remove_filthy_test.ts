import * as v from "@valibot/valibot"
import {
  HarvestEntryWithoutFilthy,
  ItemWithoutFilthy,
  UseActionWithoutFilthy,
} from "./6363.remove_filthy.ts"
import { assertEquals } from "@std/assert"

Deno.test("ItemWithoutFilthy filters filthy_volume_threshold", () => {
  const survivalKit = {
    "id": "premium_survival_kit",
    "use_action": { "type": "unpack", "group": "premium_contents", "items_fit": true },
    "flags": ["NO_REPAIR", "NO_SALVAGE"],
  }

  assertEquals(
    v.parse(ItemWithoutFilthy, {
      ...survivalKit,
      use_action: { ...survivalKit.use_action, "filthy_volume_threshold": "10 L" },
      flags: [...survivalKit.flags, "FILTHY"],
    }),
    survivalKit,
  )
})

Deno.test("UseActionWithoutFilthy is idempotent", () => {
  const item = { "use_action": ["SOLARPACK"] }
  assertEquals(v.parse(UseActionWithoutFilthy, item.use_action), item.use_action)
})

Deno.test("HarvestEntryWithoutFilthy filters flags", () => {
  assertEquals(
    v.parse(HarvestEntryWithoutFilthy, {
      type: "harvest",
      entries: [{ "drop": "spider", "type": "bionic_group", "flags": ["FILTHY"] }],
    }),
    {
      type: "harvest",
      entries: [{ "drop": "spider", "type": "bionic_group", flags: [] }],
    },
  )
})
