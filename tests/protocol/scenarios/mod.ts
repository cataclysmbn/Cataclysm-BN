/** @module The scenarios whose recorded wire becomes the contract examples. */
import { character } from "./character.ts"
import { aim } from "./aim.ts"
import { armor } from "./armor.ts"
import { autonote } from "./autonote.ts"
import { construction, crafting } from "./crafting.ts"
import { diary } from "./diary.ts"
import { editmap } from "./editmap.ts"
import { editor } from "./editor.ts"
import { editorMenu } from "./editor_menu.ts"
import type { Scenario } from "./driver.ts"
import { handshake } from "./handshake.ts"
import { generic } from "./generic.ts"
import { info } from "./info.ts"
import { inventory } from "./inventory.ts"
import { look } from "./look.ts"
import { map } from "./map.ts"
import { modes } from "./modes.ts"
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
import { vehicleEditor } from "./vehicle_editor.ts"

export const scenarios: Scenario[] = [
  handshake,
  tutorial,
  generic,
  map,
  trade,
  aim,
  inventory,
  pickup,
  modes,
  title,
  crafting,
  construction,
  look,
  editmap,
  editor,
  editorMenu,
  vehicleEditor,
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
