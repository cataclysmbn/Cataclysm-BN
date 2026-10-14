/** @module The scenarios whose recorded wire becomes the contract examples. */
import { aim } from "./aim.ts"
import { construction, crafting } from "./crafting.ts"
import type { Scenario } from "./driver.ts"
import { handshake } from "./handshake.ts"
import { generic } from "./generic.ts"
import { inventory } from "./inventory.ts"
import { look } from "./look.ts"
import { map } from "./map.ts"
import { pickup } from "./pickup.ts"
import { title } from "./title.ts"
import { trade } from "./trade.ts"
import { tutorial } from "./tutorial.ts"

export const scenarios: Scenario[] = [
  handshake,
  tutorial,
  generic,
  map,
  trade,
  aim,
  inventory,
  pickup,
  title,
  crafting,
  construction,
  look,
]
