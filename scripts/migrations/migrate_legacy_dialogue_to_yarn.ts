#!/usr/bin/env -S deno run --allow-read --allow-write
/**
 * @module
 *
 * Converts legacy JSON `talk_topic` dialogue files into Yarn Spinner `.yarn` files.
 *
 * The script reads files or directories recursively, extracts `talk_topic` entries,
 * and writes one Yarn story per source JSON file while preserving the input path
 * under the selected output directory.
 */

import { Command } from "@cliffy/command"
import { ensureDir, walk } from "@std/fs"
import { dirname, extname, join, relative } from "@std/path"
import * as v from "@valibot/valibot"

type JsonPrimitive = string | number | boolean | null
type JsonObject = { [key: string]: JsonValue }
type JsonValue = JsonPrimitive | JsonObject | JsonValue[]

type ConversionWarning = {
  source: string
  topic?: string
  message: string
}

type ConversionResult = {
  source: string
  yarn: string
  topicCount: number
  warnings: ConversionWarning[]
}

type YarnNode = {
  title: string
  preLines: string[]
  choiceLines: string[]
}

type EffectConversion = {
  lines: string[]
  extraNodes: YarnNode[]
}

const isRecord = (value: unknown): value is JsonObject =>
  typeof value === "object" && value !== null && !Array.isArray(value)

const TalkTopicSchema = v.looseObject({
  id: v.union([v.string(), v.array(v.string())]),
  type: v.literal("talk_topic"),
})

const EntryArraySchema = v.array(v.unknown())

const simpleConditions = new Map<string, string>([
  ["u_male", "u_male"],
  ["u_female", "u_female"],
  ["npc_male", "npc_male"],
  ["npc_female", "npc_female"],
  ["has_no_assigned_mission", "has_no_assigned_mission"],
  ["has_assigned_mission", "has_assigned_mission"],
  ["has_many_assigned_missions", "has_many_assigned_missions"],
  ["has_no_available_mission", "has_no_available_mission"],
  ["has_available_mission", "has_available_mission"],
  ["has_many_available_missions", "has_many_available_missions"],
  ["npc_available", "npc_available"],
  ["npc_service", "npc_available"],
  ["npc_following", "npc_following"],
  ["npc_friend", "npc_friend"],
  ["npc_hostile", "npc_hostile"],
  ["npc_train_skills", "npc_train_skills"],
  ["npc_train_styles", "npc_train_styles"],
  ["at_safe_space", "at_safe_space"],
  ["is_day", "is_day"],
  ["npc_has_activity", "npc_has_activity"],
  ["is_outside", "is_outside"],
  ["u_can_stow_weapon", "u_can_stow_weapon"],
  ["npc_can_stow_weapon", "npc_can_stow_weapon"],
  ["u_has_weapon", "u_has_weapon"],
  ["npc_has_weapon", "npc_has_weapon"],
  ["u_driving", "u_driving"],
  ["npc_driving", "npc_driving"],
  ["has_pickup_list", "has_pickup_list"],
  ["u_has_stolen_item", "u_has_stolen_item"],
  ["mission_complete", "mission_complete"],
  ["mission_incomplete", "mission_incomplete"],
  ["mission_has_generic_rewards", "mission_has_generic_rewards"],
  ["npc_is_riding", "npc_is_riding"],
  ["is_by_radio", "is_by_radio"],
  ["has_reason", "has_reason"],
  ["mission_failed", "mission_failed"],
  ["npc_has_destination", "npc_has_destination"],
  ["asked_for_item", "asked_for_item"],
])

const stringArgConditions = new Map<string, string>([
  ["u_has_trait", "u_has_trait"],
  ["npc_has_trait", "npc_has_trait"],
  ["u_has_trait_flag", "u_has_trait_flag"],
  ["npc_has_trait_flag", "npc_has_trait_flag"],
  ["npc_has_class", "npc_has_class"],
  ["u_has_mission", "u_has_mission"],
  ["u_is_wearing", "u_is_wearing"],
  ["npc_is_wearing", "npc_is_wearing"],
  ["u_has_item", "u_has_item"],
  ["npc_has_item", "npc_has_item"],
  ["u_has_item_category", "u_has_item_category"],
  ["npc_has_item_category", "npc_has_item_category"],
  ["u_has_bionics", "u_has_bionic"],
  ["npc_has_bionics", "npc_has_bionic"],
  ["u_has_effect", "u_has_effect"],
  ["npc_has_effect", "npc_has_effect"],
  ["npc_aim_rule", "npc_aim_rule"],
  ["npc_engagement_rule", "npc_engagement_rule"],
  ["npc_cbm_reserve_rule", "npc_cbm_reserve_rule"],
  ["npc_cbm_recharge_rule", "npc_cbm_recharge_rule"],
  ["npc_rule", "npc_has_rule"],
  ["npc_override", "npc_has_override"],
  ["u_at_om_location", "u_at_om_location"],
  ["npc_at_om_location", "npc_at_om_location"],
  ["npc_role_nearby", "npc_role_nearby"],
  ["is_season", "is_season"],
  ["mission_goal", "mission_goal"],
  ["u_know_recipe", "u_know_recipe"],
])

