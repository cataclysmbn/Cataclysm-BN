import { dirname, fromFileUrl, resolve } from "@std/path"

const SCRIPT_DIR = dirname(fromFileUrl(import.meta.url))
const REPO_ROOT = resolve(SCRIPT_DIR, "..", "..")

const PROMPT_CONFIRM_KEY = "Enter"
const PROMPT_NEXT_TAB_KEY = "Tab"
const PROMPT_PREV_TAB_KEY = "BTab"

export type CaptureEntry = {
  id: string
  caption: string
  text_file: string
  code_block_file: string
  screenshot_file?: string
}

type InputSource = "prompt"
type InputPriority = "normal" | "high"

export type AvailableInput = {
  id: string
  key: string
  source: InputSource
  priority: InputPriority
  label: string
  description: string
}

export type OutputFormat = "json" | "ai"

export type UiMode =
  | "gameplay"
  | "in_game_menu"
  | "overmap"
  | "main_menu"
  | "direction_prompt"
  | "modal_prompt"
  | "loading"
  | "unknown"

const OUTPUT_FORMAT_ALIASES: Record<string, OutputFormat> = {
  json: "json",
  ai: "ai",
  yaml: "ai",
  yml: "ai",
}

export const parseOutputFormat = (value: string | undefined): OutputFormat => {
  const normalized = value?.trim().toLowerCase() ?? "ai"
  if (normalized.length === 0) {
    return "ai"
  }

  const mapped = OUTPUT_FORMAT_ALIASES[normalized]
  if (mapped !== undefined) {
    return mapped
  }

  throw new Error(`invalid output format: ${value}. use one of: json, ai`)
}

export const detectUiMode = (pane: string): UiMode => {
  const lowerPane = pane.toLowerCase()

  if (lowerPane.includes("loading files")) {
    return "loading"
  }

  if (
    (lowerPane.includes("[motd]") && lowerPane.includes("[new game]")) ||
    lowerPane.includes("play now!")
  ) {
    return "main_menu"
  }

  if (lowerPane.includes("main menu")) {
    if (lowerPane.includes("press } to open sidebar options")) {
      return "in_game_menu"
    }
    return "main_menu"
  }

  if (lowerPane.includes("use movement keys to pan") || lowerPane.includes("toggle overlays")) {
    return "overmap"
  }

  if (lowerPane.includes("where? (direction button)")) {
    return "direction_prompt"
  }

  if (
    lowerPane.includes("press [esc]!") ||
    lowerPane.includes("pick wgt") ||
    lowerPane.includes("[backtab] prev") ||
    lowerPane.includes("really step") ||
    (lowerPane.includes("(y)es") && lowerPane.includes("(n)o"))
  ) {
    return "modal_prompt"
  }

  if (
    lowerPane.includes("press } to open sidebar options") ||
    (lowerPane.includes("nw:") && lowerPane.includes("west:") && lowerPane.includes("east:"))
  ) {
    return "gameplay"
  }

  return "unknown"
}

const toYamlScalar = (value: string | number | boolean): string => {
  if (typeof value === "number" || typeof value === "boolean") {
    return `${value}`
  }

  const escaped = value.replaceAll("'", "''")
  return `'${escaped}'`
}

const isSimpleYamlKey = (key: string): boolean => /^[A-Za-z_][A-Za-z0-9_-]*$/.test(key)

const shouldIncludeAiSummary = (): boolean => {
  try {
    return Deno.env.get("CURSES_CLI_AI_INCLUDE_SUMMARY") === "1"
  } catch {
    return false
  }
}

type YamlNode = null | string | number | boolean | YamlNode[] | { [key: string]: YamlNode }

const normalizeForYaml = (value: unknown, visited = new WeakSet<object>()): YamlNode => {
  if (value === null || typeof value === "string" || typeof value === "boolean") {
    return value
  }

  if (typeof value === "number") {
    return Number.isFinite(value) ? value : `${value}`
  }

  if (Array.isArray(value)) {
    return value.map((entry) => normalizeForYaml(entry, visited))
  }

  if (value instanceof Date) {
    return value.toISOString()
  }

  if (typeof value === "object") {
    if (visited.has(value)) {
      return "[Circular]"
    }

    visited.add(value)
    const normalized: { [key: string]: YamlNode } = {}
    for (const [key, entry] of Object.entries(value as Record<string, unknown>)) {
      if (entry === undefined) {
        continue
      }
      normalized[key] = normalizeForYaml(entry, visited)
    }
    visited.delete(value)
    return normalized
  }

  return `${value}`
}

