import * as v from "@valibot/valibot"

export type JsonValue = null | boolean | number | string | JsonArray | JsonObject
export type JsonArray = JsonValue[]
export type JsonObject = { [key: string]: JsonValue }

export type PotEntry = {
  msgid: string
  msgidPlural?: string
  msgctxt?: string
  comment?: string
  flags: string[]
  source: string
}

export type ExtractorState = {
  currentSourceFile: string
  entries: PotEntry[]
  projectName?: string
  verbose: boolean
  warnUnusedTypes: boolean
  suppressWarningForFiles: Set<string>
}

export type CliOptions = {
  project?: string
  verbose?: boolean
  input?: string[]
  exclude?: string[]
  excludeDir?: string[]
  trackedOnly?: boolean
  output: string
  suppress?: string[]
  warnUnusedTypes?: boolean
}

export type LuaNode = {
  type?: string
  name?: string
  identifier?: { name?: string }
  base?: LuaNode
  expression?: LuaNode
  arguments?: LuaNode[]
  body?: LuaNode[]
  comments?: LuaCommentRaw[]
  loc?: { start?: { line?: number }; end?: { line?: number } }
  raw?: string
  value?: string | number | boolean | null
  [key: string]: unknown
}

export type LuaCommentRaw = {
  value?: string
  raw?: string
  loc?: { start?: { line?: number }; end?: { line?: number } }
}

export type LuaComment = {
  line: number
  text: string
  isTranslatorComment: boolean
  used: boolean
}

export const ignorable = new Set([
  "ascii_art",
  "ammo_effect",
  "behavior",
  "charge_removal_blacklist",
  "city_building",
  "colordef",
  "construction_sequence",
  "disease_type",
  "emit",
  "enchantment",
  "event_transformation",
  "event_statistic",
  "EXTERNAL_OPTION",
  "hit_range",
  "ITEM_BLACKLIST",
  "item_group",
  "MIGRATION",
  "mod_tileset",
  "monster_adjustment",
  "MONSTER_BLACKLIST",
  "MONSTER_FACTION",
  "monstergroup",
  "MONSTER_WHITELIST",
  "mutation_type",
  "oter_id_migration",
  "overlay_order",
  "overmap_connection",
  "overmap_location",
  "overmap_special",
  "profession_item_substitutions",
  "palette",
  "region_overlay",
  "region_settings",
  "requirement",
  "rotatable_symbol",
  "SCENARIO_BLACKLIST",
  "scent_type",
  "skill_boost",
  "to_cbc_migration",
  "TRAIT_BLACKLIST",
  "trait_group",
  "uncraft",
  "vehicle_group",
  "vehicle_placement",
  "WORLD_OPTION",
  "sound_effect",
])

export const automaticallyConvertible = new Set([
  "achievement",
  "activity_type",
  "AMMO",
  "ammunition_type",
  "ARMOR",
  "BATTERY",
  "bionic",
  "BIONIC_ITEM",
  "BOOK",
  "COMESTIBLE",
  "construction_category",
  "construction_group",
  "CONTAINER",
  "dream",
  "ENGINE",
  "event_statistic",
  "faction",
  "furniture",
  "GENERIC",
  "item_action",
  "ITEM_CATEGORY",
  "mutation_flag",
  "keybinding",
  "LOOT_ZONE",
  "MAGAZINE",
  "map_extra",
  "MOD_INFO",
  "MONSTER",
  "morale_type",
  "npc",
  "npc_class",
  "overmap_land_use_code",
  "overmap_terrain",
  "PET_ARMOR",
  "score",
  "skill",
  "SPECIES",
  "speech",
  "SPELL",
  "start_location",
  "terrain",
  "TOOL",
  "TOOLMOD",
  "TOOL_ARMOR",
  "tool_quality",
  "vehicle",
  "vehicle_part",
  "vitamin",
  "WHEEL",
  "help",
  "weather_type",
  "world_type",
])

export const needsPlural = new Set([
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
  "SPECIES",
  "TOOL",
  "TOOLMOD",
  "TOOL_ARMOR",
  "WHEEL",
])

export const useActionMessages = new Set([
  "activate_msg",
  "deactive_msg",
  "out_of_power_msg",
  "msg",
  "menu_text",
  "message",
  "friendly_msg",
  "hostile_msg",
  "need_fire_msg",
  "need_charges_msg",
  "non_interactive_msg",
  "unfold_msg",
  "sound_msg",
  "no_deactivate_msg",
  "not_ready_msg",
  "success_message",
  "lacks_fuel_message",
  "failure_message",
  "descriptions",
  "use_message",
  "noise_message",
  "bury_question",
  "done_message",
  "voluntary_extinguish_message",
  "charges_extinguish_message",
  "water_extinguish_message",
  "auto_extinguish_message",
  "activation_message",
  "holster_msg",
  "holster_prompt",
  "verb",
  "gerund",
])

