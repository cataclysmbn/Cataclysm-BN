/** @module The scenarios whose recorded wire becomes the contract examples. */
import { character } from "./character.ts"
import { aim } from "./aim.ts"
import { armor } from "./armor.ts"
import { autonote } from "./autonote.ts"
import { construction, crafting } from "./crafting.ts"
import { diary } from "./diary.ts"
import type { Scenario } from "./driver.ts"
import { handshake } from "./handshake.ts"
import { generic } from "./generic.ts"
import { info } from "./info.ts"
import { inventory } from "./inventory.ts"
import { look } from "./look.ts"
import { map } from "./map.ts"
import { newgame } from "./newgame.ts"
import { overmap } from "./overmap.ts"
import { npctalk } from "./npctalk.ts"
import { pickup } from "./pickup.ts"
import { autopickup, colors, distractions, safemode } from "./rules.ts"
import { keybindings, options } from "./settings.ts"
import { title } from "./title.ts"
import { trade } from "./trade.ts"
import { tutorial } from "./tutorial.ts"
import { vehicle } from "./vehicle.ts"

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
  vehicle,
  character,
  info,
  newgame,
  options,
  keybindings,
  distractions,
  colors,
  autopickup,
  safemode,
  armor,
  overmap,
  npctalk,
  diary,
  autonote,
]