const numericArgConditions = new Map<string, string>([
  ["u_has_strength", "u_has_strength"],
  ["npc_has_strength", "npc_has_strength"],
  ["u_has_dexterity", "u_has_dexterity"],
  ["npc_has_dexterity", "npc_has_dexterity"],
  ["u_has_intelligence", "u_has_intelligence"],
  ["npc_has_intelligence", "npc_has_intelligence"],
  ["u_has_perception", "u_has_perception"],
  ["npc_has_perception", "npc_has_perception"],
  ["npc_allies", "npc_allies"],
])

const fatigueLevels = new Map<string, number>([
  ["TIRED", 191],
  ["DEAD_TIRED", 383],
  ["EXHAUSTED", 575],
  ["MASSIVE_FATIGUE", 1000],
])

const colors = {
  green: (text: string) => `\x1b[32m${text}\x1b[0m`,
  yellow: (text: string) => `\x1b[33m${text}\x1b[0m`,
  red: (text: string) => `\x1b[31m${text}\x1b[0m`,
}

const quote = (value: string): string => JSON.stringify(value)
const commandArg = (value: string | number | boolean): string =>
  typeof value === "string" ? quote(value) : String(value)
const commandLine = (name: string, args: (string | number | boolean)[] = []): string =>
  `<<${[name, ...args.map(commandArg)].join(" ")}>>`
const ifLine = (expr: string): string => `<<if ${expr}>>`
const detourLine = (topic: string): string => `<<detour ${topic}>>`
const crossDetourLine = (topic: string): string => `<<cross_detour ${topic}>>`
const fnCall = (name: string, args: (string | number | boolean)[] = []): string =>
  `${name}(${args.map(commandArg).join(", ")})`
const parenthesize = (expr: string): string =>
  /^[\w.]+\([^)]*\)$|^true$|^false$|^[\w.]+\(\)\s*(?:[<>!=]=?|==)\s*-?\d+(?:\.\d+)?$/u.test(expr)
    ? expr
    : `(${expr})`
const andExpr = (parts: string[]): string =>
  parts.length === 0 ? "true" : parts.map(parenthesize).join(" and ")
const orExpr = (parts: string[]): string =>
  parts.length === 0 ? "false" : parts.map(parenthesize).join(" or ")
const notExpr = (expr: string): string => `not ${parenthesize(expr)}`

const indentLines = (lines: string[], spaces: number): string[] => {
  const pad = " ".repeat(spaces)
  return lines.map((line) => line.length === 0 ? line : `${pad}${line}`)
}

const compactComment = (value: unknown): string =>
  JSON.stringify(value)?.replaceAll("\n", " ") ?? String(value)

const idsFromTopic = (entry: JsonObject): string[] => {
  const id = entry.id
  if (typeof id === "string") {
    return [id]
  }
  if (Array.isArray(id)) {
    return id.filter((item): item is string => typeof item === "string")
  }
  return []
}

const warning = (
  source: string,
  topic: string | undefined,
  message: string,
): ConversionWarning => ({
  source,
  topic,
  message,
})

class DialogueConverter {
  readonly warnings: ConversionWarning[] = []
  private readonly nodes = new Map<string, YarnNode>()

  constructor(private readonly source: string) {}

  convert(entries: JsonObject[]): ConversionResult {
    const topicParser = v.safeParser(TalkTopicSchema)
    const topics = entries.filter((entry) => topicParser(entry).success)

    for (const entry of topics) {
      for (const id of idsFromTopic(entry)) {
        this.addNode(this.topicToNode(id, entry))
      }
    }

    const yarn = this.render()
    return { source: this.source, yarn, topicCount: topics.length, warnings: this.warnings }
  }

  private addWarning(topic: string | undefined, message: string): void {
    this.warnings.push(warning(this.source, topic, message))
  }