export const gettextFunctions = new Map<
  string,
  { context: boolean; plural: boolean; expected: number }
>([
  ["gettext", { context: false, plural: false, expected: 1 }],
  ["pgettext", { context: true, plural: false, expected: 2 }],
  ["vgettext", { context: false, plural: true, expected: 3 }],
  ["vpgettext", { context: true, plural: true, expected: 4 }],
])

export const toArray = <T>(value: T | T[] | undefined): T[] => {
  if (value === undefined) return []
  return Array.isArray(value) ? value : [value]
}

export const logVerbose = (state: ExtractorState, message: string) => {
  if (state.verbose) console.log(message)
}

export const isObject = (value: JsonValue | unknown): value is JsonObject => {
  return typeof value === "object" && value !== null && !Array.isArray(value)
}

export const isArray = (value: JsonValue | unknown): value is JsonArray => Array.isArray(value)

export const isString = (value: JsonValue | unknown): value is string => typeof value === "string"

const JsonObjectSchema = v.custom<JsonObject>(isObject)
const JsonRootSchema = v.union([JsonObjectSchema, v.array(v.unknown())])

export const parseJsonObjects = (value: unknown): JsonObject[] => {
  const parsed = v.safeParse(JsonRootSchema, value)
  if (!parsed.success) return []
  return Array.isArray(parsed.output) ? parsed.output.filter(isObject) : [parsed.output]
}

export const sourceName = (path: string) => path.split("\\").join("/")

export const jsonStringLiteral = (text: string) => JSON.stringify(text)

export const poEscape = (text: string): string => {
  return text
    .replaceAll("\\", "\\\\")
    .replaceAll("\t", "\\t")
    .replaceAll("\r", "\\r")
    .replaceAll("\n", "\\n")
    .replaceAll('"', '\\"')
}

export const poLine = (key: string, value: string): string => `${key} "${poEscape(value)}"`

export const rawTranslationString = (value: JsonValue | undefined): string | undefined => {
  if (isString(value)) return value
  if (isObject(value)) {
    const str = value.str ?? value.str_sp
    return isString(str) ? str : undefined
  }
  return undefined
}

const pythonStringRepr = (value: string): string => {
  const escaped = value.replaceAll("\\", "\\\\")
  if (escaped.includes("'") && !escaped.includes('"')) return `"${escaped.replaceAll('"', '\\"')}"`
  return `'${escaped.replaceAll("'", "\\'")}'`
}

const pythonReprQuoted = (value: JsonValue | undefined): string => {
  if (isString(value)) return pythonStringRepr(value)
  return pythonRepr(value)
}

export const pythonRepr = (value: JsonValue | undefined): string => {
  if (value === undefined || value === null) return "None"
  if (isString(value)) return value
  if (typeof value === "number") return String(value)
  if (typeof value === "boolean") return value ? "True" : "False"
  if (isArray(value)) return `[${value.map(pythonReprQuoted).join(", ")}]`
  return `{${
    Object.entries(value).map(([key, entry]) => `'${key}': ${pythonReprQuoted(entry)}`).join(", ")
  }}`
}

const allGenders = ["f", "m", "n"]
export const genderOptions = (subject: string): string[] =>
  allGenders.map((gender) => `${subject}:${gender}`)

export const addContext = (entry: JsonValue | undefined, context: string): JsonObject => {
  if (isObject(entry)) {
    const previous = entry.ctxt
    return { ...entry, ctxt: isString(previous) ? `${previous}|${context}` : context }
  }
  return { str: entry ?? "", ctxt: context }
}

export const objectArray = (value: JsonValue | undefined): JsonObject[] => {
  if (!isArray(value)) return []
  return value.filter(isObject)
}

export const jsonArray = (value: JsonValue | undefined): JsonValue[] => isArray(value) ? value : []

export const jsonObjectEntries = (value: JsonValue | undefined): [string, JsonValue][] => {
  if (!isObject(value)) return []
  return Object.entries(value).sort(([a], [b]) => a.localeCompare(b))
}

export const rawNameList = (value: JsonValue | undefined): string[] => {
  if (!isArray(value)) return []
  return value.map(rawTranslationString).filter((entry): entry is string => entry !== undefined)
}
