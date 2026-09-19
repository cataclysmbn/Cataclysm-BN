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
import { dirname, normalize, relative, resolve, SEPARATOR } from "@std/path"
import { parse as parseJsonc } from "@std/jsonc"

import {
  addContext,
  automaticallyConvertible,
  genderOptions,
  gettextFunctions,
  ignorable,
  isArray,
  isObject,
  isString,
  jsonArray,
  jsonObjectEntries,
  jsonStringLiteral,
  logVerbose,
  needsPlural,
  objectArray,
  parseJsonObjects,
  poEscape,
  poLine,
  pythonRepr,
  rawNameList,
  rawTranslationString,
  sourceName,
  toArray,
  useActionMessages,
} from "./extract_json_strings_support.ts"
import type {
  CliOptions,
  ExtractorState,
  JsonObject,
  JsonValue,
  LuaComment,
  PotEntry,
} from "./extract_json_strings_support.ts"

const foundTypes = new Set<string>()
const warnedUnknownTypes = new Set<string>()

const jsonStringPattern = String.raw`"(?:\\.|[^"\\])*"`
const memberStringPattern = new RegExp(
  String.raw`^(\s*)(${jsonStringPattern}\s*:\s*)(${jsonStringPattern})(\s*,?\s*(?://.*)?)$`,
)
const arrayStringPattern = new RegExp(String.raw`^(\s*)(${jsonStringPattern})(\s*,?\s*(?://.*)?)$`)
const translationStringKeys = new Set(["str", "str_sp", "str_pl"])