  private addNode(node: YarnNode): void {
    if (!this.nodes.has(node.title)) {
      this.nodes.set(node.title, node)
      return
    }

    const existing = this.nodes.get(node.title)
    if (existing) {
      this.addWarning(node.title, `duplicate topic '${node.title}' merged by appending choices`)
      if (node.preLines.length > 0) {
        const prefix = existing.preLines.length > 0
          ? ["// TODO: merged duplicate legacy pre-choice content follows."]
          : []
        existing.preLines.push(...prefix, ...node.preLines)
      }
      if (node.choiceLines.length > 0) {
        existing.choiceLines.push(
          "// TODO: merged duplicate legacy choices follow.",
          ...node.choiceLines,
        )
      }
    }
  }

  private topicToNode(id: string, entry: JsonObject): YarnNode {
    const preLines: string[] = []
    const choiceLines: string[] = []

    const speakerEffect = entry.speaker_effect
    if (isRecord(speakerEffect)) {
      preLines.push(...this.speakerEffectLines(speakerEffect, id))
    } else if (Array.isArray(speakerEffect)) {
      for (const effect of speakerEffect) {
        if (isRecord(effect)) {
          preLines.push(...this.speakerEffectLines(effect, id))
        }
      }
    }

    if (entry.dynamic_line !== undefined) {
      preLines.push(...this.dynamicLineLines(entry.dynamic_line, id))
    }

    const responses = Array.isArray(entry.responses) ? entry.responses.filter(isRecord) : []
    const repeatResponses = entry.repeat_responses
    if (responses.length > 0 || repeatResponses !== undefined) {
      choiceLines.push(...this.responseLines(responses, id))
      choiceLines.push(...this.repeatResponseLines(repeatResponses, id))
    }

    if (preLines.length === 0 && choiceLines.length === 0) {
      preLines.push("// TODO: empty legacy talk_topic; review before use.")
      preLines.push(commandLine("return"))
    }

    return { title: id, preLines, choiceLines }
  }

  private render(): string {
    const nodes = [...this.nodes.values()]
    const header = [
      `// Generated from ${this.source} by scripts/migrations/migrate_legacy_dialogue_to_yarn.ts.`,
      "// Review TODO comments and unsupported conversion warnings before removing legacy JSON.",
    ]

    if (this.warnings.length > 0) {
      header.push("//")
      for (const item of this.warnings) {
        header.push(`// TODO: ${item.topic ? `${item.topic}: ` : ""}${item.message}`)
      }
    }

    const renderedNodes = nodes.map((node, index) =>
      [
        `title: ${node.title}`,
        `position: ${(index % 8) * 320},${Math.floor(index / 8) * 240}`,
        "---",
        ...node.preLines,
        ...node.choiceLines,
        "===",
      ].join("\n")
    )

    return `${header.join("\n")}\n\n${renderedNodes.join("\n\n")}\n`
  }

  private dynamicLineLines(value: JsonValue | undefined, topic: string): string[] {
    if (typeof value === "string") {
      return this.dialogueLines(value)
    }

    if (Array.isArray(value)) {
      if (value.every((item) => typeof item === "string")) {
        return [
          `NPC: {random_line(${value.map((item) => quote(String(item))).join(", ")})}`,
        ]
      }

      const lines: string[] = []
      for (const item of value) {
        lines.push(...this.dynamicLineLines(item, topic))
      }
      return lines
    }

    if (!isRecord(value)) {
      return []
    }

    if (Array.isArray(value.and)) {
      return value.and.flatMap((item) => this.dynamicLineLines(item, topic))
    }

    if (value.give_hint === true) {
      this.addWarning(topic, "dynamic_line { give_hint: true } has no direct Yarn text equivalent")
      return ["// TODO: legacy dynamic_line give_hint was omitted."]
    }

    if (value.use_reason === true) {
      this.addWarning(topic, "dynamic_line { use_reason: true } has no direct Yarn text equivalent")
      return ["// TODO: legacy dynamic_line use_reason was omitted."]
    }

    if (typeof value.gendered_line === "string") {
      this.addWarning(topic, "gendered_line was converted to its base text only")
      return this.dialogueLines(value.gendered_line)
    }

    const condition = this.conditionFromObject(value, topic)
    const yesSource = value.yes !== undefined ? value.yes : value[condition.yesKey ?? ""]
    const yesLines = this.dynamicLineLines(yesSource, topic)
    const noLines = this.dynamicLineLines(value.no, topic)

    if (yesLines.length > 0 || noLines.length > 0) {
      const lines = [ifLine(condition.expr)]
      lines.push(...indentLines(yesLines, 4))
      if (noLines.length > 0) {
        lines.push(commandLine("else"))
        lines.push(...indentLines(noLines, 4))
      }
      lines.push(commandLine("endif"))
      return lines
    }

    this.addWarning(topic, `unsupported dynamic_line object: ${compactComment(value)}`)
    return [`// TODO: unsupported dynamic_line ${compactComment(value)}`]
  }