const renderFlowYaml = (value: YamlNode): string => {
  if (value === null || typeof value === "number" || typeof value === "boolean") {
    return `${value}`
  }

  if (typeof value === "string") {
    return toYamlScalar(value)
  }

  if (Array.isArray(value)) {
    const entries = value.map((entry) => renderFlowYaml(entry)).join(",")
    return `[${entries}]`
  }

  const entries = Object.entries(value as { [key: string]: YamlNode }).map(([key, entry]) => {
    const renderedKey = isSimpleYamlKey(key) ? key : toYamlScalar(key)
    return `${renderedKey}:${renderFlowYaml(entry)}`
  }).join(",")
  return `{${entries}}`
}

const toInlineYamlValue = (value: YamlNode, depth: number): string | undefined => {
  if (value === null || typeof value === "number" || typeof value === "boolean") {
    return `${value}`
  }

  if (typeof value === "string") {
    if (value.includes("\n")) {
      return undefined
    }
    return toYamlScalar(value)
  }

  if (depth >= 2) {
    return renderFlowYaml(value)
  }

  return undefined
}

const renderYamlValue = (value: YamlNode, indent: number, depth: number): string[] => {
  const padding = " ".repeat(indent)
  const inline = toInlineYamlValue(value, depth)
  if (inline !== undefined) {
    return [`${padding}${inline}`]
  }

  if (typeof value === "string") {
    const lines = value.split("\n")
    return [
      `${padding}|-`,
      ...lines.map((line) => `${padding}  ${line}`),
    ]
  }

  if (Array.isArray(value)) {
    if (value.length === 0) {
      return [`${padding}[]`]
    }

    const lines: string[] = []
    for (const entry of value) {
      const entryInline = toInlineYamlValue(entry, depth + 1)
      if (entryInline !== undefined) {
        lines.push(`${padding}- ${entryInline}`)
        continue
      }

      lines.push(`${padding}-`)
      lines.push(...renderYamlValue(entry, indent + 2, depth + 1))
    }
    return lines
  }

  const entries = Object.entries(value as { [key: string]: YamlNode })
  if (entries.length === 0) {
    return [`${padding}{}`]
  }

  const lines: string[] = []
  for (const [key, entry] of entries) {
    const entryInline = toInlineYamlValue(entry, depth + 1)
    const yamlKey = isSimpleYamlKey(key) ? key : toYamlScalar(key)
    if (entryInline !== undefined) {
      lines.push(`${padding}${yamlKey}: ${entryInline}`)
      continue
    }

    lines.push(`${padding}${yamlKey}:`)
    lines.push(...renderYamlValue(entry, indent + 2, depth + 1))
  }
  return lines
}

type RenderCommandOutputOptions = {
  format: OutputFormat
  command: string
  payload: unknown
  summary?: Record<string, unknown>
  screen?: string
}

export const renderCommandOutput = (options: RenderCommandOutputOptions): string => {
  if (options.format === "json") {
    return JSON.stringify(options.payload, null, 2)
  }

  const payload = normalizeForYaml(options.payload)
  const lines = [
    `command: ${toYamlScalar(options.command)}`,
    "payload:",
    ...renderYamlValue(payload, 2, 1),
  ]

  const includeSummary = shouldIncludeAiSummary()
  if (includeSummary) {
    const summary = normalizeForYaml(options.summary ?? {})
    lines.splice(1, 0, "summary:", ...renderYamlValue(summary, 2, 1))
  }

  if (options.screen !== undefined) {
    lines.push("screen:")
    lines.push(...renderYamlValue(normalizeForYaml(options.screen), 2, 1))
  }

  return lines.join("\n")
}

export const timestamp = (): string =>
  new Date().toISOString().replaceAll(":", "").replaceAll(".", "-")

/** Produces `yyyy-mm-dd_hh-mm-ss` for cast filenames. */
export const sessionTimestamp = (): string => {
  const d = new Date()
  const pad = (n: number) => String(n).padStart(2, "0")
  const date = [d.getFullYear(), pad(d.getMonth() + 1), pad(d.getDate())].join("-")
  const time = [pad(d.getHours()), pad(d.getMinutes()), pad(d.getSeconds())].join("-")
  return `${date}_${time}`
}

const shellEscape = (value: string): string => `'${value.replaceAll("'", "'\\''")}'`

export const sanitizeId = (value: string): string =>
  value.replaceAll(/[^a-zA-Z0-9_-]/g, "-").replaceAll(/-+/g, "-")

