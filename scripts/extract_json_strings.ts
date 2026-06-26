#!/usr/bin/env -S deno run --allow-read --allow-write --allow-run
/**
 * @module
 *
 * Extract translatable strings from JSON, JSONC, and Lua files into a POT file.
 *
 * This script keeps the command line surface used by the in-repo extraction shell scripts.
 */

import { Command } from "@cliffy/command"
import { walk } from "@std/fs"
import { dirname, normalize } from "@std/path"
import { parse as parseJsonc } from "@std/jsonc"
import luaparse from "npm:luaparse@0.3.1"

type JsonValue = null | boolean | number | string | JsonArray | JsonObject
type JsonArray = JsonValue[]
type JsonObject = { [key: string]: JsonValue }

type PotEntry = {
  msgid: string
  msgidPlural?: string
  msgctxt?: string
  comment?: string
  flags: string[]
  source: string
}

type ExtractorState = {
  currentSourceFile: string
  entries: PotEntry[]
  projectName?: string
  verbose: boolean
  warnUnusedTypes: boolean
  suppressWarningForFiles: Set<string>
}

type CliOptions = {
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

type LuaNode = {
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

type LuaCommentRaw = {
  value?: string
  raw?: string
  loc?: { start?: { line?: number }; end?: { line?: number } }
}

type LuaComment = {
  line: number
  text: string
  isTranslatorComment: boolean
  used: boolean
}

const ignorable = new Set([
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

const automaticallyConvertible = new Set([
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

const needsPlural = new Set([
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

const useActionMessages = new Set([
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

const foundTypes = new Set<string>()
const warnedUnknownTypes = new Set<string>()

const jsonStringPattern = String.raw`"(?:\\.|[^"\\])*"`
const memberStringPattern = new RegExp(
  String.raw`^(\s*)(${jsonStringPattern}\s*:\s*)(${jsonStringPattern})(\s*,?\s*)$`,
)
const arrayStringPattern = new RegExp(String.raw`^(\s*)(${jsonStringPattern})(\s*,?\s*)$`)
const translationStringKeys = new Set(["str", "str_sp", "str_pl"])

const gettextFunctions = new Map<string, { context: boolean; plural: boolean; expected: number }>([
  ["gettext", { context: false, plural: false, expected: 1 }],
  ["pgettext", { context: true, plural: false, expected: 2 }],
  ["vgettext", { context: false, plural: true, expected: 3 }],
  ["vpgettext", { context: true, plural: true, expected: 4 }],
])

const toArray = <T>(value: T | T[] | undefined): T[] => {
  if (value === undefined) return []
  return Array.isArray(value) ? value : [value]
}

const logVerbose = (state: ExtractorState, message: string) => {
  if (state.verbose) console.log(message)
}

const isObject = (value: JsonValue | undefined): value is JsonObject => {
  return typeof value === "object" && value !== null && !Array.isArray(value)
}

const isArray = (value: JsonValue | undefined): value is JsonArray => Array.isArray(value)

const isString = (value: JsonValue | undefined): value is string => typeof value === "string"

const sourceName = (path: string) => path.split("\\").join("/")

const jsonStringLiteral = (text: string) => JSON.stringify(text)

const injectJsoncTranslatorComments = (raw: string): string => {
  const out: string[] = []
  let pendingComments: string[] = []
  let pendingLineIndices: number[] = []

  for (const line of raw.match(/^.*(?:\r\n|\n|\r|$)/gm) ?? []) {
    if (line.length === 0) continue
    const lineBody = line.replace(/[\r\n]+$/, "")
    const newline = line.slice(lineBody.length)
    const commentMatch = lineBody.match(/^(\s*)\/\/~\s?(.*)$/)
    if (commentMatch) {
      pendingComments.push(commentMatch[2].trim())
      pendingLineIndices.push(out.length)
      out.push(newline)
      continue
    }

    if (pendingComments.length > 0 && lineBody.trim() && !lineBody.trimStart().startsWith("//")) {
      const commentJson = jsonStringLiteral(pendingComments.join("\n"))
      const memberMatch = lineBody.match(memberStringPattern)
      const arrayMatch = lineBody.match(arrayStringPattern)
      if (memberMatch) {
        const key = JSON.parse(memberMatch[2].split(":", 1)[0].trim()) as string
        if (translationStringKeys.has(key)) {
          out[pendingLineIndices.at(-1)!] = `${memberMatch[1]}"//~": ${commentJson},${newline}`
          out.push(line)
        } else {
          out.push(
            `${memberMatch[1]}${memberMatch[2]}{ "//~": ${commentJson}, "str": ${memberMatch[3]} }${
              memberMatch[4]
            }${newline}`,
          )
        }
      } else if (arrayMatch) {
        out.push(
          `${arrayMatch[1]}{ "//~": ${commentJson}, "str": ${arrayMatch[2]} }${
            arrayMatch[3]
          }${newline}`,
        )
      } else {
        const indent = lineBody.match(/^(\s*)/)?.[1] ?? ""
        out[pendingLineIndices.at(-1)!] = `${indent}"//~": ${commentJson},${newline}`
        out.push(line)
      }
      pendingComments = []
      pendingLineIndices = []
    } else {
      out.push(line)
    }
  }

  return out.join("")
}

const poEscape = (text: string): string => {
  return text
    .replaceAll("\\", "\\\\")
    .replaceAll("\t", "\\t")
    .replaceAll("\r", "\\r")
    .replaceAll("\n", "\\n")
    .replaceAll('"', '\\"')
}

const poLine = (key: string, value: string): string => `${key} "${poEscape(value)}"`

const writeStringBasic = (
  state: ExtractorState,
  msgid: string,
  msgidPlural?: string,
  msgctxt?: string,
  comment?: string,
  checkCFormat = false,
) => {
  const flags: string[] = []
  if (checkCFormat && (msgid.includes("%") || msgidPlural?.includes("%"))) {
    flags.push("c-format")
  }
  state.entries.push({
    msgid,
    msgidPlural,
    msgctxt,
    comment: comment ? `~ ${comment}` : undefined,
    flags,
    source: state.currentSourceFile,
  })
}

const rawTranslationString = (value: JsonValue | undefined): string | undefined => {
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

const pythonRepr = (value: JsonValue | undefined): string => {
  if (value === undefined || value === null) return "None"
  if (isString(value)) return value
  if (typeof value === "number") return String(value)
  if (typeof value === "boolean") return value ? "True" : "False"
  if (isArray(value)) return `[${value.map(pythonReprQuoted).join(", ")}]`
  return `{${
    Object.entries(value).map(([key, entry]) => `'${key}': ${pythonReprQuoted(entry)}`).join(", ")
  }}`
}

const writeString = (
  state: ExtractorState,
  value: JsonValue | undefined,
  options: { context?: string; formatStrings?: boolean; comment?: string; pluralFormat?: boolean } =
    {},
) => {
  if (isArray(value)) {
    for (const entry of value) writeString(state, entry, options)
    return
  }

  let strSingular: string | undefined
  let strPlural: string | undefined
  let context = options.context
  let comment = options.comment

  if (isObject(value)) {
    const translatorComment = value["//~"]
    if (isString(translatorComment)) {
      comment = comment === undefined ? translatorComment : `${comment}\n${translatorComment}`
    }
    const objectContext = value.ctxt
    if (isString(objectContext)) context = objectContext
    if (options.pluralFormat) {
      if (isString(value.str_pl)) strPlural = value.str_pl
      else if (isString(value.str_sp)) strPlural = value.str_sp
      else if (isString(value.str)) strPlural = `${value.str}s`
    }
    if (isString(value.str)) strSingular = value.str
    else if (isString(value.str_sp)) strSingular = value.str_sp
  } else if (isString(value)) {
    if (value.length === 0) return
    strSingular = value
    if (options.pluralFormat) strPlural = `${value}s`
  } else if (value === null || value === undefined) {
    return
  }

  if (strSingular === undefined) return
  writeStringBasic(state, strSingular, strPlural, context, comment, options.formatStrings)
}

const extractUseActionMessages = (
  state: ExtractorState,
  useAction: JsonValue | undefined,
  itemName?: string,
) => {
  if (isArray(useAction)) {
    for (const entry of useAction) extractUseActionMessages(state, entry, itemName)
  } else if (isObject(useAction)) {
    for (const key of Object.keys(useAction).sort()) {
      const value = useAction[key]
      if (useActionMessages.has(key) && itemName) {
        writeString(state, value, { comment: `Use action ${key} for ${itemName}.` })
      }
      extractUseActionMessages(state, value, itemName)
    }
  }
}

const allGenders = ["f", "m", "n"]
const genderOptions = (subject: string): string[] =>
  allGenders.map((gender) => `${subject}:${gender}`)

const addContext = (entry: JsonValue | undefined, context: string): JsonObject => {
  if (isObject(entry)) {
    const previous = entry.ctxt
    return { ...entry, ctxt: isString(previous) ? `${previous}|${context}` : context }
  }
  return { str: entry ?? "", ctxt: context }
}

const objectArray = (value: JsonValue | undefined): JsonObject[] => {
  if (!isArray(value)) return []
  return value.filter(isObject)
}

const jsonArray = (value: JsonValue | undefined): JsonValue[] => isArray(value) ? value : []

const jsonObjectEntries = (value: JsonValue | undefined): [string, JsonValue][] => {
  if (!isObject(value)) return []
  return Object.entries(value).sort(([a], [b]) => a.localeCompare(b))
}

const rawNameList = (value: JsonValue | undefined): string[] => {
  if (!isArray(value)) return []
  return value.map(rawTranslationString).filter((entry): entry is string => entry !== undefined)
}

const extractHarvest = (state: ExtractorState, item: JsonObject) => {
  if (item.message !== undefined) writeString(state, item.message)
}

const extractBodypart = (state: ExtractorState, item: JsonObject) => {
  for (
    const key of [
      "name",
      "name_multiple",
      "accusative",
      "accusative_multiple",
      "encumbrance_text",
      "heading",
      "heading_multiple",
      "hp_bar_ui_text",
    ] as const
  ) {
    if (item[key] !== undefined) writeString(state, item[key])
  }
}

const extractClothingMod = (state: ExtractorState, item: JsonObject) => {
  writeString(state, item.implement_prompt)
  writeString(state, item.destroy_prompt)
}

const extractConstruction = (state: ExtractorState, item: JsonObject) => {
  if (item.pre_note !== undefined) writeString(state, item.pre_note)
}

const extractMaterial = (state: ExtractorState, item: JsonObject) => {
  writeString(state, item.name)
  let wrote = false
  for (const key of ["bash_dmg_verb", "cut_dmg_verb"] as const) {
    if (item[key] !== undefined) {
      writeString(state, item[key])
      wrote = true
    }
  }
  const dmgAdj = jsonArray(item.dmg_adj)
  for (let idx = 0; idx < 4 && idx < dmgAdj.length; idx++) {
    writeString(state, dmgAdj[idx])
    wrote = true
  }
  if (!wrote && item["copy-from"] === undefined) {
    console.log(
      `WARNING: ${state.currentSourceFile}: no mandatory field in item: ${JSON.stringify(item)}`,
    )
  }
}

const extractMartialArt = (state: ExtractorState, item: JsonObject) => {
  const nameValue = item.name ?? item.id
  const name = pythonRepr(nameValue)
  if (item.name !== undefined) writeString(state, item.name)
  if (item.description !== undefined) {
    writeString(state, item.description, { comment: `Description for martial art '${name}'` })
  }
  if (item.initiate !== undefined) {
    writeString(state, item.initiate, {
      formatStrings: true,
      comment: `Initiate message for martial art '${name}'`,
    })
  }
  for (
    const buff of [
      ...objectArray(item.onhit_buffs),
      ...objectArray(item.static_buffs),
      ...objectArray(item.onmove_buffs),
      ...objectArray(item.ondodge_buffs),
    ]
  ) {
    const buffName = rawTranslationString(buff.name) ?? ""
    writeString(state, buff.name)
    writeString(state, buff.description, {
      comment: buffName === name
        ? `Description of buff for martial art '${name}'`
        : `Description of buff '${buffName}' for martial art '${name}'`,
    })
  }
}

const extractEffectType = (state: ExtractorState, item: JsonObject) => {
  const names = rawNameList(item.name)
  const contextNames = jsonArray(item.name)
  const desc = jsonArray(item.desc)
  if (names.length > 0 && names.length === desc.length) {
    for (let idx = 0; idx < names.length; idx++) {
      writeString(state, jsonArray(item.name)[idx])
      writeString(state, desc[idx], {
        formatStrings: true,
        comment: `Description of effect '${pythonRepr(contextNames[idx])}'.`,
      })
    }
  } else if (names.length > 0) {
    for (const entry of jsonArray(item.name)) writeString(state, entry)
    for (const key of ["desc", "reduced_desc"] as const) {
      for (const entry of jsonArray(item[key])) writeString(state, entry, { formatStrings: true })
    }
  }
  const nameText = names.join(", ")
  for (const [key, label] of [["apply_message", "Apply"], ["remove_message", "Remove"]] as const) {
    writeString(state, item[key], {
      formatStrings: true,
      comment: nameText ? `${label} message for effect(s) '${nameText}'.` : undefined,
    })
  }
  for (const [key, label] of [["miss_messages", "Miss"], ["decay_messages", "Decay"]] as const) {
    for (const entry of jsonArray(item[key])) {
      if (isArray(entry)) {
        writeString(state, entry[0], {
          comment: nameText ? `${label} message for effect(s) '${nameText}'.` : undefined,
        })
      }
    }
  }
  if (item.speed_name !== undefined) {
    writeString(state, item.speed_name, {
      comment: nameText ? `Speed name of effect(s) '${nameText}'.` : undefined,
    })
  }
  for (
    const [key, label] of [["apply_memorial_log", "apply"], [
      "remove_memorial_log",
      "remove",
    ]] as const
  ) {
    writeString(state, item[key], {
      context: "memorial_male",
      comment: nameText ? `Male memorial ${label} log for effect(s) '${nameText}'.` : undefined,
    })
    writeString(state, item[key], {
      context: "memorial_female",
      comment: nameText ? `Female memorial ${label} log for effect(s) '${nameText}'.` : undefined,
    })
  }
}

const extractGun = (state: ExtractorState, item: JsonObject) => {
  const type = rawTranslationString(item.type) ?? ""
  if (item.name !== undefined) {
    writeString(state, item.name, { pluralFormat: needsPlural.has(type) })
  }
  if (item.description !== undefined) writeString(state, item.description)
  for (const mode of jsonArray(item.modes)) if (isArray(mode)) writeString(state, mode[1])
  if (item.skill !== undefined && rawTranslationString(item.skill) !== "archery") {
    writeString(state, item.skill, { context: "gun_type_type" })
  }
  if (item.reload_noise !== undefined) writeString(state, item.reload_noise)
}

const extractGunmod = (state: ExtractorState, item: JsonObject) => {
  const type = rawTranslationString(item.type) ?? ""
  if (item.name !== undefined) {
    writeString(state, item.name, { pluralFormat: needsPlural.has(type) })
  }
  if (item.description !== undefined) writeString(state, item.description)
  for (const mode of jsonArray(item.mode_modifier)) if (isArray(mode)) writeString(state, mode[1])
  if (item.location !== undefined) writeString(state, item.location)
  for (const target of jsonArray(item.mod_targets)) {
    writeString(state, target, { context: "gun_type_type" })
  }
}

const extractProfession = (state: ExtractorState, item: JsonObject) => {
  let commentM = "???"
  let commentF = "???"
  if (item.name !== undefined) {
    const name = item.name
    const male = isObject(name) && name.male !== undefined && name.female !== undefined
      ? name.male
      : name
    const female = isObject(name) && name.male !== undefined && name.female !== undefined
      ? name.female
      : name
    const entryM = addContext(male, "profession_male")
    const entryF = addContext(female, "profession_female")
    writeString(state, entryM)
    writeString(state, entryF)
    commentM = rawTranslationString(entryM) ?? commentM
    commentF = rawTranslationString(entryF) ?? commentF
  }
  if (item.description !== undefined) {
    writeString(state, addContext(item.description, "prof_desc_male"), {
      comment: `Profession (male ${commentM}) description`,
    })
    writeString(state, addContext(item.description, "prof_desc_female"), {
      comment: `Profession (female ${commentF}) description`,
    })
  }
}

const extractScenario = (state: ExtractorState, item: JsonObject) => {
  const name = item.name
  const nameText = pythonRepr(name)
  writeString(state, name, {
    context: "scenario_male",
    comment: `Name for scenario '${nameText}' for a male character`,
  })
  writeString(state, name, {
    context: "scenario_female",
    comment: `Name for scenario '${nameText}' for a female character`,
  })
  if (name !== undefined) {
    if (item.description !== undefined) {
      writeString(state, item.description, {
        context: "scen_desc_male",
        comment: `Description for scenario '${nameText}' for a male character.`,
      })
      writeString(state, item.description, {
        context: "scen_desc_female",
        comment: `Description for scenario '${nameText}' for a female character.`,
      })
    }
    if (item.start_name !== undefined) {
      writeString(state, item.start_name, {
        context: "start_name",
        comment: `Starting location for scenario '${nameText}'.`,
      })
    }
  } else {
    writeString(state, item.description)
    writeString(state, item.start_name)
  }
}

const extractMapgen = (state: ExtractorState, item: JsonObject) => {
  if (!isObject(item.object)) return
  for (const [objKey, objVal] of jsonObjectEntries(item.object)) {
    if (objKey === "place_specials" || objKey === "place_signs") {
      for (const special of objectArray(objVal)) {
        for (const [key, value] of jsonObjectEntries(special)) {
          if (key === "signage") writeString(state, value, { comment: "Sign" })
        }
      }
    } else if (objKey === "signs" || objKey === "computers") {
      for (const [, value] of jsonObjectEntries(objVal)) {
        if (!isObject(value)) continue
        if (objKey === "signs") writeString(state, value.signage, { comment: "Sign" })
        else {
          if (value.name !== undefined) writeString(state, value.name, { comment: "Computer name" })
          for (const option of objectArray(value.options)) {
            writeString(state, option.name, { comment: "Computer option" })
          }
          if (value.access_denied !== undefined) {
            writeString(state, value.access_denied, { comment: "Computer access denied warning" })
          }
        }
      }
    }
  }
}

const extractMonsterAttack = (state: ExtractorState, item: JsonObject) => {
  for (const key of ["hit_dmg_u", "hit_dmg_npc", "no_dmg_msg_u", "no_dmg_msg_npc"] as const) {
    if (item[key] !== undefined) writeString(state, item[key])
  }
}

const extractRecipe = (state: ExtractorState, item: JsonObject) => {
  for (const bookLearn of jsonArray(item.book_learn)) {
    if (isArray(bookLearn) && bookLearn.length >= 3 && rawTranslationString(bookLearn[2])) {
      writeString(state, bookLearn[2])
    }
  }
  if (item.description !== undefined) writeString(state, item.description)
  if (item.blueprint_name !== undefined) writeString(state, item.blueprint_name)
}

const extractRecipeGroup = (state: ExtractorState, item: JsonObject) => {
  for (const recipe of objectArray(item.recipes)) writeString(state, recipe.description)
}

const extractGenderedDynamicLineOptional = (state: ExtractorState, line: JsonObject) => {
  if (line.gendered_line === undefined || !isArray(line.relevant_genders)) return
  const subjects = line.relevant_genders.map(rawTranslationString).filter((
    entry,
  ): entry is string => entry !== undefined)
  const options = subjects.map(genderOptions)
  const visit = (idx: number, context: string[]) => {
    if (idx === options.length) {
      writeString(state, line.gendered_line, { context: context.join(" ") })
      return
    }
    for (const option of options[idx]) visit(idx + 1, [...context, option])
  }
  visit(0, [])
}

const dynamicLineStringKeys = [
  "u_male",
  "u_female",
  "npc_male",
  "npc_female",
  "has_no_assigned_mission",
  "has_assigned_mission",
  "has_many_assigned_missions",
  "has_no_available_mission",
  "has_available_mission",
  "has_many_available_missions",
  "mission_complete",
  "mission_incomplete",
  "mission_has_generic_rewards",
  "npc_available",
  "npc_following",
  "npc_friend",
  "npc_hostile",
  "npc_train_skills",
  "npc_train_styles",
  "at_safe_space",
  "is_day",
  "npc_has_activity",
  "is_outside",
  "u_has_camp",
  "u_can_stow_weapon",
  "npc_can_stow_weapon",
  "u_has_weapon",
  "npc_has_weapon",
  "u_driving",
  "npc_driving",
  "has_pickup_list",
  "is_by_radio",
  "has_reason",
  "yes",
  "no",
  "and",
]

const extractDynamicLine = (state: ExtractorState, line: JsonValue | undefined) => {
  if (isArray(line)) { for (const entry of line) extractDynamicLine(state, entry) }
  else if (isObject(line)) {
    extractGenderedDynamicLineOptional(state, line)
    for (const key of dynamicLineStringKeys) {
      if (line[key] !== undefined) extractDynamicLine(state, line[key])
    }
  } else if (isString(line)) writeString(state, line)
}

const extractTalkEffects = (state: ExtractorState, effects: JsonValue | undefined) => {
  for (const effect of isArray(effects) ? effects : [effects]) {
    if (isObject(effect) && effect.u_buy_monster !== undefined && effect.name !== undefined) {
      writeString(state, effect.name, {
        comment: `Nickname for creature '${pythonRepr(effect.u_buy_monster)}'`,
      })
    }
  }
}

const extractTalkResponse = (state: ExtractorState, response: JsonObject) => {
  if (response.text !== undefined) writeString(state, response.text)
  if (isObject(response.truefalsetext)) {
    writeString(state, response.truefalsetext.true)
    writeString(state, response.truefalsetext.false)
  }
  if (isObject(response.success)) extractTalkResponse(state, response.success)
  if (isObject(response.failure)) extractTalkResponse(state, response.failure)
  for (const effect of objectArray(response.speaker_effect)) {
    if (effect.effect !== undefined) extractTalkEffects(state, effect.effect)
  }
  if (response.effect !== undefined) extractTalkEffects(state, response.effect)
}

const extractTalkTopic = (state: ExtractorState, item: JsonObject) => {
  if (item.dynamic_line !== undefined) extractDynamicLine(state, item.dynamic_line)
  for (const response of objectArray(item.responses)) extractTalkResponse(state, response)
  if (item.effect !== undefined) extractTalkEffects(state, item.effect)
}

const extractTechnique = (state: ExtractorState, item: JsonObject) => {
  writeString(state, item.name)
  if (item.description !== undefined) writeString(state, item.description)
  for (const message of jsonArray(item.messages)) {
    writeString(state, message, { formatStrings: true })
  }
}

const extractTrap = (state: ExtractorState, item: JsonObject) => {
  writeString(state, item.name)
  if (isObject(item.vehicle_data) && item.vehicle_data.sound !== undefined) {
    writeString(state, item.vehicle_data.sound, {
      comment: `Trap-vehicle collision message for trap '${pythonRepr(item.name)}'`,
    })
  }
}

const extractMissionDef = (state: ExtractorState, item: JsonObject) => {
  const itemName = item.name
  writeString(state, itemName)
  const itemNameText = pythonRepr(itemName)
  if (item.description !== undefined) {
    writeString(state, item.description, { comment: `Description for mission '${itemNameText}'` })
  }
  if (isObject(item.dialogue)) {
    for (
      const key of [
        "describe",
        "offer",
        "accepted",
        "rejected",
        "advice",
        "inquire",
        "success",
        "success_lie",
        "failure",
      ] as const
    ) {
      if (item.dialogue[key] !== undefined) writeString(state, item.dialogue[key])
    }
  }
  for (const key of ["start", "end", "fail"] as const) {
    const value = item[key]
    if (isObject(value) && value.effect !== undefined) extractTalkEffects(state, value.effect)
  }
}

const extractMutation = (state: ExtractorState, item: JsonObject) => {
  const itemNameOrId = item.name ?? item.id
  if (item.name !== undefined) writeString(state, item.name)
  if (item.description !== undefined) {
    writeString(state, item.description, {
      comment: `Description for ${pythonRepr(itemNameOrId)}`,
    })
  }
  const attacks = item.attacks
  for (
    const attack of isArray(attacks) ? objectArray(attacks) : isObject(attacks) ? [attacks] : []
  ) {
    if (attack.attack_text_u !== undefined) writeString(state, attack.attack_text_u)
    if (attack.attack_text_npc !== undefined) writeString(state, attack.attack_text_npc)
  }
  if (isObject(item.spawn_item)) writeString(state, item.spawn_item.message)
}

const extractMutationCategory = (state: ExtractorState, item: JsonObject) => {
  const itemName = item.name
  const itemNameText = pythonRepr(itemName)
  writeString(state, itemName, { comment: "Mutation class name" })
  for (
    const key of [
      "mutagen_message",
      "iv_message",
      "iv_sleep_message",
      "iv_sound_message",
      "junkie_message",
    ] as const
  ) {
    if (item[key] !== undefined) {
      writeString(state, item[key], { comment: `Mutation class: ${itemNameText} ${key}` })
    }
  }
  writeString(state, item.memorial_message, {
    context: "memorial_male",
    comment: `Mutation class: ${itemNameText} Male memorial messsage`,
  })
  writeString(state, item.memorial_message, {
    context: "memorial_female",
    comment: `Mutation class: ${itemNameText} Female memorial messsage`,
  })
}

const extractVehspawn = (state: ExtractorState, item: JsonObject) => {
  for (const spawnType of objectArray(item.spawn_types)) {
    writeString(state, spawnType.description, { comment: "Vehicle Spawn Description" })
  }
}

const extractRecipeCategory = (state: ExtractorState, item: JsonObject) => {
  const id = rawTranslationString(item.id)
  if (!id || id === "CC_NONCRAFT") return
  const catName = id.split("_")[1]
  writeString(state, catName, { comment: "Crafting recipes category name" })
  for (const subcatValue of jsonArray(item.recipe_subcategories)) {
    const subcat = rawTranslationString(subcatValue)
    if (!subcat) continue
    if (subcat === "CSC_ALL") {
      writeString(state, "ALL", { comment: "Crafting recipes subcategory all" })
    } else {writeString(state, subcat.split("_")[2], {
        comment: `Crafting recipes subcategory of '${catName}' category`,
      })}
  }
}

const extractGate = (state: ExtractorState, item: JsonObject) => {
  for (const [key, value] of jsonObjectEntries(item.messages)) {
    writeString(state, value, { comment: `'${key}' action message of some gate object.` })
  }
}

const extractFieldType = (state: ExtractorState, item: JsonObject) => {
  for (const level of objectArray(item.intensity_levels)) {
    if (level.name !== undefined) writeString(state, level.name)
  }
}

const extractTerFurnTransform = (state: ExtractorState, item: JsonObject) => {
  writeString(state, item.fail_message)
  for (const terrain of objectArray(item.terrain)) writeString(state, terrain.message)
  for (const furniture of objectArray(item.furniture)) writeString(state, furniture.message)
}

const extractSkillDisplayType = (state: ExtractorState, item: JsonObject) => {
  writeString(state, item.display_string, {
    comment: `Display string for skill display type '${rawTranslationString(item.id)}'`,
  })
}

const extractFault = (state: ExtractorState, item: JsonObject) => {
  const name = pythonRepr(item.name)
  writeString(state, item.name)
  writeString(state, item.description, { comment: `Description for fault '${name}'` })
  for (const method of objectArray(item.mending_methods)) {
    const methodName = pythonRepr(method.name)
    if (method.name !== undefined) {
      writeString(state, method.name, { comment: `Name of mending method for fault '${name}'` })
    }
    if (method.description !== undefined) {
      writeString(state, method.description, {
        comment: `Description for mending method '${methodName}' of fault '${name}'`,
      })
    }
    if (method.success_msg !== undefined) {
      writeString(state, method.success_msg, {
        formatStrings: true,
        comment: `Success message for mending method '${methodName}' of fault '${name}'`,
      })
    }
  }
}

const extractJsonFlag = (state: ExtractorState, item: JsonObject) => {
  const id = rawTranslationString(item.id)
  for (const field of ["info", "restriction", "tag"] as const) {
    if (item[field] !== undefined) {
      writeString(state, item[field], { comment: `${field} for JSON flag '${id}'` })
    }
  }
}

const extractSnippet = (state: ExtractorState, item: JsonObject) => {
  for (const snippet of isArray(item.text) ? item.text : [item.text]) {
    writeString(state, isObject(snippet) ? snippet.text : snippet)
  }
}

const extractWeaponCategory = (state: ExtractorState, item: JsonObject) => {
  writeString(state, item.name, { comment: "weapon category name" })
}

const extractSpecials = new Map<string, (state: ExtractorState, item: JsonObject) => void>([
  ["body_part", extractBodypart],
  ["clothing_mod", extractClothingMod],
  ["construction", extractConstruction],
  ["effect_type", extractEffectType],
  ["fault", extractFault],
  ["field_type", extractFieldType],
  ["gate", extractGate],
  ["GUN", extractGun],
  ["GUNMOD", extractGunmod],
  ["harvest", extractHarvest],
  ["mapgen", extractMapgen],
  ["martial_art", extractMartialArt],
  ["material", extractMaterial],
  ["mission_definition", extractMissionDef],
  ["monster_attack", extractMonsterAttack],
  ["mutation_category", extractMutationCategory],
  ["mutation", extractMutation],
  ["profession", extractProfession],
  ["recipe_category", extractRecipeCategory],
  ["recipe_group", extractRecipeGroup],
  ["recipe", extractRecipe],
  ["scenario", extractScenario],
  ["skill_display_type", extractSkillDisplayType],
  ["json_flag", extractJsonFlag],
  ["snippet", extractSnippet],
  ["talk_topic", extractTalkTopic],
  ["technique", extractTechnique],
  ["ter_furn_transform", extractTerFurnTransform],
  ["trap", extractTrap],
  ["vehicle_spawn", extractVehspawn],
  ["weapon_category", extractWeaponCategory],
])

const shouldSuppressWarning = (state: ExtractorState, file: string): boolean => {
  for (const suppressed of state.suppressWarningForFiles) {
    if (file.startsWith(suppressed)) return true
  }
  return false
}

const extractJsonObject = (state: ExtractorState, item: JsonObject) => {
  const objectType = item.type
  if (!isString(objectType)) return
  foundTypes.add(objectType)
  if (ignorable.has(objectType)) return
  const specialExtractor = extractSpecials.get(objectType)
  if (specialExtractor) {
    specialExtractor(state, item)
    return
  }
  if (!automaticallyConvertible.has(objectType)) {
    if (!warnedUnknownTypes.has(objectType)) {
      warnedUnknownTypes.add(objectType)
      console.log(
        `WARNING: Skipping unrecognized object type '${objectType}' in '${state.currentSourceFile}'`,
      )
    }
    return
  }

  if (objectType === "MOD_INFO" && state.projectName === undefined && isString(item.id)) {
    state.projectName = item.id
  }

  let wrote = false
  const name = item.name
  const nameText = rawTranslationString(name)
  const commentNameText = nameText ?? "None"
  if (nameText === "none") return
  if (name !== undefined) {
    writeString(state, name, { pluralFormat: needsPlural.has(objectType) })
    wrote = true
  }

  for (const key of ["name_suffix", "name_unique", "job_description"] as const) {
    if (item[key] !== undefined) {
      writeString(state, item[key])
      wrote = true
    }
  }

  if (item.use_action !== undefined) {
    extractUseActionMessages(state, item.use_action, nameText)
    wrote = true
  }

  if (isArray(item.conditional_names)) {
    for (const conditionalName of item.conditional_names) {
      if (!isObject(conditionalName)) continue
      const type = rawTranslationString(conditionalName.type)
      const condition = rawTranslationString(conditionalName.condition)
      writeString(state, conditionalName.name, {
        comment: `Conditional name for ${commentNameText} when ${type} matches ${condition}`,
        formatStrings: true,
        pluralFormat: true,
      })
      wrote = true
    }
  }

  if (item.description !== undefined) {
    writeString(state, item.description, {
      comment: nameText ? `Description for ${nameText}` : undefined,
    })
    wrote = true
  }

  for (const key of ["detailed_definition", "sound", "text", "prompt"] as const) {
    if (item[key] !== undefined) {
      writeString(state, item[key])
      wrote = true
    }
  }

  if (item.sound_description !== undefined) {
    writeString(state, item.sound_description, {
      comment: `Description for the sound of spell '${commentNameText}'`,
    })
    wrote = true
  }

  if (isArray(item.snippet_category)) {
    for (const entry of item.snippet_category) {
      writeString(state, isObject(entry) ? entry.text : entry)
      wrote = true
    }
  }

  if (isObject(item.bash)) {
    for (const key of ["sound", "sound_fail"] as const) {
      if (item.bash[key] !== undefined) {
        writeString(state, item.bash[key])
        wrote = true
      }
    }
  }

  if (isObject(item.oxytorch) && item.oxytorch.message !== undefined) {
    writeString(state, item.oxytorch.message, {
      comment: `message when oxytorch cutting ${commentNameText}`,
    })
    wrote = true
  }

  for (
    const [member, soundComment, messageComment] of [
      ["hacksaw", "sound of sawing", "message when finished sawing"],
      ["boltcut", "sound of bolt cutting", "message when finished bolt cutting"],
    ] as const
  ) {
    const value = item[member]
    if (!isObject(value)) continue
    if (value.sound !== undefined) {
      writeString(state, value.sound, { comment: `${soundComment} ${commentNameText}` })
      wrote = true
    }
    if (value.message !== undefined) {
      writeString(state, value.message, { comment: `${messageComment} ${commentNameText}` })
      wrote = true
    }
  }

  if (isObject(item.pry)) {
    for (
      const key of [
        "sound",
        "break_sound",
        "success_message",
        "fail_message",
        "break_message",
      ] as const
    ) {
      if (item.pry[key] !== undefined) {
        writeString(state, item.pry[key])
        wrote = true
      }
    }
  }

  if (item.lockpick_message !== undefined) {
    writeString(state, item.lockpick_message)
    wrote = true
  }

  if (isObject(item.seed_data) && item.seed_data.plant_name !== undefined) {
    writeString(state, item.seed_data.plant_name)
    wrote = true
  }

  if (isObject(item.relic_data)) {
    if (item.relic_data.name !== undefined) {
      writeString(state, item.relic_data.name)
      wrote = true
    }
    if (isArray(item.relic_data.recharge_scheme)) {
      for (const recharge of item.relic_data.recharge_scheme) {
        if (isObject(recharge) && recharge.message !== undefined) {
          writeString(state, recharge.message, {
            comment: `Relic recharge message for ${objectType} '${commentNameText}'`,
          })
          wrote = true
        }
      }
    }
  }

  if (item.message !== undefined) {
    writeString(state, item.message, {
      formatStrings: true,
      comment: `Message for ${objectType} '${commentNameText}'`,
    })
    wrote = true
  }

  if (isArray(item.messages)) {
    for (const message of item.messages) {
      writeString(state, message)
      wrote = true
    }
  }

  if (isArray(item.valid_mod_locations)) {
    for (const modLocation of item.valid_mod_locations) {
      if (isArray(modLocation)) writeString(state, modLocation[0])
      wrote = true
    }
  }

  if (item.info !== undefined) {
    writeString(state, item.info, {
      comment: "Please leave anything in <angle brackets> unchanged.",
    })
    wrote = true
  }

  if (item.verb !== undefined) {
    writeString(state, item.verb)
    wrote = true
  }

  if (isArray(item.special_attacks)) {
    for (const specialAttack of item.special_attacks) {
      if (!isObject(specialAttack)) continue
      if (specialAttack.description !== undefined) {
        writeString(state, specialAttack.description)
        wrote = true
      }
      if (specialAttack.monster_message !== undefined) {
        writeString(state, specialAttack.monster_message, {
          formatStrings: true,
          comment: `Attack message of monster "${commentNameText}"'s spell "${
            pythonRepr(specialAttack.spell_id)
          }"`,
        })
        wrote = true
      }
    }
  }

  if (item.footsteps !== undefined) {
    writeString(state, item.footsteps)
    wrote = true
  }

  if (
    !wrote && item["copy-from"] === undefined &&
    !shouldSuppressWarning(state, state.currentSourceFile)
  ) {
    console.log(
      `WARNING: ${state.currentSourceFile}: nothing translatable found in item: ${
        JSON.stringify(item)
      }`,
    )
  }
}

const parseJsonFile = async (path: string): Promise<JsonValue> => {
  const raw = await Deno.readTextFile(path)
  return parseJsonc(injectJsoncTranslatorComments(raw)) as JsonValue
}

const extractJsonFile = async (state: ExtractorState, path: string) => {
  state.currentSourceFile = sourceName(path)
  logVerbose(state, `Loading ${path}`)
  const data = await parseJsonFile(path)
  if (isArray(data)) {
    for (const entry of data) {
      if (isObject(entry)) extractJsonObject(state, entry)
    }
  } else if (isObject(data)) {
    extractJsonObject(state, data)
  }
}

const luaLiteralValue = (raw: string): string => {
  const rawBody = raw.slice(1, -1)
  return rawBody
    .replaceAll("\\n", "\n")
    .replaceAll("\\t", "\t")
    .replaceAll("\\r", "\r")
    .replaceAll("\\\\", "\\")
}

const luaStringValue = (node: LuaNode | undefined): string => {
  if (!node || node.type !== "StringLiteral") {
    throw new Error("argument to translation call should be string")
  }
  if (typeof node.value === "string") return node.value
  if (typeof node.raw === "string") {
    const quote = node.raw[0]
    if ((quote === '"' || quote === "'") && node.raw.at(-1) === quote) {
      return luaLiteralValue(node.raw)
    }
  }
  throw new Error("argument to translation call should be string")
}

const collectLuaCalls = (node: LuaNode | unknown, calls: LuaNode[] = []): LuaNode[] => {
  if (!node || typeof node !== "object") return calls
  const typedNode = node as LuaNode
  if (typedNode.type === "CallExpression") calls.push(typedNode)
  for (const value of Object.values(typedNode)) {
    if (Array.isArray(value)) {
      for (const entry of value) collectLuaCalls(entry, calls)
    } else if (value && typeof value === "object") {
      collectLuaCalls(value, calls)
    }
  }
  return calls
}

const luaFunctionName = (call: LuaNode): string | undefined => {
  const base = call.base
  if (!base) return undefined
  if (base.type === "Identifier") return base.name
  if (base.type === "MemberExpression" || base.type === "IndexExpression") {
    const identifier = base.identifier
    if (identifier?.name) return identifier.name
  }
  return undefined
}

const parseLuaComments = (rawComments: LuaCommentRaw[] = []): LuaComment[] => {
  return rawComments.map((comment) => {
    const raw = comment.raw ?? ""
    const value = comment.value ?? raw.replace(/^--/, "")
    const isTranslatorComment = raw.startsWith("--~") || value.trimStart().startsWith("~")
    const text = isTranslatorComment ? value.replace(/^\s*~\s?/, "").trim() : value.trim()
    return { line: comment.loc?.start?.line ?? 0, text, isTranslatorComment, used: false }
  })
}

const findAdjacentLuaTranslatorComments = (
  comments: LuaComment[],
  line: number,
): string[] => {
  const found: string[] = []
  let nextLine = line - 1
  while (true) {
    const comment = comments.find((entry) => entry.isTranslatorComment && entry.line === nextLine)
    if (!comment) break
    comment.used = true
    found.unshift(comment.text)
    nextLine -= 1
  }
  return found
}

const findLuaTranslatorCommentsBefore = (
  comments: LuaComment[],
  line: number,
): string | undefined => {
  let cursorLine = line
  while (true) {
    const found = findAdjacentLuaTranslatorComments(comments, cursorLine)
    if (found.length > 0) return found.join("\n")
    const regularComment = comments.find(
      (entry) => !entry.isTranslatorComment && entry.line === cursorLine - 1,
    )
    if (!regularComment) return undefined
    cursorLine = regularComment.line
  }
}

const luaCallRanges = (line: string): Array<{ name: string; args: string }> => {
  const ranges: Array<{ name: string; args: string }> = []
  const callStart = /\b(gettext|pgettext|vgettext|vpgettext)\s*\(/g
  for (const match of line.matchAll(callStart)) {
    const name = match[1]
    let idx = (match.index ?? 0) + match[0].length
    let depth = 1
    let quote = ""
    let escaped = false
    const start = idx
    while (idx < line.length) {
      const ch = line[idx]
      if (quote) {
        if (escaped) escaped = false
        else if (ch === "\\") escaped = true
        else if (ch === quote) quote = ""
      } else if (ch === '"' || ch === "'") quote = ch
      else if (ch === "(") depth += 1
      else if (ch === ")") {
        depth -= 1
        if (depth === 0) break
      }
      idx += 1
    }
    if (depth === 0) ranges.push({ name, args: line.slice(start, idx) })
  }
  return ranges
}

const extractLuaCall = (
  state: ExtractorState,
  call: { name: string; args: string },
  commentText?: string,
) => {
  if (call.args.includes("..")) return
  const literalRegex = /"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'/g
  const args = [...call.args.matchAll(literalRegex)].map((match) => luaLiteralValue(match[0]))
  if (call.name === "gettext" && args.length >= 1) {
    writeStringBasic(state, args[0], undefined, undefined, commentText, true)
  } else if (call.name === "pgettext" && args.length >= 2) {
    writeStringBasic(state, args[1], undefined, args[0], commentText, true)
  } else if (call.name === "vgettext" && args.length >= 2) {
    writeStringBasic(state, args[0], args[1], undefined, commentText, true)
  } else if (call.name === "vpgettext" && args.length >= 3) {
    writeStringBasic(state, args[1], args[2], args[0], commentText, true)
  }
}

const extractLuaWithRegex = (state: ExtractorState, raw: string) => {
  const pendingComments: string[] = []
  let inBlockComment = false
  for (const line of raw.split(/\r?\n/)) {
    const trimmed = line.trimStart()
    if (inBlockComment) {
      if (trimmed.includes("]]")) inBlockComment = false
      continue
    }
    if (trimmed.startsWith("--[[")) {
      if (!trimmed.includes("]]")) inBlockComment = true
      continue
    }
    const comment = line.match(/^\s*--~\s?(.*)$/)
    if (comment) {
      pendingComments.push(comment[1].trim())
      continue
    }
    if (trimmed.startsWith("--")) continue
    const calls = luaCallRanges(line)
    if (calls.length === 0) {
      if (line.trim()) pendingComments.length = 0
      continue
    }
    for (const call of calls) {
      const commentText = pendingComments.length > 0 ? pendingComments.join("\n") : undefined
      pendingComments.length = 0
      extractLuaCall(state, call, commentText)
    }
  }

  const blocklessRaw = raw
    .replace(/--\[\[[\s\S]*?\]\]/g, "")
    .replace(/^\s*--.*$/gm, "")
  for (const call of luaCallRanges(blocklessRaw)) {
    if (call.args.includes("\n")) extractLuaCall(state, call)
  }
}

const extractLuaFile = async (state: ExtractorState, path: string) => {
  state.currentSourceFile = sourceName(path)
  logVerbose(state, `Loading ${path}`)
  const raw = await Deno.readTextFile(path)
  let ast: LuaNode
  try {
    ast = luaparse.parse(raw, { comments: true, locations: true, ranges: true }) as LuaNode
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error)
    console.log(`WARNING: Could not parse Lua file '${state.currentSourceFile}': ${message}`)
    extractLuaWithRegex(state, raw)
    return
  }
  const comments = parseLuaComments(ast.comments)
  for (const call of collectLuaCalls(ast)) {
    const functionName = luaFunctionName(call)
    const spec = functionName ? gettextFunctions.get(functionName) : undefined
    if (!spec) continue
    const args = call.arguments ?? []
    if (args.length !== spec.expected) {
      console.log(
        `WARNING: invalid amount of arguments in translation call (found ${args.length}, expected ${spec.expected})`,
      )
      continue
    }
    try {
      const msgctxt = spec.context ? luaStringValue(args[0]) : undefined
      const msgid = luaStringValue(args[spec.context ? 1 : 0])
      const msgidPlural = spec.plural ? luaStringValue(args[spec.context ? 2 : 1]) : undefined
      const comment = findLuaTranslatorCommentsBefore(comments, call.loc?.start?.line ?? 0)
      writeStringBasic(state, msgid, msgidPlural, msgctxt, comment, true)
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error)
      console.log(`WARNING: ${message}`)
    }
  }
  for (const comment of comments) {
    if (comment.isTranslatorComment && !comment.used) {
      console.log(
        `WARNING: unused translator comment at ${state.currentSourceFile}:${comment.line}`,
      )
    }
  }
}

const trackedFiles = async (): Promise<Set<string>> => {
  const command = new Deno.Command("git", { args: ["ls-files"], stdout: "piped", stderr: "piped" })
  const { code, stdout, stderr } = await command.output()
  if (code !== 0) {
    throw new Error(new TextDecoder().decode(stderr))
  }
  return new Set(new TextDecoder().decode(stdout).split(/\r?\n/).filter(Boolean).map(normalize))
}

const extractAllFromDir = async (
  state: ExtractorState,
  directory: string,
  options: { ignoredFiles: Set<string>; ignoredDirs: Set<string>; tracked?: Set<string> },
) => {
  const entries = []
  for await (
    const entry of walk(directory, { includeDirs: false, exts: [".json", ".jsonc", ".lua"] })
  ) {
    entries.push(entry.path)
  }
  entries.sort()
  for (const path of entries) {
    const normalized = normalize(path)
    if (options.ignoredFiles.has(normalized)) {
      logVerbose(state, `Skipping file (ignored): '${path}'`)
      continue
    }
    if ([...options.ignoredDirs].some((dir) => normalized.startsWith(dir))) {
      logVerbose(state, `Skipping file in ignored dir: '${path}'`)
      continue
    }
    if (options.tracked && !options.tracked.has(normalized)) {
      logVerbose(state, `Skipping file (untracked): '${path}'`)
      continue
    }
    if (path.endsWith(".json") || path.endsWith(".jsonc")) await extractJsonFile(state, path)
    else if (path.endsWith(".lua")) await extractLuaFile(state, path)
  }
}

const wrapPoComment = (comment: string): string[] => {
  const prefix = "#. "
  const width = 78
  if (comment.length + prefix.length <= width) return [`${prefix}${comment}`]
  const chunks = comment.replaceAll(/\s/g, " ").match(/ +|[^ ]+/g) ?? []
  const lines: string[] = []
  let current = prefix
  let pendingSpace = ""

  const pushCurrent = () => {
    lines.push(current.trimEnd())
    current = prefix
    pendingSpace = ""
  }

  for (const chunk of chunks) {
    if (chunk.trim() === "") {
      pendingSpace += chunk
      continue
    }
    let word = chunk
    while (word.length > 0) {
      const separator = current === prefix ? "" : pendingSpace
      const candidate = `${current}${separator}${word}`
      if (candidate.length <= width) {
        current = candidate
        pendingSpace = ""
        word = ""
        continue
      }
      const hyphenIndex = word.lastIndexOf("-", width - current.length - separator.length)
      if (hyphenIndex > 0) {
        const head = word.slice(0, hyphenIndex + 1)
        const headCandidate = `${current}${separator}${head}`
        if (headCandidate.length <= width) {
          current = headCandidate
          pushCurrent()
          word = word.slice(hyphenIndex + 1)
          continue
        }
      }
      if (current !== prefix) pushCurrent()
      else {
        current = `${prefix}${word}`
        word = ""
      }
    }
  }
  lines.push(current.trimEnd())
  return lines
}

const formatPot = (entries: PotEntry[], projectName: string): string => {
  const lines = [
    "#",
    'msgid ""',
    'msgstr ""',
    `"Project-Id-Version: ${poEscape(projectName)}\\n"`,
    `"POT-Creation-Date: ${poEscape(new Date().toISOString())}\\n"`,
    '"Language: \\n"',
    '"MIME-Version: 1.0\\n"',
    '"Content-Type: text/plain; charset=UTF-8\\n"',
    '"Content-Transfer-Encoding: 8bit\\n"',
    "",
  ]

  for (const entry of entries) {
    if (entry.comment) {
      for (const line of entry.comment.split("\n")) lines.push(...wrapPoComment(line))
    }
    lines.push(`#: ${sourceName(entry.source)}`)
    if (entry.flags.length > 0) lines.push(`#, ${entry.flags.join(", ")}`)
    if (entry.msgctxt !== undefined) lines.push(poLine("msgctxt", entry.msgctxt))
    lines.push(poLine("msgid", entry.msgid))
    if (entry.msgidPlural !== undefined) {
      lines.push(poLine("msgid_plural", entry.msgidPlural))
      lines.push('msgstr[0] ""')
      lines.push('msgstr[1] ""')
    } else {
      lines.push('msgstr ""')
    }
    lines.push("")
  }
  return `${lines.join("\n")}`
}

const run = async (options: CliOptions) => {
  const inputFolders = toArray(options.input)
  if (inputFolders.length === 0) throw new Error("Missing input list")
  if (!options.output) throw new Error("Missing output file")

  const tracked = options.trackedOnly ? await trackedFiles() : undefined
  const state: ExtractorState = {
    currentSourceFile: "",
    entries: [],
    projectName: options.project,
    verbose: options.verbose ?? false,
    warnUnusedTypes: options.warnUnusedTypes ?? false,
    suppressWarningForFiles: new Set(toArray(options.suppress).map(normalize)),
  }
  const ignoredFiles = new Set(toArray(options.exclude).map(normalize))
  const ignoredDirs = new Set(toArray(options.excludeDir).map(normalize))

  console.log("==> Parsing JSON")
  for (const directory of inputFolders.map(normalize).sort()) {
    console.log(`----> Traversing directory ${directory}`)
    await extractAllFromDir(state, directory, { ignoredFiles, ignoredDirs, tracked })
  }

  console.log("==> Writing POT")
  await Deno.mkdir(dirname(options.output), { recursive: true }).catch(() => undefined)
  await Deno.writeTextFile(
    options.output,
    formatPot(state.entries, state.projectName ?? "Unknown Mod"),
  )
}

if (import.meta.main) {
  await new Command()
    .name("extract_json_strings.ts")
    .description("Extract translatable JSON/JSONC/Lua strings into POT")
    .option("-p, --project <name:string>", "project name and optional version")
    .option("-v, --verbose", "be verbose")
    .option("-i, --input <folder:string>", "input folder", { collect: true })
    .option("-e, --exclude <file:string>", "exclude individual file", { collect: true })
    .option("-E, --exclude-dir <dir:string>", "exclude individual directory", { collect: true })
    .option("--tracked-only", "scan only git tracked files")
    .option("-o, --output <file:string>", "output file", { required: true })
    .option("-s, --suppress <file:string>", "suppress warnings for file", { collect: true })
    .option("--warn-unused-types", "warn about types defined in script but unused in JSON")
    .action(async (options) => {
      try {
        await run(options as CliOptions)
      } catch (error) {
        const message = error instanceof Error ? error.message : String(error)
        console.error(message)
        Deno.exit(1)
      }
    })
    .parse(Deno.args)
}

export { formatPot, injectJsoncTranslatorComments, run }