  private dialogueLines(text: string): string[] {
    const parts = text.split(/\r?\n/u)
    return parts.map((part, index) => index === 0 ? `NPC: ${part}` : part)
  }

  private responseLines(responses: JsonObject[], topic: string): string[] {
    const lines: string[] = []
    let switchConditions: string[] = []

    for (const response of responses) {
      const isSwitch = response.switch === true
      if (!isSwitch) {
        switchConditions = []
      }

      const choices = this.responseChoices(response, topic)
      const previousSwitch = [...switchConditions]
      if (isSwitch) {
        switchConditions.push(...choices.map((choice) => choice.condition ?? "true"))
      }

      for (const choice of choices) {
        const guards = [...previousSwitch]
        const condition = isSwitch && guards.length > 0
          ? andExpr([notExpr(orExpr(guards)), ...(choice.condition ? [choice.condition] : [])])
          : choice.condition
        lines.push(this.choiceLine(choice.text, condition))
        lines.push(
          ...indentLines(choice.body.length > 0 ? choice.body : [commandLine("nothing")], 4),
        )
      }
    }

    return lines
  }

  private responseChoices(
    response: JsonObject,
    topic: string,
  ): { text: string; condition?: string; body: string[] }[] {
    if (isRecord(response.truefalsetext)) {
      const condition = response.truefalsetext.condition === undefined
        ? undefined
        : this.conditionExpr(response.truefalsetext.condition, topic)
      const body = this.responseBody(response, topic)
      return [
        {
          text: typeof response.truefalsetext.true === "string" ? response.truefalsetext.true : "",
          condition,
          body,
        },
        {
          text: typeof response.truefalsetext.false === "string"
            ? response.truefalsetext.false
            : "",
          condition: condition === undefined ? undefined : notExpr(condition),
          body,
        },
      ]
    }

    const condition = response.condition === undefined
      ? undefined
      : this.conditionExpr(response.condition, topic)
    return [{
      text: typeof response.text === "string" ? response.text : "",
      condition,
      body: this.responseBody(response, topic),
    }]
  }

  private choiceLine(text: string, condition?: string): string {
    const suffix = condition === undefined ? "" : ` <<if ${condition}>>`
    return `-> ${text}${suffix}`
  }

  private responseBody(response: JsonObject, topic: string): string[] {
    if (isRecord(response.trial)) {
      const success = isRecord(response.success)
        ? this.talkEffectLines(response.success, topic).lines
        : []
      const failure = isRecord(response.failure)
        ? this.talkEffectLines(response.failure, topic).lines
        : []
      const trialType = typeof response.trial.type === "string" ? response.trial.type : "NONE"
      const difficulty = typeof response.trial.difficulty === "number"
        ? response.trial.difficulty
        : 0

      if (trialType === "NONE") {
        return success
      }

      const condition = trialType === "CONDITION"
        ? response.trial.condition === undefined
          ? "true"
          : this.conditionExpr(response.trial.condition, topic)
        : fnCall("trial_roll", [trialType, difficulty])
      const lines = [ifLine(condition), ...indentLines(success, 4)]
      if (failure.length > 0) {
        lines.push(commandLine("else"), ...indentLines(failure, 4))
      }
      lines.push(commandLine("endif"))
      return lines
    }

    if (isRecord(response.success)) {
      return this.talkEffectLines(response.success, topic).lines
    }

    return this.talkEffectLines(response, topic).lines
  }

  private repeatResponseLines(value: JsonValue | undefined, topic: string): string[] {
    if (value === undefined) {
      return []
    }

    const groups = Array.isArray(value) ? value.filter(isRecord) : isRecord(value) ? [value] : []
    return groups.flatMap((group) => this.repeatResponseGroupLines(group, topic))
  }