export const resolveInputKey = (inputId: string): string => {
  const normalized = inputId.trim()
  if (normalized.length === 0) {
    throw new Error("input id cannot be empty")
  }

  if (normalized.startsWith("key:")) {
    const key = normalized.slice("key:".length).trim()
    if (key.length === 0) {
      throw new Error("key input id cannot be empty")
    }
    return key
  }

  return normalized
}

const dedupeInputs = (inputs: AvailableInput[]): AvailableInput[] => {
  const byId = new Map<string, AvailableInput>()
  for (const input of inputs) {
    if (!byId.has(input.id)) {
      byId.set(input.id, input)
    }
  }
  return [...byId.values()]
}

const normalizePromptKey = (raw: string): string | undefined => {
  const token = raw.trim().replaceAll("`", "")
  if (token.length === 0) {
    return undefined
  }

  if (token === "'" || token === '"\'"') {
    return "'"
  }

  const upper = token.toUpperCase()
  if (upper === "TAB") {
    return "Tab"
  }
  if (upper === "BACKTAB") {
    return "BTab"
  }
  if (upper === "SPACE") {
    return "Space"
  }
  if (upper === "ENTER") {
    return "Enter"
  }
  if (token.length === 1) {
    return token
  }

  return undefined
}

const toPromptLabel = (text: string): string =>
  text
    .trim()
    .toLowerCase()
    .replaceAll(/[^a-z0-9]+/g, "_")
    .replaceAll(/^_+|_+$/g, "")