const injectJsoncTranslatorComments = (raw: string): string => {
  const out: string[] = []
  let pendingComments: string[] = []
  let pendingLineIndices: number[] = []

  const withoutBlockComments = raw.replace(
    /"(?:\\.|[^"\\])*"|\/\/[^\r\n]*|\/\*[\s\S]*?\*\//g,
    (token) => token.startsWith("/*") ? token.replace(/[^\r\n]/g, " ") : token,
  )
  for (const line of withoutBlockComments.match(/^.*(?:\r\n|\n|\r|$)/gm) ?? []) {
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
        const key = JSON.parse(memberMatch[2].match(jsonStringPattern)![0]) as string
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

type WriteOptions = {
  context?: string
  formatStrings?: boolean
  comment?: string
  pluralFormat?: boolean
}

type FieldRule = readonly [key: string, options?: WriteOptions]

const fields = (...keys: string[]): readonly FieldRule[] => keys.map((key) => [key] as const)

const writeString = (
  state: ExtractorState,
  value: JsonValue | undefined,
  options: WriteOptions = {},
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

const writeField = (
  state: ExtractorState,
  item: JsonObject,
  key: string,
  options?: WriteOptions,
): boolean => {
  if (item[key] === undefined) return false
  writeString(state, item[key], options)
  return true
}

const writeFields = (
  state: ExtractorState,
  item: JsonObject,
  rules: readonly FieldRule[],
): boolean => {
  let wrote = false
  for (const [key, options] of rules) wrote = writeField(state, item, key, options) || wrote
  return wrote
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

const extractHarvest = (state: ExtractorState, item: JsonObject) => {
  writeField(state, item, "message")
}

const bodypartFields = fields(
  "name",
  "name_multiple",
  "accusative",
  "accusative_multiple",
  "encumbrance_text",
  "heading",
  "heading_multiple",
  "hp_bar_ui_text",
)

const extractBodypart = (state: ExtractorState, item: JsonObject) => {
  writeFields(state, item, bodypartFields)
}

const extractClothingMod = (state: ExtractorState, item: JsonObject) => {
  writeFields(state, item, fields("implement_prompt", "destroy_prompt"))
}

const extractConstruction = (state: ExtractorState, item: JsonObject) => {
  writeField(state, item, "pre_note")
}

const extractMaterial = (state: ExtractorState, item: JsonObject) => {
  writeString(state, item.name)
  let wrote = false
  wrote = writeFields(state, item, fields("bash_dmg_verb", "cut_dmg_verb")) || wrote
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

const monsterAttackFields = fields("hit_dmg_u", "hit_dmg_npc", "no_dmg_msg_u", "no_dmg_msg_npc")

const extractMonsterAttack = (state: ExtractorState, item: JsonObject) => {
  writeFields(state, item, monsterAttackFields)
}

const extractRecipe = (state: ExtractorState, item: JsonObject) => {
  for (const bookLearn of jsonArray(item.book_learn)) {
    if (isArray(bookLearn) && bookLearn.length >= 3 && rawTranslationString(bookLearn[2])) {
      writeString(state, bookLearn[2])
    }
  }
  writeFields(state, item, fields("description", "blueprint_name"))
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
  writeField(state, response, "text")
  if (isObject(response.truefalsetext)) {
    writeFields(state, response.truefalsetext, fields("true", "false"))
  }
  for (const key of ["success", "failure"] as const) {
    if (isObject(response[key])) extractTalkResponse(state, response[key])
  }
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
  writeFields(state, item, fields("name", "description"))
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

const missionDialogueFields = fields(
  "describe",
  "offer",
  "accepted",
  "rejected",
  "advice",
  "inquire",
  "success",
  "success_lie",
  "failure",
)

const extractMissionDef = (state: ExtractorState, item: JsonObject) => {
  const itemName = item.name
  writeString(state, itemName)
  const itemNameText = pythonRepr(itemName)
  writeField(state, item, "description", { comment: `Description for mission '${itemNameText}'` })
  if (isObject(item.dialogue)) writeFields(state, item.dialogue, missionDialogueFields)
  for (const key of ["start", "end", "fail"] as const) {
    const value = item[key]
    if (isObject(value) && value.effect !== undefined) extractTalkEffects(state, value.effect)
  }
}

const extractMutation = (state: ExtractorState, item: JsonObject) => {
  const itemNameOrId = item.name ?? item.id
  writeField(state, item, "name")
  writeField(state, item, "description", { comment: `Description for ${pythonRepr(itemNameOrId)}` })
  const attacks = item.attacks
  for (
    const attack of isArray(attacks) ? objectArray(attacks) : isObject(attacks) ? [attacks] : []
  ) {
    if (attack.attack_text_u !== undefined) writeString(state, attack.attack_text_u)
    if (attack.attack_text_npc !== undefined) writeString(state, attack.attack_text_npc)
  }
  if (isObject(item.spawn_item)) writeString(state, item.spawn_item.message)
}

const mutationCategoryMessageFields = [
  "mutagen_message",
  "iv_message",
  "iv_sleep_message",
  "iv_sound_message",
  "junkie_message",
]

const extractMutationCategory = (state: ExtractorState, item: JsonObject) => {
  const itemName = item.name
  const itemNameText = pythonRepr(itemName)
  writeString(state, itemName, { comment: "Mutation class name" })
  for (const key of mutationCategoryMessageFields) {
    writeField(state, item, key, { comment: `Mutation class: ${itemNameText} ${key}` })
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
  for (const level of objectArray(item.intensity_levels)) writeField(state, level, "name")
}

const extractTerFurnTransform = (state: ExtractorState, item: JsonObject) => {
  writeField(state, item, "fail_message")
  for (const terrain of objectArray(item.terrain)) writeField(state, terrain, "message")
  for (const furniture of objectArray(item.furniture)) writeField(state, furniture, "message")
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
    writeField(state, item, field, { comment: `${field} for JSON flag '${id}'` })
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
    const path = relative(suppressed, resolve(file))
    if (path === "" || (path !== ".." && !path.startsWith(`..${SEPARATOR}`))) return true
  }
  return false
}

const automaticNameFields = fields("name_suffix", "name_unique", "job_description")
const automaticTextFields = fields("detailed_definition", "sound", "text", "prompt")
const pryFields = fields("sound", "break_sound", "success_message", "fail_message", "break_message")
const cuttingMembers = [
  ["hacksaw", "sound of sawing", "message when finished sawing"],
  ["boltcut", "sound of bolt cutting", "message when finished bolt cutting"],
] as const

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
  if (name === "none") return
  if (name !== undefined) {
    writeString(state, name, { pluralFormat: needsPlural.has(objectType) })
    wrote = true
  }

  wrote = writeFields(state, item, automaticNameFields) || wrote

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

  wrote = writeField(state, item, "description", {
    comment: nameText ? `Description for ${nameText}` : undefined,
  }) || wrote

  wrote = writeFields(state, item, automaticTextFields) || wrote

  wrote = writeField(state, item, "sound_description", {
    comment: `Description for the sound of spell '${commentNameText}'`,
  }) || wrote

  if (isArray(item.snippet_category)) {
    for (const entry of item.snippet_category) {
      writeString(state, isObject(entry) ? entry.text : entry)
      wrote = true
    }
  }

  if (isObject(item.bash)) {
    wrote = writeFields(state, item.bash, fields("sound", "sound_fail")) || wrote
  }

  if (isObject(item.oxytorch)) {
    wrote = writeField(state, item.oxytorch, "message", {
      comment: `message when oxytorch cutting ${commentNameText}`,
    }) || wrote
  }

  for (const [member, soundComment, messageComment] of cuttingMembers) {
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

  if (isObject(item.pry)) wrote = writeFields(state, item.pry, pryFields) || wrote

  wrote = writeField(state, item, "lockpick_message") || wrote

  if (isObject(item.seed_data)) wrote = writeField(state, item.seed_data, "plant_name") || wrote

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

  wrote = writeField(state, item, "message", {
    formatStrings: true,
    comment: `Message for ${objectType} '${commentNameText}'`,
  }) || wrote

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

  wrote = writeField(state, item, "info", {
    comment: "Please leave anything in <angle brackets> unchanged.",
  }) || wrote

  wrote = writeField(state, item, "verb") || wrote

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

  wrote = writeField(state, item, "footsteps") || wrote

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

const parseJsonFile = async (path: string): Promise<unknown> => {
  const raw = await Deno.readTextFile(path)
  return parseJsonc(injectJsoncTranslatorComments(raw))
}

const extractJsonFile = async (state: ExtractorState, path: string) => {
  state.currentSourceFile = sourceName(path)
  logVerbose(state, `Loading ${path}`)
  const data = await parseJsonFile(path)
  for (const entry of parseJsonObjects(data)) extractJsonObject(state, entry)
}

type LuaToken = {
  kind: "identifier" | "string" | "punctuation" | "other"
  value: string
  raw: string
  line: number
}

type LuaScanResult = {
  tokens: LuaToken[]
  comments: LuaComment[]
}

const luaLongDelimiter = (source: string, start: number): number | undefined => {
  if (source[start] !== "[") return undefined
  let index = start + 1
  while (source[index] === "=") index++
  return source[index] === "[" ? index - start - 1 : undefined
}

const luaLongEnd = (source: string, start: number, equals: number): number => {
  const closing = `]${"=".repeat(equals)}]`
  const end = source.indexOf(closing, start)
  return end < 0 ? source.length : end + closing.length
}

const scanLua = (source: string): LuaScanResult => {
  const tokens: LuaToken[] = []
  const comments: LuaComment[] = []
  let index = 0
  let line = 1
  const advance = (raw: string) => {
    line += (raw.match(/\r\n|\r|\n/g) ?? []).length
  }
  const addToken = (kind: LuaToken["kind"], start: number, value: string) => {
    tokens.push({ kind, value, raw: source.slice(start, index), line })
  }

  while (index < source.length) {
    const char = source[index]
    if (/\s/.test(char)) {
      const start = index++
      while (index < source.length && /\s/.test(source[index])) index++
      advance(source.slice(start, index))
      continue
    }
    if (source.startsWith("--", index)) {
      const start = index
      index += 2
      const equals = luaLongDelimiter(source, index)
      if (equals !== undefined) index = luaLongEnd(source, index, equals)
      else while (index < source.length && !"\r\n".includes(source[index])) index++
      const raw = source.slice(start, index)
      const translatorComment = equals === undefined ? raw.match(/^--\s*~(.*)$/) : null
      if (translatorComment) {
        comments.push({
          line,
          text: translatorComment[1].trim(),
          isTranslatorComment: true,
          used: false,
        })
      } else if (equals === undefined) {
        comments.push({ line, text: raw.slice(2).trim(), isTranslatorComment: false, used: false })
      }
      advance(raw)
      continue
    }
    const longEquals = luaLongDelimiter(source, index)
    if (longEquals !== undefined) {
      const start = index
      index = luaLongEnd(source, index, longEquals)
      const raw = source.slice(start, index)
      addToken("string", start, raw)
      advance(raw)
      continue
    }
    if (char === '"' || char === "'") {
      const start = index++
      let escaped = false
      while (index < source.length) {
        const current = source[index++]
        if (escaped) escaped = false
        else if (current === "\\") escaped = true
        else if (current === char) break
      }
      const raw = source.slice(start, index)
      addToken("string", start, raw)
      advance(raw)
      continue
    }
    if (/[A-Za-z_]/.test(char)) {
      const start = index++
      while (index < source.length && /[A-Za-z0-9_]/.test(source[index])) index++
      addToken("identifier", start, source.slice(start, index))
      continue
    }
    const start = index++
    addToken(/[0-9]/.test(char) ? "other" : "punctuation", start, char)
  }
  return { tokens, comments }
}

const decodeLuaString = (literal: string): string => {
  if (literal[0] === "[") {
    const equals = luaLongDelimiter(literal, 0)
    if (equals === undefined) throw new Error("invalid long string")
    return literal.slice(equals + 2, -equals - 2).replace(/\r\n|\n\r|\r/g, "\n").replace(/^\n/, "")
  }
  const encodeBytes = (text: string) =>
    Array.from(new TextEncoder().encode(text), (byte) => String.fromCharCode(byte)).join("")
  const raw = encodeBytes(literal)
  let value = ""
  for (let index = 1; index < raw.length - 1; index++) {
    const char = raw[index]
    if (char !== "\\") {
      value += char
      continue
    }
    const escaped = raw[++index]
    const simpleEscapes: Record<string, string> = {
      a: "\x07",
      b: "\b",
      f: "\f",
      n: "\n",
      r: "\r",
      t: "\t",
      v: "\v",
      "\\": "\\",
      '"': '"',
      "'": "'",
    }
    if (simpleEscapes[escaped] !== undefined) value += simpleEscapes[escaped]
    else if (escaped === "z") { while (/\s/.test(raw[index + 1] ?? "")) index++ }
    else if (escaped === "x") {
      const hex = raw.slice(index + 1, index + 3)
      if (!/^[0-9a-fA-F]{2}$/.test(hex)) throw new Error("invalid hexadecimal escape")
      value += String.fromCodePoint(Number.parseInt(hex, 16))
      index += 2
    } else if (escaped === "u" && raw[index + 1] === "{") {
      const end = raw.indexOf("}", index + 2)
      const codePoint = end < 0 ? "" : raw.slice(index + 2, end)
      if (!/^[0-9a-fA-F]+$/.test(codePoint)) throw new Error("invalid Unicode escape")
      value += encodeBytes(String.fromCodePoint(Number.parseInt(codePoint, 16)))
      index = end
    } else if (/\d/.test(escaped)) {
      const decimal = raw.slice(index, index + 3).match(/^\d{1,3}/)?.[0] ?? ""
      const byte = Number.parseInt(decimal, 10)
      if (byte > 255) throw new Error("decimal escape exceeds one byte")
      value += String.fromCharCode(byte)
      index += decimal.length - 1
    } else if (escaped === "\n") value += "\n"
    else if (escaped === "\r") {
      if (raw[index + 1] === "\n") index++
      value += "\n"
    } else throw new Error(`invalid escape sequence \\\\${escaped}`)
  }
  return new TextDecoder("utf-8", { fatal: true }).decode(
    Uint8Array.from(value, (byte) => byte.charCodeAt(0)),
  )
}

const findLuaTranslatorCommentsBefore = (
  comments: LuaComment[],
  line: number,
): string | undefined => {
  const found: string[] = []
  for (let previousLine = line - 1;; previousLine--) {
    const comment = comments.find((entry) => entry.line === previousLine)
    if (!comment || (!comment.isTranslatorComment && found.length)) break
    if (comment.isTranslatorComment) {
      comment.used = true
      found.unshift(comment.text)
    }
  }
  return found.length ? found.join("\n") : undefined
}

const luaCallArguments = (tokens: LuaToken[], index: number): LuaToken[][] | undefined => {
  if (tokens[index + 1]?.kind === "string") return [[tokens[index + 1]]]
  if (tokens[index + 1]?.value !== "(") return undefined
  const args: LuaToken[][] = []
  const stack = ["("]
  let start = index + 2
  for (let cursor = start; cursor < tokens.length; cursor++) {
    const value = tokens[cursor].value
    if (["(", "[", "{"].includes(value)) stack.push(value)
    else if ([")", "]", "}"].includes(value)) {
      if (stack.pop() !== { ")": "(", "]": "[", "}": "{" }[value]) return undefined
      if (stack.length === 0) {
        if (cursor > start || args.length) args.push(tokens.slice(start, cursor))
        return args
      }
    } else if (value === "," && stack.length === 1) {
      args.push(tokens.slice(start, cursor))
      start = cursor + 1
    }
  }
  return undefined
}

const extractLuaCall = (
  state: ExtractorState,
  tokens: LuaToken[],
  index: number,
  comments: LuaComment[],
) => {
  const token = tokens[index]
  if (!token || token.kind !== "identifier" || !gettextFunctions.has(token.value)) return
  if (tokens[index - 1]?.value === "function") return
  const args = luaCallArguments(tokens, index)
  if (!args) return
  const spec = gettextFunctions.get(token.value)!
  if (args.length !== spec.expected) {
    console.log(
      `WARNING: invalid amount of arguments in translation call (found ${args.length}, expected ${spec.expected})`,
    )
    return
  }
  const offset = spec.context ? 1 : 0
  const literalArguments = [
    spec.context ? 0 : undefined,
    offset,
    spec.plural ? offset + 1 : undefined,
  ]
    .filter((argumentIndex): argumentIndex is number => argumentIndex !== undefined)
  const values = new Map<number, string>()
  try {
    for (const argumentIndex of literalArguments) {
      const argument = args[argumentIndex]
      if (argument.length !== 1 || argument[0].kind !== "string") {
        throw new Error("argument to translation call should be string")
      }
      values.set(argumentIndex, decodeLuaString(argument[0].raw))
    }
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error)
    console.log(`WARNING: ${message}`)
    return
  }
  const context = spec.context ? values.get(0) : undefined
  const comment = findLuaTranslatorCommentsBefore(comments, token.line)
  writeStringBasic(
    state,
    values.get(offset)!,
    spec.plural ? values.get(offset + 1) : undefined,
    context,
    comment,
    true,
  )
}

const extractLuaFile = async (state: ExtractorState, path: string) => {
  state.currentSourceFile = sourceName(path)
  logVerbose(state, `Loading ${path}`)
  const { tokens, comments } = scanLua(await Deno.readTextFile(path))
  for (let index = 0; index < tokens.length; index++) extractLuaCall(state, tokens, index, comments)
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
  return new Set(
    new TextDecoder().decode(stdout).split(/\r?\n/).filter(Boolean).map((path) => resolve(path)),
  )
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
    const normalized = resolve(path)
    if (options.ignoredFiles.has(normalized)) {
      logVerbose(state, `Skipping file (ignored): '${path}'`)
      continue
    }
    if (
      [...options.ignoredDirs].some((dir) => {
        const relativePath = relative(dir, normalized)
        return relativePath === "" ||
          (!relativePath.startsWith(`..${SEPARATOR}`) && relativePath !== "..")
      })
    ) {
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
    suppressWarningForFiles: new Set(toArray(options.suppress).map((path) => resolve(path))),
  }
  const ignoredFiles = new Set(toArray(options.exclude).map((path) => resolve(path)))
  const ignoredDirs = new Set(toArray(options.excludeDir).map((path) => resolve(path)))

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