  private repeatResponseGroupLines(group: JsonObject, topic: string): string[] {
    const isNpc = group.is_npc === true
    const itemIds = this.stringList(group.for_item)
    const categories = this.stringList(group.for_category)
    const usesCategory = categories.length > 0
    const command = `${isNpc ? "npc_" : ""}repeat_for_${usesCategory ? "category" : "item"}`
    const args = [...(usesCategory ? categories : itemIds).map(quote)]
    if (group.include_containers === true) {
      args.push("#include_containers")
    }

    const response = isRecord(group.response) ? group.response : undefined
    if (response?.condition !== undefined) {
      args.push("if", this.conditionExpr(response.condition, topic))
    }

    const text = response && typeof response.text === "string" ? response.text : "<topic_item>"
    const body = response ? this.responseBody(response, topic) : [commandLine("return")]
    return [
      `<<${command}${args.length > 0 ? ` ${args.join(" ")}` : ""}>>`,
      `-> ${text}`,
      ...indentLines(body, 4),
      commandLine("endrepeat"),
    ]
  }

  private speakerEffectLines(effect: JsonObject, topic: string): string[] {
    const body = this.effectMemberLines(effect, topic).lines
    if (effect.condition === undefined) {
      return body
    }

    const condition = this.conditionExpr(effect.condition, topic)
    return [
      ifLine(condition),
      ...indentLines(body, 4),
      commandLine("endif"),
    ]
  }

  private talkEffectLines(effect: JsonObject, topic: string): EffectConversion {
    const converted = this.effectMemberLines(effect, topic)
    const lines = [...converted.lines]
    const extraNodes = [...converted.extraNodes]

    if (typeof effect.topic === "string") {
      lines.push(...this.topicTerminalLines(effect.topic))
    } else if (isRecord(effect.topic)) {
      const inlineIds = idsFromTopic(effect.topic)
      for (const id of inlineIds) {
        const node = this.topicToNode(id, effect.topic)
        extraNodes.push(node)
        this.addNode(node)
        lines.push(...this.topicTerminalLines(id))
      }
    }

    return { lines, extraNodes }
  }

  private effectMemberLines(effect: JsonObject, topic: string): EffectConversion {
    const lines: string[] = []
    const extraNodes: YarnNode[] = []

    if (isRecord(effect.opinion)) {
      lines.push(...this.opinionLines(effect.opinion))
    }

    const member = effect.effect
    if (member === undefined) {
      return { lines, extraNodes }
    }

    if (typeof member === "string") {
      lines.push(commandLine(member))
    } else if (isRecord(member)) {
      lines.push(...this.subEffectLines(member, topic))
    } else if (Array.isArray(member)) {
      for (const item of member) {
        if (typeof item === "string") {
          lines.push(commandLine(item))
        } else if (isRecord(item)) {
          lines.push(...this.subEffectLines(item, topic))
        }
      }
    }

    return { lines, extraNodes }
  }