const detectInlinePromptChoices = (pane: string): AvailableInput[] => {
  const detected: AvailableInput[] = []

  for (const match of pane.matchAll(/\(([A-Za-z0-9!?'./;,])\)\s*([A-Za-z][A-Za-z0-9_-]*)/g)) {
    const matchIndex = match.index ?? -1
    if (matchIndex > 0) {
      const previousChar = pane[matchIndex - 1]
      if (/[A-Za-z0-9_]/.test(previousChar)) {
        continue
      }
    }

    const promptKey = normalizePromptKey(match[1])
    if (promptKey === undefined) {
      continue
    }

    const actionText = match[2]
    const label = toPromptLabel(actionText)
    detected.push({
      id: `key:${promptKey}`,
      key: promptKey,
      source: "prompt",
      priority: "high",
      label: label.length > 0 ? label : `key_${promptKey}`,
      description: `Prompt option: ${actionText}`,
    })
  }

  for (
    const match of pane.matchAll(
      /(?:Press|press)\s+([^\s]+)\s+to\s+([^,.\n\r)]+)/g,
    )
  ) {
    const promptKey = normalizePromptKey(match[1])
    if (promptKey === undefined) {
      continue
    }

    const actionText = match[2].trim()
    if (actionText.toLowerCase().includes("open sidebar options")) {
      continue
    }
    const label = toPromptLabel(actionText)
    detected.push({
      id: `key:${promptKey}`,
      key: promptKey,
      source: "prompt",
      priority: "high",
      label: label.length > 0 ? label : `key_${promptKey}`,
      description: `Prompt option: ${actionText}`,
    })
  }

  return dedupeInputs(detected)
}

export const detectPromptInputs = (pane: string): AvailableInput[] => {
  const detected: AvailableInput[] = [...detectInlinePromptChoices(pane)]
  // --- Press any key / space bar prompts ---
  if (pane.includes("Press any key") || pane.includes("space bar")) {
    detected.push({
      id: "key:Space",
      key: "Space",
      source: "prompt",
      priority: "high",
      label: "dismiss_prompt",
      description: "Advance a Press any key style prompt",
    })
  }

  // --- Safe mode prompt ---
  if (pane.includes("safe mode is on") || pane.includes("Press ! to turn it off")) {
    detected.push({
      id: "key:!",
      key: "!",
      source: "prompt",
      priority: "high",
      label: "disable_safe_mode",
      description: "Disable safe mode warning stops",
    })
    detected.push({
      id: "key:'",
      key: "'",
      source: "prompt",
      priority: "high",
      label: "ignore_current_monster",
      description: "Ignore current spotted monster and continue",
    })
  }

  // --- Generic yes/no confirmation prompts ---
  // Covers: "Are you SURE you're finished?", "Really step into ...?",
  // "Really walk into ...?", "Wield the ..?" and other Y/N query_yn prompts
  if (
    pane.includes("Are you SURE you're finished?") ||
    /Really (step|walk|drag|drive|dismount|fly|swim|climb|go)/.test(pane) ||
    /Wield the .+\?/.test(pane) ||
    /Consume the .+\?/.test(pane) ||
    /Drink the .+\?/.test(pane) ||
    /Start butchering/.test(pane) ||
    /You see .*\. Continue\?/.test(pane) ||
    /You hear .*\. Continue\?/.test(pane)
  ) {
    detected.push({
      id: "key:Y",
      key: "Y",
      source: "prompt",
      priority: "high",
      label: "answer_yes",
      description: "Confirm yes/no prompt",
    })
    detected.push({
      id: "key:N",
      key: "N",
      source: "prompt",
      priority: "normal",
      label: "answer_no",
      description: "Decline yes/no prompt",
    })
  }

  // --- Character creation finish prompt ---
  if (pane.includes("Press TAB to finish character creation or BACKTAB to go back.")) {
    detected.push({
      id: `key:${PROMPT_NEXT_TAB_KEY}`,
      key: PROMPT_NEXT_TAB_KEY,
      source: "prompt",
      priority: "high",
      label: "finish_character_creation",
      description: "Move to finish character creation",
    })
    detected.push({
      id: `key:${PROMPT_PREV_TAB_KEY}`,
      key: PROMPT_PREV_TAB_KEY,
      source: "prompt",
      priority: "normal",
      label: "go_back_character_creation",
      description: "Go back in character creation tabs",
    })
  }

  // --- Inventory item selection / letter prompts ---
  // When the game shows an inventory list for wield/eat/wear/drop/read/apply,
  // items are listed as " a - rock" or " b - pipe" with letter keys a-z, A-Z
  if (
    pane.includes("Wield what?") ||
    pane.includes("Wear what?") ||
    pane.includes("Eat what?") ||
    pane.includes("Read what?") ||
    pane.includes("Drop what?") ||
    pane.includes("Apply what?") ||
    pane.includes("Consume item:") ||
    pane.includes("Use item:") ||
    pane.includes("Compare:") ||
    pane.includes("Choose an item") ||
    pane.includes("Select an item") ||
    /Get items from where\?/.test(pane)
  ) {
    // Extract letter-keyed items like "a - rock", "B - pipe"
    for (const match of pane.matchAll(/^\s*([a-zA-Z])\s+-\s+(.+?)\s*$/gm)) {
      const letterKey = match[1]
      const itemName = match[2].trim()
      detected.push({
        id: `key:${letterKey}`,
        key: letterKey,
        source: "prompt",
        priority: "high",
        label: `select_item_${letterKey}`,
        description: `Select: ${itemName}`,
      })
    }
  }

  // --- Crafting menu detection ---
  // The crafting menu shows recipe categories and filter prompt
  if (
    pane.includes("Craft:") || pane.includes("Your crafting inventory") ||
    pane.includes("Recipe search")
  ) {
    detected.push({
      id: "key:/",
      key: "/",
      source: "prompt",
      priority: "high",
      label: "filter_recipes",
      description: "Type to filter/search recipes in crafting menu",
    })
    detected.push({
      id: `key:${PROMPT_NEXT_TAB_KEY}`,
      key: PROMPT_NEXT_TAB_KEY,
      source: "prompt",
      priority: "high",
      label: "next_craft_category",
      description: "Switch to next crafting category tab",
    })
    detected.push({
      id: `key:${PROMPT_PREV_TAB_KEY}`,
      key: PROMPT_PREV_TAB_KEY,
      source: "prompt",
      priority: "normal",
      label: "prev_craft_category",
      description: "Switch to previous crafting category tab",
    })
    detected.push({
      id: `key:${PROMPT_CONFIRM_KEY}`,
      key: PROMPT_CONFIRM_KEY,
      source: "prompt",
      priority: "high",
      label: "select_recipe",
      description: "Select the highlighted recipe to craft",
    })
  }

  // --- Quantity / number input prompts ---
  // "How many? (0-10)" or "Craft how many?" or "Set value (0-100):"
  if (/[Hh]ow many|[Ss]et value|[Ee]nter a number|[Qq]uantity/.test(pane)) {
    detected.push({
      id: `key:${PROMPT_CONFIRM_KEY}`,
      key: PROMPT_CONFIRM_KEY,
      source: "prompt",
      priority: "high",
      label: "confirm_quantity",
      description: "Confirm the entered quantity/number",
    })
  }

  // --- Pickup prompt (multi-item) ---
  // When 'g' is pressed with multiple items, shows pickup list
  if (pane.includes("PICK UP") || pane.includes("Pickup:")) {
    detected.push({
      id: `key:${PROMPT_CONFIRM_KEY}`,
      key: PROMPT_CONFIRM_KEY,
      source: "prompt",
      priority: "high",
      label: "confirm_pickup",
      description: "Confirm item pickup selection",
    })
  }

  // --- Butcher / disassemble menu ---
  if (pane.includes("Choose corpse to butcher") || pane.includes("item to disassemble")) {
    detected.push({
      id: `key:${PROMPT_CONFIRM_KEY}`,
      key: PROMPT_CONFIRM_KEY,
      source: "prompt",
      priority: "high",
      label: "confirm_butcher_selection",
      description: "Confirm butcher/disassemble selection",
    })
  }

  // --- World building / loading screen ---
  if (pane.includes("Please wait as we build your world") || pane.includes("Loading the save")) {
    detected.push({
      id: "key:Space",
      key: "Space",
      source: "prompt",
      priority: "normal",
      label: "wait_loading",
      description: "Game is loading \u2014 wait for completion",
    })
  }

  // --- Main menu detection ---
  if (pane.includes("MOTD") && (pane.includes("New Game") || pane.includes("Load"))) {
    detected.push({
      id: "key:Down",
      key: "Down",
      source: "prompt",
      priority: "high",
      label: "navigate_main_menu",
      description: "Navigate main menu options",
    })
    detected.push({
      id: `key:${PROMPT_CONFIRM_KEY}`,
      key: PROMPT_CONFIRM_KEY,
      source: "prompt",
      priority: "high",
      label: "select_main_menu",
      description: "Select highlighted main menu option",
    })
  }
  return dedupeInputs(detected)
}

export const listAvailableInputs = (
  pane: string,
  _includeCatalog = true,
): AvailableInput[] => {
  return detectPromptInputs(pane)
}

type BuildLaunchCommandOptions = {
  binPath: string
  userdir: string
  world: string
  seed?: string
  availableKeysJson?: string
  availableMacrosJson?: string
  aiHelperOutput?: string
}

export const buildLaunchCommand = (options: BuildLaunchCommandOptions): string => {
  const seed = options.seed ?? ""
  const availableKeysJson = options.availableKeysJson ?? ""
  const availableMacrosJson = options.availableMacrosJson ?? ""
  const aiHelperOutput = options.aiHelperOutput ?? ""
  const parts = [
    "TERM=xterm-256color",
    "COLORTERM=truecolor",
    "BROWSER=true",
    "CATA_DISABLE_OPEN_URL=1",
    ...(availableKeysJson.length > 0
      ? [`CATA_AVAILABLE_KEYS_JSON=${shellEscape(availableKeysJson)}`]
      : []),
    ...(availableMacrosJson.length > 0
      ? [`CATA_AVAILABLE_MACROS_JSON=${shellEscape(availableMacrosJson)}`]
      : []),
    ...(aiHelperOutput.length > 0 ? [`AI_HELPER_OUTPUT=${shellEscape(aiHelperOutput)}`] : []),
    shellEscape(options.binPath),
    "--basepath",
    shellEscape(REPO_ROOT),
    "--userdir",
    shellEscape(options.userdir),
    "--no-blinking",
  ]

  if (options.world.length > 0) {
    parts.push("--world", shellEscape(options.world))
  }

  if (seed.length > 0) {
    parts.push("--seed", shellEscape(seed))
  }

  return parts.join(" ")
}

export const writeCodeBlockCapture = async (
  outputPath: string,
  textContent: string,
): Promise<void> => {
  const body = ["```text", textContent, "```", ""].join("\n")
  await Deno.writeTextFile(outputPath, body)
}

type WriteIndexOptions = {
  outputPath: string
  sessionName: string
  captures: CaptureEntry[]
  castFile?: string
}

export const writeIndex = async (options: WriteIndexOptions): Promise<void> => {
  const lines = [
    "# PR Verify Artifact",
    "",
    `Session: ${options.sessionName}`,
    "",
    "## Captures",
    "",
    ...options.captures.map((capture, index) => {
      const title = capture.caption.length > 0 ? capture.caption : capture.id
      const screenshotSuffix = capture.screenshot_file
        ? `, screenshot: \`${capture.screenshot_file}\``
        : ""
      return `${
        index + 1
      }. ${title} - text: \`${capture.text_file}\`, code block: \`${capture.code_block_file}\`${screenshotSuffix}`
    }),
    "",
  ]

  if (options.castFile !== undefined) {
    lines.push(`Cast recording: \`${options.castFile}\``)
    lines.push("")
  }

  await Deno.writeTextFile(options.outputPath, lines.join("\n"))
}