  private subEffectLines(effect: JsonObject, topic: string): string[] {
    const stringCommand = (jsonKey: string, yarnName = jsonKey): string[] =>
      typeof effect[jsonKey] === "string" ? [commandLine(yarnName, [effect[jsonKey]])] : []
    const amountCommand = (jsonKey: string, yarnName = jsonKey): string[] =>
      typeof effect[jsonKey] === "number" ? [commandLine(yarnName, [effect[jsonKey]])] : []
    const duration = this.durationValue(effect.duration)

    for (const key of ["u_add_effect", "npc_add_effect"]) {
      if (typeof effect[key] === "string") {
        return [commandLine(key, [effect[key], duration])]
      }
    }

    for (
      const key of [
        "u_lose_effect",
        "npc_lose_effect",
        "u_add_trait",
        "npc_add_trait",
        "u_lose_trait",
        "npc_lose_trait",
        "u_learn_recipe",
        "u_remove_item_with",
        "npc_remove_item_with",
        "u_set_first_topic",
        "toggle_npc_rule",
        "set_npc_rule",
        "clear_npc_rule",
        "set_npc_engagement_rule",
        "set_npc_aim_rule",
        "set_npc_cbm_reserve_rule",
        "set_npc_cbm_recharge_rule",
        "add_mission",
        "assign_mission",
        "npc_change_faction",
        "npc_change_class",
      ]
    ) {
      const converted = stringCommand(key)
      if (converted.length > 0) {
        return converted
      }
    }

    if (typeof effect.npc_first_topic === "string") {
      return [commandLine("npc_set_first_topic", [effect.npc_first_topic])]
    }

    for (const key of ["u_add_var", "npc_add_var"]) {
      if (typeof effect[key] === "string") {
        return [commandLine(key, [
          effect[key],
          typeof effect.type === "string" ? effect.type : "",
          typeof effect.context === "string" ? effect.context : "",
          typeof effect.value === "string" ? effect.value : "",
        ])]
      }
    }

    for (const key of ["u_lose_var", "npc_lose_var"]) {
      if (typeof effect[key] === "string") {
        return [commandLine(key, [
          effect[key],
          typeof effect.type === "string" ? effect.type : "",
          typeof effect.context === "string" ? effect.context : "",
        ])]
      }
    }

    for (
      const [jsonKey, yarnName] of [["u_adjust_var", "u_adjust_var_legacy"], [
        "npc_adjust_var",
        "npc_adjust_var_legacy",
      ]]
    ) {
      if (typeof effect[jsonKey] === "string") {
        return [commandLine(yarnName, [
          effect[jsonKey],
          typeof effect.type === "string" ? effect.type : "",
          typeof effect.context === "string" ? effect.context : "",
          typeof effect.adjustment === "number" ? effect.adjustment : 0,
        ])]
      }
    }

    const spend = amountCommand("u_spend_ecash")
    if (spend.length > 0) {
      return spend
    }

    for (const key of ["u_buy_item", "u_sell_item"]) {
      if (typeof effect[key] === "string") {
        return [commandLine(key, [
          effect[key],
          typeof effect.cost === "number" ? effect.cost : 0,
          typeof effect.count === "number" ? effect.count : 1,
        ])]
      }
    }

    for (const key of ["u_consume_item", "npc_consume_item"]) {
      if (typeof effect[key] === "string") {
        return [
          commandLine(key, [effect[key], typeof effect.count === "number" ? effect.count : 1]),
        ]
      }
    }

    if (typeof effect.finish_mission === "string") {
      return [commandLine("finish_mission", [effect.finish_mission, effect.success !== false])]
    }

    if (effect.mapgen_update !== undefined) {
      return [commandLine("mapgen_update", this.stringList(effect.mapgen_update))]
    }

    const factionRep = amountCommand("u_faction_rep")
    if (factionRep.length > 0) {
      return factionRep
    }

    if (Array.isArray(effect.add_debt)) {
      const args = effect.add_debt.flatMap((entry) =>
        Array.isArray(entry) && typeof entry[0] === "string" && typeof entry[1] === "number"
          ? [entry[0], entry[1]] as (string | number)[]
          : []
      )
      return [commandLine("add_debt", args)]
    }

    if (isRecord(effect.opinion)) {
      return this.opinionLines(effect.opinion)
    }

    this.addWarning(topic, `unsupported effect object: ${compactComment(effect)}`)
    return [`// TODO: unsupported effect ${compactComment(effect)}`]
  }

  private durationValue(value: JsonValue | undefined): number {
    if (typeof value === "number") {
      return value
    }
    if (typeof value === "string") {
      if (value === "PERMANENT") {
        return -1
      }
      const parsed = Number.parseInt(value, 10)
      return Number.isFinite(parsed) ? parsed : 1
    }
    return 1
  }

  private opinionLines(opinion: JsonObject): string[] {
    const mapping = [
      ["trust", "npc_add_trust"],
      ["fear", "npc_add_fear"],
      ["value", "npc_add_value"],
      ["anger", "npc_add_anger"],
    ] as const
    return mapping.flatMap(([key, command]) =>
      typeof opinion[key] === "number" && opinion[key] !== 0
        ? [commandLine(command, [opinion[key]])]
        : []
    )
  }

  private topicTerminalLines(topic: string): string[] {
    if (topic === "TALK_DONE") {
      return [commandLine("stop")]
    }
    if (topic === "TALK_NONE") {
      return [commandLine("return")]
    }
    if (topic.includes("::")) {
      return [crossDetourLine(topic)]
    }
    return [detourLine(topic)]
  }

  private conditionExpr(value: JsonValue, topic: string): string {
    if (typeof value === "string") {
      const mapped = simpleConditions.get(value)
      if (mapped !== undefined) {
        return fnCall(mapped)
      }
      this.addWarning(topic, `unsupported string condition '${value}' converted to true`)
      return "true"
    }

    if (Array.isArray(value)) {
      return andExpr(value.map((item) => this.conditionExpr(item, topic)))
    }

    if (isRecord(value)) {
      return this.conditionFromObject(value, topic).expr
    }

    this.addWarning(topic, `unsupported condition ${compactComment(value)} converted to true`)
    return "true"
  }

  private conditionFromObject(value: JsonObject, topic: string): { expr: string; yesKey?: string } {
    if (Array.isArray(value.and)) {
      return { expr: andExpr(value.and.map((item) => this.conditionExpr(item, topic))) }
    }
    if (Array.isArray(value.or)) {
      return { expr: orExpr(value.or.map((item) => this.conditionExpr(item, topic))) }
    }
    if (value.not !== undefined) {
      return { expr: notExpr(this.conditionExpr(value.not, topic)) }
    }

    for (const [key, fnName] of stringArgConditions) {
      if (typeof value[key] === "string") {
        return { expr: fnCall(fnName, [value[key]]), yesKey: key }
      }
    }

    for (const [key, fnName] of numericArgConditions) {
      if (typeof value[key] === "number") {
        return { expr: fnCall(fnName, [value[key]]), yesKey: key }
      }
    }

    for (const key of ["mission_complete", "mission_incomplete"]) {
      if (typeof value[key] === "string") {
        return { expr: fnCall(key, [value[key]]), yesKey: key }
      }
    }

    for (const [key, fnName] of simpleConditions) {
      if (value[key] !== undefined) {
        return { expr: fnCall(fnName), yesKey: key }
      }
    }

    for (
      const [key, fnName] of [["u_has_any_trait", "u_has_any_trait"], [
        "npc_has_any_trait",
        "npc_has_any_trait",
      ]]
    ) {
      if (value[key] !== undefined) {
        return { expr: fnCall(fnName, this.stringList(value[key])), yesKey: key }
      }
    }

    for (
      const [key, fnName] of [["u_has_items", "u_has_items"], ["npc_has_items", "npc_has_items"]]
    ) {
      if (isRecord(value[key])) {
        const item = typeof value[key].item === "string" ? value[key].item : ""
        const count = typeof value[key].count === "number" ? value[key].count : 1
        return { expr: fnCall(fnName, [item, count]), yesKey: key }
      }
    }

    for (const [key, fnName] of [["u_need", "u_need"], ["npc_need", "npc_need"]]) {
      if (typeof value[key] === "string") {
        const amount = typeof value.amount === "number"
          ? value.amount
          : typeof value.level === "string"
          ? fatigueLevels.get(value.level) ?? 0
          : 0
        return { expr: fnCall(fnName, [value[key], amount]), yesKey: key }
      }
    }

    if (typeof value.u_has_ecash === "number") {
      return { expr: `${fnCall("u_get_ecash")} >= ${value.u_has_ecash}`, yesKey: "u_has_ecash" }
    }
    if (typeof value.u_are_owed === "number") {
      return { expr: `${fnCall("u_get_owed")} >= ${value.u_are_owed}`, yesKey: "u_are_owed" }
    }
    if (typeof value.days_since_cataclysm === "number") {
      return {
        expr: `${fnCall("days_since_cataclysm")} >= ${value.days_since_cataclysm}`,
        yesKey: "days_since_cataclysm",
      }
    }

    for (
      const [key, fnName] of [["u_has_skill", "u_has_skill"], ["npc_has_skill", "npc_has_skill"]]
    ) {
      if (isRecord(value[key])) {
        const skill = typeof value[key].skill === "string" ? value[key].skill : ""
        const level = typeof value[key].level === "number" ? value[key].level : 1
        return { expr: fnCall(fnName, [skill, level]), yesKey: key }
      }
      if (typeof value[key] === "string") {
        return { expr: fnCall(fnName, [value[key], 1]), yesKey: key }
      }
    }

    for (const [key, fnName] of [["u_has_var", "u_has_var"], ["npc_has_var", "npc_has_var"]]) {
      if (typeof value[key] === "string") {
        return {
          expr: fnCall(fnName, [
            value[key],
            typeof value.type === "string" ? value.type : "",
            typeof value.context === "string" ? value.context : "",
            typeof value.value === "string" ? value.value : "",
          ]),
          yesKey: key,
        }
      }
    }

    for (
      const [key, fnName] of [["u_compare_var", "u_compare_var"], [
        "npc_compare_var",
        "npc_compare_var",
      ]]
    ) {
      if (typeof value[key] === "string") {
        return {
          expr: fnCall(fnName, [
            value[key],
            typeof value.type === "string" ? value.type : "",
            typeof value.context === "string" ? value.context : "",
            typeof value.op === "string" ? value.op : "==",
            typeof value.value === "number" ? value.value : 0,
          ]),
          yesKey: key,
        }
      }
    }

    this.addWarning(
      topic,
      `unsupported condition object ${compactComment(value)} converted to true`,
    )
    return { expr: "true" }
  }

  private stringList(value: JsonValue | undefined): string[] {
    if (typeof value === "string") {
      return [value]
    }
    if (Array.isArray(value)) {
      return value.filter((item): item is string => typeof item === "string")
    }
    return []
  }
}

export const convertEntriesToYarn = (
  entries: JsonObject[],
  source = "<memory>",
): ConversionResult => new DialogueConverter(source).convert(entries)

const parseJsonEntries = (text: string, source: string): JsonObject[] => {
  const data = JSON.parse(text) as unknown
  const parsed = v.safeParse(EntryArraySchema, data)
  if (!parsed.success) {
    throw new Error(`${source}: expected a top-level JSON array`)
  }
  return parsed.output.filter(isRecord)
}

const discoverJsonFiles = async (paths: string[]): Promise<string[]> => {
  const files: string[] = []
  for (const path of paths) {
    const stat = await Deno.stat(path)
    if (stat.isFile) {
      if (extname(path) === ".json") {
        files.push(path)
      }
      continue
    }
    if (stat.isDirectory) {
      for await (const entry of walk(path, { exts: [".json"], includeDirs: false })) {
        if (entry.isFile) {
          files.push(entry.path)
        }
      }
    }
  }
  return files.sort()
}

const outputPathFor = (sourcePath: string, outputDir: string): string => {
  const cwdRelative = relative(Deno.cwd(), sourcePath)
  const safeRelative = cwdRelative.startsWith("..") ? sourcePath.replace(/^\/+/, "") : cwdRelative
  return join(outputDir, safeRelative.replace(/\.json$/u, ".yarn"))
}

const convertFile = async (sourcePath: string): Promise<ConversionResult | undefined> => {
  const text = await Deno.readTextFile(sourcePath)
  const entries = parseJsonEntries(text, sourcePath)
  const result = convertEntriesToYarn(entries, sourcePath)
  return result.topicCount === 0 ? undefined : result
}

const main = () =>
  new Command()
    .name("migrate-legacy-dialogue-to-yarn")
    .version("1.0.0")
    .description("Recursively convert legacy JSON talk_topic dialogue to Yarn Spinner .yarn files.")
    .option("-o, --output-dir <dir:string>", "Directory for generated .yarn files")
    .option("--stdout", "Print a single converted file to stdout instead of writing files")
    .option("-f, --overwrite", "Overwrite existing .yarn files")
    .option("-q, --quiet", "Suppress progress output")
    .arguments("<paths...>")
    .action(
      async (
        { outputDir, stdout = false, overwrite = false, quiet = false },
        ...paths: string[]
      ) => {
        if (!stdout && outputDir === undefined) {
          throw new Error("--output-dir is required unless --stdout is used")
        }
        const destinationDir = outputDir ?? ""

        const files = await discoverJsonFiles(paths)
        const results = (await Promise.all(files.map(convertFile))).filter(
          (result): result is ConversionResult => result !== undefined,
        )

        if (stdout) {
          if (results.length !== 1) {
            throw new Error(
              `--stdout requires exactly one input file with talk_topic data; found ${results.length}`,
            )
          }
          console.log(results[0].yarn)
          return
        }

        for (const result of results) {
          const outPath = outputPathFor(result.source, destinationDir)
          if (!overwrite) {
            try {
              await Deno.lstat(outPath)
              throw new Error(`${outPath} already exists; pass --overwrite to replace it`)
            } catch (error) {
              if (!(error instanceof Deno.errors.NotFound)) {
                throw error
              }
            }
          }
          await ensureDir(dirname(outPath))
          await Deno.writeTextFile(outPath, result.yarn)
          if (!quiet) {
            const warningText = result.warnings.length > 0
              ? colors.yellow(` (${result.warnings.length} warnings)`)
              : ""
            console.log(`${colors.green("wrote")} ${outPath}${warningText}`)
          }
        }

        if (!quiet) {
          const warningCount = results.reduce((sum, result) => sum + result.warnings.length, 0)
          const suffix = warningCount > 0 ? colors.yellow(`, ${warningCount} warnings`) : ""
          console.log(`${colors.green("converted")} ${results.length} file(s)${suffix}`)
        }
      },
    )

if (import.meta.main) {
  try {
    await main().parse(Deno.args)
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error)
    console.error(colors.red(`Error: ${message}`))
    Deno.exit(1)
  }
}
