#!/usr/bin/env -S deno run --allow-run --allow-read --allow-write

import { ensureDir } from "@std/fs"
import { basename, dirname, fromFileUrl, join, relative, resolve } from "@std/path"
import { Command } from "@cliffy/command"
import {
  buildLaunchCommand,
  DEFAULT_BENCH_DELAY_MS,
  detectUiMode,
  listAvailableInputs,
  parseOutputFormat,
  renderCommandOutput,
  resolveInputKey,
  sanitizeId,
  sessionTimestamp,
  timestamp,
  writeCodeBlockCapture,
  writeIndex,
} from "./common.ts"
import type { AvailableInput, CaptureEntry, OutputFormat, UiMode } from "./common.ts"
import {
  configureTmuxBinary,
  configureTmuxSocket,
  ensureTmuxAvailable,
  TmuxSession,
} from "./tmux.ts"
import type { MouseButton, MouseEventType } from "./tmux.ts"

const SCRIPT_DIR = dirname(fromFileUrl(import.meta.url))
const REPO_ROOT = resolve(SCRIPT_DIR, "..", "..")
const TMP_CLI_ROOT = resolve("/tmp", "curses-cli")
const DEFAULT_STATE_FILE = resolve(TMP_CLI_ROOT, "state", ".live-session.json")
const DEFAULT_OUTPUT_FORMAT: OutputFormat = parseOutputFormat(
  Deno.env.get("CURSES_CLI_OUTPUT_FORMAT"),
)
const SINGLETON_SESSION_NAME = "curses-cli"
const SINGLETON_TMUX_SOCKET = "curses-cli"

type SessionState = {
  id: string
  artifact_dir: string
  captures_dir: string
  session_name: string
  userdir: string
  bin_path: string
  world: string
  seed?: string
  available_keys_json?: string
  available_macros_json?: string
  tmux_bin: string
  tmux_socket?: string
  width: number
  height: number
  magick_bin: string
  render_webp: boolean
  webp_quality: number
  record_cast: boolean
  asciinema_bin: string
  cast_idle_time_limit?: number
  cast_file?: string
  cast_pid?: number
  cast_log_file?: string
  status: "running" | "stopped"
  started_at: string
  finished_at?: string
  captures: CaptureEntry[]
}

const delay = (ms: number): Promise<void> =>
  new Promise((resolveDelay) => setTimeout(resolveDelay, ms))

const ANSI_ESCAPE_PATTERN = new RegExp("\\u001B(?:\\[[0-?]*[ -/]*[@-~]|[@-Z\\\\-_])", "g")

const stripAnsiFromPane = (pane: string): string => pane.replaceAll(ANSI_ESCAPE_PATTERN, "")

const displayPath = (absolutePath: string): string =>
  absolutePath.startsWith(`${REPO_ROOT}/`) ? relative(REPO_ROOT, absolutePath) : absolutePath

const shellEscape = (value: string): string => `'${value.replaceAll("'", "'\\''")}'`

const isBinaryAvailable = async (
  binary: string,
  versionArgs: string[] = ["-version"],
): Promise<boolean> => {
  try {
    const output = await new Deno.Command(binary, {
      args: versionArgs,
      stdout: "null",
      stderr: "null",
    }).output()
    return output.success
  } catch {
    return false
  }
}

const ensureLaunchBinaryExists = async (binPath: string): Promise<void> => {
  try {
    const stat = await Deno.stat(binPath)
    if (!stat.isFile) {
      throw new Error(`game binary path is not a file: ${binPath}`)
    }
  } catch (error) {
    if (error instanceof Deno.errors.NotFound) {
      throw new Error(`game binary not found: ${binPath}`)
    }
    throw error
  }
}

type StartDetachedCastProcessOptions = {
  asciinemaBin: string
  castArgs: string[]
  castLogFile: string
}

const startDetachedCastProcess = async (
  options: StartDetachedCastProcessOptions,
): Promise<number> => {
  const escapedArgs = [shellEscape(options.asciinemaBin), ...options.castArgs.map(shellEscape)]
    .join(" ")
  const launchScript = `nohup env -u TMUX ${escapedArgs} >${
    shellEscape(options.castLogFile)
  } 2>&1 < /dev/null & echo $!`
  const output = await new Deno.Command("bash", {
    args: ["-lc", launchScript],
    cwd: REPO_ROOT,
    stdout: "piped",
    stderr: "piped",
  }).output()

  const stderr = new TextDecoder().decode(output.stderr).trim()
  const stdout = new TextDecoder().decode(output.stdout).trim()
  if (!output.success) {
    throw new Error(
      stderr.length > 0
        ? `failed to start detached cast recorder: ${stderr}`
        : "failed to start detached cast recorder",
    )
  }

  const pid = Number.parseInt(stdout.split("\n").at(-1)?.trim() ?? "", 10)
  if (!Number.isFinite(pid) || pid <= 0) {
    const details = stderr.length > 0 ? stderr : stdout
    throw new Error(`failed to parse cast recorder pid: ${details}`)
  }

  return pid
}

const getFileSize = async (filePath: string): Promise<number> => {
  try {
    const stat = await Deno.stat(filePath)
    return stat.size
  } catch {
    return 0
  }
}

const waitForFileData = async (filePath: string, timeoutMs: number): Promise<boolean> => {
  const startedAt = Date.now()
  while (Date.now() - startedAt <= timeoutMs) {
    if (await getFileSize(filePath) > 0) {
      return true
    }
    await delay(200)
  }
  return false
}

const getCastStats = async (
  castFile: string | undefined,
): Promise<{ bytes: number; lines: number; has_events: boolean } | undefined> => {
  if (castFile === undefined) {
    return undefined
  }

  try {
    const content = await Deno.readTextFile(castFile)
    const lines = content.length === 0 ? 0 : content.split("\n").length
    const bytes = new TextEncoder().encode(content).length
    return {
      bytes,
      lines,
      has_events: lines > 1,
    }
  } catch {
    return undefined
  }
}

const copyFileIntoArtifactDir = async (
  sourcePath: string | undefined,
  artifactDir: string,
): Promise<string | undefined> => {
  if (sourcePath === undefined || sourcePath.length === 0) {
    return sourcePath
  }

  const destinationDir = join(artifactDir, "casts")
  await ensureDir(destinationDir)
  const destinationPath = join(destinationDir, basename(sourcePath))
  await Deno.copyFile(sourcePath, destinationPath)
  return destinationPath
}

type AvailableKeysSnapshot = {
  actions?: Array<{
    id: string
    name: string
    description?: string
    key: string
    requires_coordinate?: boolean
    mouse_capable?: boolean
  }>
}

type AvailableMacrosSnapshot = {
  macros?: Array<{
    id: string
    name: string
    description: string
    category: string
    hotkey: string
  }>
}

type SnapshotCache<TSnapshot> = {
  path?: string
  mtimeMs?: number
  value?: TSnapshot
}

const availableKeysSnapshotCache: SnapshotCache<AvailableKeysSnapshot> = {}
const availableMacrosSnapshotCache: SnapshotCache<AvailableMacrosSnapshot> = {}

const cacheMtimeMs = (mtime: Date | null): number => mtime?.getTime() ?? -1

const loadCachedSnapshot = async <TSnapshot>(options: {
  path: string | undefined
  cache: SnapshotCache<TSnapshot>
}): Promise<TSnapshot | undefined> => {
  if (options.path === undefined || options.path.length === 0) {
    return undefined
  }

  try {
    const stat = await Deno.stat(options.path)
    const mtimeMs = cacheMtimeMs(stat.mtime)
    if (
      options.cache.path === options.path && options.cache.mtimeMs === mtimeMs &&
      options.cache.value !== undefined
    ) {
      return options.cache.value
    }

    const content = await Deno.readTextFile(options.path)
    const parsed = JSON.parse(content) as TSnapshot
    options.cache.path = options.path
    options.cache.mtimeMs = mtimeMs
    options.cache.value = parsed
    return parsed
  } catch {
    return undefined
  }
}

type JsonRecord = Record<string, unknown>

const asRecord = (value: unknown): JsonRecord | undefined => {
  if (typeof value !== "object" || value === null || Array.isArray(value)) {
    return undefined
  }
  return value as JsonRecord
}

const trimMiddle = (text: string, maxChars: number): string => {
  if (text.length <= maxChars) {
    return text
  }
  const headLength = Math.max(0, Math.floor((maxChars - 3) / 2))
  const tailLength = Math.max(0, maxChars - 3 - headLength)
  return `${text.slice(0, headLength)}...${text.slice(text.length - tailLength)}`
}

type StateDumpBuildOptions = {
  paneMaxChars: number
  inputLimit: number
  actionLimit: number
  macroLimit: number
}

const summarizeAiState = (gameState: unknown): JsonRecord | null => {
  const gameStateRecord = asRecord(gameState)
  if (gameStateRecord === undefined) {
    return null
  }

  const keys = [
    "turn",
    "safe_mode",
    "hostile_visible",
    "has_destination",
    "player_pos",
    "is_sleepy",
    "is_hungry",
    "is_thirsty",
    "pain",
    "stamina",
    "focus",
  ]
  const summary: JsonRecord = {}
  for (const key of keys) {
    if (key in gameStateRecord) {
      summary[key] = gameStateRecord[key]
    }
  }

  return Object.keys(summary).length > 0 ? summary : gameStateRecord
}

const detectStopReasonCandidate = (
  aiSummary: JsonRecord | null,
  pane: string,
): string | undefined => {
  if (aiSummary !== null) {
    if (aiSummary.safe_mode === true) {
      return "safe_mode_interrupt"
    }
    if (aiSummary.has_destination === true) {
      return "travel_in_progress"
    }
    if (aiSummary.hostile_visible === true) {
      return "hostile_visible"
    }
  }

  const lowerPane = pane.toLowerCase()
  if (lowerPane.includes("safe mode")) {
    return "safe_mode_interrupt"
  }
  if (lowerPane.includes("really step") || lowerPane.includes("continue anyway")) {
    return "confirm_prompt"
  }
  if (lowerPane.includes("where to") || lowerPane.includes("examine where")) {
    return "mode_trap"
  }

  const modeTrapMarkers = [
    "look around",
    "examine",
    "peek around",
    "target",
    "line of fire",
    "fire mode",
    "debug menu",
    "lua console",
  ]
  if (modeTrapMarkers.some((marker) => lowerPane.includes(marker))) {
    return "mode_trap"
  }

  return undefined
}

const summarizeActionKeys = (
  snapshot: AvailableKeysSnapshot | undefined,
  limit: number,
): Array<JsonRecord> =>
  (snapshot?.actions ?? []).slice(0, Math.max(1, limit)).map((action) => ({
    id: action.id,
    key: action.key,
    name: action.name,
  }))

const summarizeMacroIds = (
  snapshot: AvailableMacrosSnapshot | undefined,
  limit: number,
): string[] => (snapshot?.macros ?? []).slice(0, Math.max(1, limit)).map((macro) => macro.id)

type BuildStateDumpPayloadOptions = {
  state: SessionState
  pane: string
  promptInputs: AvailableInput[]
  keysSnapshot: AvailableKeysSnapshot | undefined
  macrosSnapshot: AvailableMacrosSnapshot | undefined
  gameState: unknown
  build: StateDumpBuildOptions
}

const dedupeStringList = (values: string[]): string[] => [...new Set(values)]

const toSendTokenFromKey = (key: string): string => {
  if ([...key].length === 1) {
    return key
  }
  return `key:${key}`
}

const listAvailableInputTokens = (
  promptInputs: AvailableInput[],
  limit: number,
): string[] => {
  const available = promptInputs
    .map((input) => toSendTokenFromKey(input.key))

  return dedupeStringList(available).slice(0, Math.max(1, limit))
}

const buildStateDumpPayload = (
  options: BuildStateDumpPayloadOptions,
): JsonRecord => {
  const aiStateSummary = summarizeAiState(options.gameState)
  const stopReasonCandidate = detectStopReasonCandidate(aiStateSummary, options.pane)
  return {
    state_id: options.state.id,
    status: options.state.status,
    started_at: options.state.started_at,
    pane_excerpt: trimMiddle(options.pane, options.build.paneMaxChars),
    available_inputs: listAvailableInputTokens(options.promptInputs, options.build.inputLimit),
    action_keys_top: summarizeActionKeys(options.keysSnapshot, options.build.actionLimit),
    macro_ids: summarizeMacroIds(options.macrosSnapshot, options.build.macroLimit),
    ai_state_summary: aiStateSummary,
    stop_reason_candidate: stopReasonCandidate,
  }
}

type CompactStateDumpOptions = {
  state: SessionState
  pane: string
  promptInputs: AvailableInput[]
  keysSnapshot: AvailableKeysSnapshot | undefined
  macrosSnapshot: AvailableMacrosSnapshot | undefined
  gameState: unknown
  maxChars: number
}

const compactStateDump = (
  options: CompactStateDumpOptions,
): JsonRecord => {
  const maxChars = Math.max(1200, options.maxChars)
  let build: StateDumpBuildOptions = {
    paneMaxChars: Math.min(2400, Math.floor(maxChars * 0.45)),
    inputLimit: 12,
    actionLimit: 12,
    macroLimit: 12,
  }

  for (let pass = 0; pass < 4; pass += 1) {
    const payload = buildStateDumpPayload({
      state: options.state,
      pane: options.pane,
      promptInputs: options.promptInputs,
      keysSnapshot: options.keysSnapshot,
      macrosSnapshot: options.macrosSnapshot,
      gameState: options.gameState,
      build,
    })
    if (JSON.stringify(payload).length <= maxChars) {
      return payload
    }

    build = {
      paneMaxChars: Math.max(250, Math.floor(build.paneMaxChars * 0.65)),
      inputLimit: Math.max(4, Math.floor(build.inputLimit * 0.65)),
      actionLimit: Math.max(4, Math.floor(build.actionLimit * 0.65)),
      macroLimit: Math.max(4, Math.floor(build.macroLimit * 0.65)),
    }
  }

  const aiStateSummary = summarizeAiState(options.gameState)
  const stopReasonCandidate = detectStopReasonCandidate(aiStateSummary, options.pane)
  return {
    state_id: options.state.id,
    status: options.state.status,
    available_inputs: listAvailableInputTokens(options.promptInputs, 8),
    ai_state_summary: aiStateSummary,
    stop_reason_candidate: stopReasonCandidate,
  }
}

const loadAvailableKeysSnapshot = async (
  state: SessionState,
): Promise<AvailableKeysSnapshot | undefined> => {
  return await loadCachedSnapshot({
    path: state.available_keys_json,
    cache: availableKeysSnapshotCache,
  })
}

const loadAvailableMacrosSnapshot = async (
  state: SessionState,
): Promise<AvailableMacrosSnapshot | undefined> => {
  return await loadCachedSnapshot({
    path: state.available_macros_json,
    cache: availableMacrosSnapshotCache,
  })
}

type ResolvedSessionInput =
  | {
    kind: "key"
    keys: string[]
  }
  | {
    kind: "mouse"
    x: number
    y: number
    button: MouseButton
    event: MouseEventType
  }

type CoordinateOptions = {
  col?: number
  row?: number
}

const COMPACT_KEY_ALIASES: Record<string, string> = {
  enter: "Enter",
  esc: "Escape",
  escape: "Escape",
  tab: "Tab",
  btab: "BTab",
  up: "Up",
  down: "Down",
  left: "Left",
  right: "Right",
  home: "Home",
  end: "End",
  pgup: "PageUp",
  pgdn: "PageDown",
  space: "Space",
}

const UNICODE_ARROW_KEY_ALIASES: Record<string, string> = {
  "↑": "Up",
  "↓": "Down",
  "←": "Left",
  "→": "Right",
}

const parseCompactInputId = (inputId: string): string => {
  const normalized = inputId.trim()
  const unicodeArrowAlias = UNICODE_ARROW_KEY_ALIASES[normalized]
  if (unicodeArrowAlias !== undefined) {
    return `key:${unicodeArrowAlias}`
  }

  const usesSlashPrefix = normalized.startsWith("/") && normalized.length > 1
  if (!usesSlashPrefix) {
    return normalized
  }

  if (normalized === "//") {
    return "/"
  }

  if (normalized.startsWith("action:")) {
    const actionId = normalized.slice("action:".length)
    if (actionId.length === 0) {
      throw new Error("action input id cannot be empty")
    }
    return `engine_action:${actionId}`
  }

  const compactToken = normalized.slice(1)
  const keyAlias = COMPACT_KEY_ALIASES[compactToken.toLowerCase()]
  if (keyAlias !== undefined) {
    return `key:${keyAlias}`
  }

  return `engine_action:${compactToken}`
}

const isPromptInputUnavailableError = (error: unknown): boolean =>
  error instanceof Error &&
  error.message.includes("input is not available in current prompt context")

type ResolveSessionInputWithRefreshOptions = ResolveSessionInputOptions & {
  session: TmuxSession
  paneLines: number
}

const resolveSessionInputWithRefresh = async (
  options: ResolveSessionInputWithRefreshOptions,
): Promise<{ resolvedInput: ResolvedSessionInput; promptInputs: AvailableInput[] }> => {
  const {
    session,
    paneLines,
    ...resolveOptions
  } = options
  const currentPromptInputs = resolveOptions.promptInputs ?? []

  try {
    return {
      resolvedInput: await resolveSessionInput({
        ...resolveOptions,
        promptInputs: currentPromptInputs,
      }),
      promptInputs: currentPromptInputs,
    }
  } catch (error) {
    const strictPromptInputs = resolveOptions.strictPromptInputs ?? true
    if (!strictPromptInputs || !isPromptInputUnavailableError(error)) {
      throw error
    }

    const refreshedPane = stripAnsiFromPane(await session.capturePane(paneLines))
    const refreshedPromptInputs = listAvailableInputs(refreshedPane)
    return {
      resolvedInput: await resolveSessionInput({
        ...resolveOptions,
        promptInputs: refreshedPromptInputs,
      }),
      promptInputs: refreshedPromptInputs,
    }
  }
}

const requireCoordinate = (
  coordinate: CoordinateOptions,
): {
  col: number
  row: number
} => {
  if (coordinate.col === undefined || coordinate.row === undefined) {
    throw new Error("this input requires --col and --row")
  }

  const col = Math.floor(coordinate.col)
  const row = Math.floor(coordinate.row)
  if (col < 1 || row < 1) {
    throw new Error("--col and --row must be >= 1")
  }

  return { col, row }
}

type ResolveSessionInputOptions = {
  state: SessionState
  inputId: string
  strictPromptInputs?: boolean
  promptInputs?: AvailableInput[]
  coordinate?: CoordinateOptions
  availableKeysSnapshot?: AvailableKeysSnapshot
  availableMacrosSnapshot?: AvailableMacrosSnapshot
}

const toPromptKeyId = (normalizedInputId: string): string => {
  if (normalizedInputId.startsWith("key:")) {
    return normalizedInputId
  }

  if (normalizedInputId.length === 1) {
    return `key:${normalizedInputId}`
  }

  return `key:${resolveInputKey(normalizedInputId)}`
}

const rejectVerboseSingleCharacterKeyId = (normalizedInputId: string): void => {
  if (!normalizedInputId.startsWith("key:")) {
    return
  }

  const resolvedKey = resolveInputKey(normalizedInputId)
  if (resolvedKey === "/") {
    return
  }
  if ([...resolvedKey].length === 1) {
    throw new Error(
      `single-character key ids must be raw keys: ${normalizedInputId}. use '${resolvedKey}' instead`,
    )
  }
}

const ensurePromptInputAllowed = (
  normalizedInputId: string,
  promptInputs: AvailableInput[],
): void => {
  const availablePromptInputIds = new Set(promptInputs.map((input) => input.id))
  if (availablePromptInputIds.size === 0) {
    return
  }

  const requestedPromptId = toPromptKeyId(normalizedInputId)
  if (availablePromptInputIds.has(requestedPromptId)) {
    return
  }

  const suggestions = [...availablePromptInputIds].slice(0, 8)
  throw new Error(
    `input is not available in current prompt context: ${normalizedInputId}. ` +
      `use one of: ${suggestions.join(", ")}`,
  )
}

const resolveSessionInput = async (
  options: ResolveSessionInputOptions,
): Promise<ResolvedSessionInput> => {
  const normalized = parseCompactInputId(options.inputId)
  rejectVerboseSingleCharacterKeyId(normalized)
  const coordinate = options.coordinate ?? {}
  const strictPromptInputs = options.strictPromptInputs ?? true
  const promptInputs = options.promptInputs ?? []

  if (normalized === "mouse:waypoint") {
    const target = requireCoordinate(coordinate)
    return {
      kind: "mouse",
      x: target.col,
      y: target.row,
      button: "left",
      event: "click",
    }
  }

  if (!normalized.startsWith("engine_action:")) {
    if (strictPromptInputs) {
      ensurePromptInputAllowed(normalized, promptInputs)
    }

    if (normalized.startsWith("macro:")) {
      const macroId = normalized.slice("macro:".length)
      if (macroId.length === 0) {
        throw new Error("macro input id cannot be empty")
      }

      const macrosSnapshot = options.availableMacrosSnapshot ??
        await loadAvailableMacrosSnapshot(options.state)
      const macro = macrosSnapshot?.macros?.find((entry) => entry.id === macroId)
      if (macro === undefined) {
        throw new Error(`macro is not available in current context: ${macroId}`)
      }

      return {
        kind: "key",
        keys: [`/m ${macroId}`, "Enter"],
      }
    }

    return {
      kind: "key",
      keys: [resolveInputKey(normalized)],
    }
  }

  const actionId = normalized.slice("engine_action:".length)
  if (actionId.length === 0) {
    throw new Error("engine_action input id cannot be empty")
  }

  const snapshot = options.availableKeysSnapshot ?? await loadAvailableKeysSnapshot(options.state)
  const actions = snapshot?.actions ?? []
  const matched = actions.find((action) => action.id === actionId)

  if (matched === undefined) {
    throw new Error(`engine action not available in current context: ${actionId}`)
  }

  if (matched.requires_coordinate || actionId === "COORDINATE") {
    const target = requireCoordinate(coordinate)
    return {
      kind: "mouse",
      x: target.col,
      y: target.row,
      button: matched.key === "MOUSE_RIGHT" ? "right" : "left",
      event: "click",
    }
  }

  if (matched.key.startsWith("Unbound")) {
    throw new Error(`engine action is not bound in current context: ${actionId}`)
  }

  if (matched.key === "MOUSE_LEFT" || matched.key === "MOUSE_RIGHT") {
    const target = requireCoordinate(coordinate)
    return {
      kind: "mouse",
      x: target.col,
      y: target.row,
      button: matched.key === "MOUSE_RIGHT" ? "right" : "left",
      event: "click",
    }
  }

  return {
    kind: "key",
    keys: [resolveInputKey(`key:${matched.key}`)],
  }
}

type RenderTextCaptureWebpOptions = {
  magickBin: string
  textPath: string
  screenshotPath: string
  quality: number
}

const renderTextCaptureWebp = async (options: RenderTextCaptureWebpOptions): Promise<void> => {
  const output = await new Deno.Command(options.magickBin, {
    args: [
      "-background",
      "#101010",
      "-fill",
      "#f5f5f5",
      "-pointsize",
      "16",
      "-interline-spacing",
      "2",
      `label:@${options.textPath}`,
      "-strip",
      "-colorspace",
      "sRGB",
      "-quality",
      `${options.quality}`,
      "-define",
      "webp:method=6",
      "-define",
      "webp:sns-strength=0",
      "-define",
      "webp:filter-strength=0",
      "-define",
      "webp:preprocessing=0",
      "-define",
      "webp:segments=2",
      "-define",
      "webp:exact=true",
      options.screenshotPath,
    ],
    stdout: "piped",
    stderr: "piped",
  }).output()

  if (!output.success) {
    const decoder = new TextDecoder()
    const stderr = decoder.decode(output.stderr).trim()
    throw new Error(
      stderr.length > 0
        ? `failed to render webp screenshot with ${options.magickBin}: ${stderr}`
        : `failed to render webp screenshot with ${options.magickBin}`,
    )
  }
}

const loadState = async (stateFile: string): Promise<SessionState> => {
  const content = await Deno.readTextFile(stateFile)
  return JSON.parse(content) as SessionState
}

const saveState = async (stateFile: string, state: SessionState): Promise<void> => {
  await ensureDir(dirname(stateFile))
  await Deno.writeTextFile(stateFile, JSON.stringify(state, null, 2))
}

const requireRunningState = async (stateFile: string): Promise<SessionState> => {
  const state = await loadState(stateFile)
  if (state.status !== "running") {
    throw new Error(`session is not running in ${stateFile}`)
  }
  return state
}

const stopRunningStateSession = async (
  stateFilePath: string,
  fallbackTmuxBin: string,
): Promise<void> => {
  let state: SessionState | undefined
  try {
    state = await loadState(stateFilePath)
  } catch {
    return
  }

  if (state.status !== "running") {
    return
  }

  const tmuxBin = state.tmux_bin.length > 0 ? state.tmux_bin : fallbackTmuxBin
  try {
    configureTmuxBinary(tmuxBin)
    configureTmuxSocket(normalizeTmuxSocket(state.tmux_socket))
    await stopCastProcess(state.cast_pid, state.cast_file)
    await TmuxSession.killByName(state.session_name, REPO_ROOT)
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error)
    console.error(`warning: failed stopping previous session from ${stateFilePath}: ${message}`)
  }

  state.status = "stopped"
  state.finished_at = new Date().toISOString()
  await saveState(stateFilePath, state)
}

const ensureSingleSessionPerMachine = async (fallbackTmuxBin: string): Promise<void> => {
  const stateDir = resolve(TMP_CLI_ROOT, "state")
  await ensureDir(stateDir)
  for await (const entry of Deno.readDir(stateDir)) {
    if (!entry.isFile || !entry.name.endsWith(".json")) {
      continue
    }

    await stopRunningStateSession(join(stateDir, entry.name), fallbackTmuxBin)
  }
}

const normalizeTmuxSocket = (socketName: string | undefined): string =>
  socketName !== undefined && socketName.length > 0 ? socketName : SINGLETON_TMUX_SOCKET

type CaptureIntoArtifactsOptions = {
  state: SessionState
  session: TmuxSession
  id: string
  caption: string
  lines: number
}

const captureIntoArtifacts = async (
  options: CaptureIntoArtifactsOptions,
): Promise<CaptureEntry> => {
  const captureNumber = options.state.captures.length + 1
  const fileName = `${String(captureNumber).padStart(2, "0")}-${sanitizeId(options.id)}.txt`
  const absPath = join(options.state.captures_dir, fileName)
  const content = stripAnsiFromPane(await options.session.capturePane(options.lines))
  await Deno.writeTextFile(absPath, content)

  const codeBlockFileName = fileName.replace(/\.txt$/, ".md")
  const absCodeBlockPath = join(options.state.captures_dir, codeBlockFileName)
  await writeCodeBlockCapture(absCodeBlockPath, content)

  let screenshotPath: string | undefined
  if (options.state.render_webp && await isBinaryAvailable(options.state.magick_bin)) {
    const screenshotName = fileName.replace(/\.txt$/, ".webp")
    const absScreenshotPath = join(options.state.captures_dir, screenshotName)
    await renderTextCaptureWebp({
      magickBin: options.state.magick_bin,
      textPath: absPath,
      screenshotPath: absScreenshotPath,
      quality: options.state.webp_quality,
    })
    screenshotPath = displayPath(absScreenshotPath)
  }

  return {
    id: options.id,
    caption: options.caption,
    text_file: displayPath(absPath),
    code_block_file: displayPath(absCodeBlockPath),
    screenshot_file: screenshotPath,
  }
}

const parseInputIdsJson = (input: string): string[] => {
  const parsed = JSON.parse(input)
  if (!Array.isArray(parsed) || parsed.some((entry) => typeof entry !== "string")) {
    throw new Error("--ids-json must be a JSON array of strings")
  }
  if (parsed.length === 0) {
    throw new Error("--ids-json must include at least one input id")
  }
  return parsed
}

const resolveStateFilePath = (stateFile: string): string => resolve(REPO_ROOT, stateFile)

const withRunningSession = async (
  stateFile: string,
  handler: (state: SessionState, session: TmuxSession) => Promise<void>,
): Promise<void> => {
  const state = await requireRunningState(resolveStateFilePath(stateFile))
  configureTmuxBinary(state.tmux_bin)
  configureTmuxSocket(normalizeTmuxSocket(state.tmux_socket))
  const session = await TmuxSession.attach(state.session_name, REPO_ROOT)
  await handler(state, session)
}

class PendingTmuxSessionResource {
  #tmuxBin: string
  #tmuxSocket: string | undefined
  #sessionName: string
  #cwd: string
  #completed = false

  constructor(
    options: { tmuxBin: string; tmuxSocket: string | undefined; sessionName: string; cwd: string },
  ) {
    this.#tmuxBin = options.tmuxBin
    this.#tmuxSocket = options.tmuxSocket
    this.#sessionName = options.sessionName
    this.#cwd = options.cwd
  }

  get sessionName(): string {
    return this.#sessionName
  }

  markCompleted(): void {
    this.#completed = true
  }

  async [Symbol.asyncDispose](): Promise<void> {
    if (this.#completed) {
      return
    }

    configureTmuxBinary(this.#tmuxBin)
    configureTmuxSocket(this.#tmuxSocket)
    await TmuxSession.killByName(this.#sessionName, this.#cwd)
  }
}

type CreateTmuxSessionOptions = {
  tmuxBin: string
  tmuxSocket: string | undefined
  session: {
    name: string
    command: string
    cwd: string
    width: number
    height: number
  }
}

const createTmuxSession = async (
  options: CreateTmuxSessionOptions,
): Promise<PendingTmuxSessionResource> => {
  configureTmuxBinary(options.tmuxBin)
  configureTmuxSocket(options.tmuxSocket)
  await TmuxSession.start(options.session)

  try {
    const attached = await TmuxSession.attach(options.session.name, options.session.cwd)
    await attached.capturePane(1)
    await delay(150)
    const hasSession = await TmuxSession.hasSession(options.session.name, options.session.cwd)
    if (!hasSession) {
      throw new Error(
        `tmux session exited immediately: ${options.session.name}. ` +
          "verify --bin points to a runnable cataclysm-bn curses binary.",
      )
    }
  } catch (error) {
    try {
      await TmuxSession.killByName(options.session.name, options.session.cwd)
    } catch {
    }
    throw error
  }

  return new PendingTmuxSessionResource({
    tmuxBin: options.tmuxBin,
    tmuxSocket: options.tmuxSocket,
    sessionName: options.session.name,
    cwd: options.session.cwd,
  })
}

type SendWaypointClicksOptions = {
  session: TmuxSession
  col: number
  row: number
  delayMs: number
  clicks: number
}

const sendWaypointClicks = async (
  options: SendWaypointClicksOptions,
): Promise<{ clicks_sent: number }> => {
  const clickCount = Math.max(1, options.clicks)
  for (let index = 0; index < clickCount; index += 1) {
    await options.session.sendMouse({
      x: options.col,
      y: options.row,
      button: "left",
      event: "click",
    })
    if (options.delayMs > 0 && index + 1 < clickCount) {
      await delay(options.delayMs)
    }
  }

  return {
    clicks_sent: clickCount,
  }
}

const captureScreenExcerpt = async (
  session: TmuxSession,
  lines = 160,
  maxChars = 3200,
): Promise<string> => {
  const pane = stripAnsiFromPane(await session.capturePane(lines))
  return trimMiddle(pane, maxChars)
}

type ExpectedUiMode = "any" | "gameplay"

const parseExpectedUiMode = (value: string | undefined): ExpectedUiMode => {
  const normalized = value?.trim().toLowerCase() ?? "any"
  if (normalized === "" || normalized === "any") {
    return "any"
  }
  if (normalized === "gameplay") {
    return "gameplay"
  }

  throw new Error(`invalid --expect-mode: ${value}. use one of: any, gameplay`)
}

type GameplayRecoveryStep = {
  step: number
  mode: UiMode
  action: "wait" | "escape" | "cancel"
}

type GameplayRecoveryResult = {
  initial_mode: UiMode
  final_mode: UiMode
  recovered: boolean
  steps: GameplayRecoveryStep[]
  screen: string
}

const recoverGameplayMode = async (
  session: TmuxSession,
  maxSteps: number,
): Promise<GameplayRecoveryResult> => {
  const boundedSteps = Math.max(1, Math.min(20, Math.floor(maxSteps)))
  const steps: GameplayRecoveryStep[] = []
  let initialMode: UiMode = "unknown"
  let finalMode: UiMode = "unknown"
  let latestScreen = ""
  let consecutiveUnknown = 0

  for (let step = 1; step <= boundedSteps; step += 1) {
    const pane = stripAnsiFromPane(await session.capturePane(220))
    const mode = detectUiMode(pane)
    latestScreen = trimMiddle(pane, 3200)
    if (step === 1) {
      initialMode = mode
    }
    finalMode = mode

    if (mode === "gameplay") {
      return {
        initial_mode: initialMode,
        final_mode: finalMode,
        recovered: true,
        steps,
        screen: latestScreen,
      }
    }

    if (mode === "loading") {
      consecutiveUnknown = 0
      steps.push({ step, mode, action: "wait" })
      await delay(150)
      continue
    }

    if (mode === "main_menu") {
      consecutiveUnknown = 0
      steps.push({ step, mode, action: "wait" })
      await delay(100)
      continue
    }

    if (mode === "in_game_menu") {
      consecutiveUnknown = 0
      steps.push({ step, mode, action: "escape" })
      await session.sendKeys(["Escape"])
      await delay(80)
      continue
    }

    if (mode === "direction_prompt") {
      consecutiveUnknown = 0
      steps.push({ step, mode, action: "cancel" })
      await session.sendKeys(["."])
      await delay(80)
      continue
    }

    if (mode === "unknown") {
      consecutiveUnknown += 1
      if (consecutiveUnknown <= 8) {
        steps.push({ step, mode, action: "wait" })
        await delay(120)
        continue
      }
    } else {
      consecutiveUnknown = 0
    }

    steps.push({ step, mode, action: "escape" })
    await session.sendKeys(["Escape"])
    await delay(80)
  }

  return {
    initial_mode: initialMode,
    final_mode: finalMode,
    recovered: false,
    steps,
    screen: latestScreen,
  }
}

type EnsureExpectedModeOptions = {
  session: TmuxSession
  expectedMode: ExpectedUiMode
  autoRecover: boolean
  recoverMaxSteps: number
}

type EnsureExpectedModeResult = {
  mode: UiMode
  recovered: boolean
  recovery_steps: GameplayRecoveryStep[]
  screen: string
}

const ensureExpectedMode = async (
  options: EnsureExpectedModeOptions,
): Promise<EnsureExpectedModeResult> => {
  const pane = stripAnsiFromPane(await options.session.capturePane(220))
  const mode = detectUiMode(pane)
  const screen = trimMiddle(pane, 3200)

  if (options.expectedMode === "any" || mode === options.expectedMode) {
    return { mode, recovered: false, recovery_steps: [], screen }
  }

  if (!options.autoRecover || options.expectedMode !== "gameplay") {
    throw new Error(
      `unexpected ui mode: ${mode}. expected ${options.expectedMode}. use recover-ui or set --auto-recover-mode true`,
    )
  }

  const recovery = await recoverGameplayMode(options.session, options.recoverMaxSteps)
  if (!recovery.recovered) {
    throw new Error(
      `failed to recover ui mode to gameplay. initial=${recovery.initial_mode}, final=${recovery.final_mode}`,
    )
  }

  return {
    mode: recovery.final_mode,
    recovered: true,
    recovery_steps: recovery.steps,
    screen: recovery.screen,
  }
}

const stopCastProcess = async (
  castPid: number | undefined,
  castFile: string | undefined,
): Promise<void> => {
  if (castPid === undefined) {
    return
  }

  if (castFile !== undefined) {
    await waitForFileData(castFile, 1_500)
  }

  try {
    Deno.kill(castPid, "SIGTERM")
  } catch (_error) {
    void _error
  }
  await delay(500)

  if (castFile !== undefined) {
    await waitForFileData(castFile, 4_000)
  }
}

class StartRollbackResource {
  #tmuxBin: string
  #tmuxSocket: string | undefined
  #sessionName: string | undefined
  #castPid: number | undefined
  #castFile: string | undefined
  #completed = false

  constructor(tmuxBin: string, tmuxSocket: string | undefined) {
    this.#tmuxBin = tmuxBin
    this.#tmuxSocket = tmuxSocket
  }

  setSession(sessionName: string): void {
    this.#sessionName = sessionName
  }

  setCast(castPid: number, castFile: string): void {
    this.#castPid = castPid
    this.#castFile = castFile
  }

  markCompleted(): void {
    this.#completed = true
  }

  async [Symbol.asyncDispose](): Promise<void> {
    if (this.#completed) {
      return
    }

    await stopCastProcess(this.#castPid, this.#castFile)

    if (this.#sessionName !== undefined) {
      configureTmuxBinary(this.#tmuxBin)
      configureTmuxSocket(this.#tmuxSocket)
      await TmuxSession.killByName(this.#sessionName, REPO_ROOT)
    }
  }
}

class StopCleanupResource {
  #tmuxBin: string
  #tmuxSocket: string | undefined
  #sessionName: string
  #castPid: number | undefined
  #castFile: string | undefined

  constructor(options: {
    tmuxBin: string
    tmuxSocket: string | undefined
    sessionName: string
    castPid: number | undefined
    castFile: string | undefined
  }) {
    this.#tmuxBin = options.tmuxBin
    this.#tmuxSocket = options.tmuxSocket
    this.#sessionName = options.sessionName
    this.#castPid = options.castPid
    this.#castFile = options.castFile
  }

  async [Symbol.asyncDispose](): Promise<void> {
    await stopCastProcess(this.#castPid, this.#castFile)
    configureTmuxBinary(this.#tmuxBin)
    configureTmuxSocket(this.#tmuxSocket)
    await TmuxSession.killByName(this.#sessionName, REPO_ROOT)
  }
}

if (import.meta.main) {
  await new Command()
    .name("pr-verify-curses-cli")
    .description("Playwright-like command-driven CLI for tmux curses verification")
    .command(
      "actions",
      new Command()
        .description("Print deprecation notice for removed static catalog")
        .option("--output-format <value:string>", "Output format: json|ai", {
          default: DEFAULT_OUTPUT_FORMAT,
        })
        .action((options) => {
          const outputFormat = parseOutputFormat(options.outputFormat)
          const payload = {
            deprecated: true,
            message:
              "Static key catalog was removed. Use 'inputs-jsonl' or 'available-keys-json' for runtime keys.",
          }
          console.log(renderCommandOutput({
            format: outputFormat,
            command: "actions",
            payload,
            summary: { deprecated: true },
          }))
        }),
    )
    .command(
      "inputs-jsonl",
      new Command()
        .description("Emit available input list as JSONL from current pane")
        .option("--state-file <path:string>", "State file path", { default: DEFAULT_STATE_FILE })
        .option("--lines <value:number>", "Pane lines to inspect", { default: 200 })
        .action(async (options) => {
          await withRunningSession(options.stateFile, async (state, session) => {
            const pane = stripAnsiFromPane(await session.capturePane(options.lines))
            const inputs = listAvailableInputs(pane)
            const engineSnapshot = await loadAvailableKeysSnapshot(state)
            const macrosSnapshot = await loadAvailableMacrosSnapshot(state)

            if (engineSnapshot?.actions !== undefined) {
              const hasCoordinateAction = engineSnapshot.actions.some((action) =>
                action.id === "COORDINATE"
              )
              if (hasCoordinateAction) {
                console.log(JSON.stringify({
                  id: "mouse:waypoint",
                  key: "MOUSE_LEFT@<col,row>",
                  source: "mouse_waypoint",
                  priority: "high",
                  label: "Point-and-click move",
                  description:
                    "Use send-inputs --ids-json '[\"mouse:waypoint\"]' --col <x> --row <y> to click a tile waypoint.",
                  requires_coordinate: true,
                  mouse_capable: true,
                }))
              }

              for (const action of engineSnapshot.actions) {
                console.log(JSON.stringify({
                  id: `engine_action:${action.id}`,
                  key: action.key,
                  source: "engine_json",
                  priority: "high",
                  label: action.name,
                  description: action.id,
                  requires_coordinate: action.requires_coordinate ?? false,
                  mouse_capable: action.mouse_capable ?? false,
                }))
              }
            }

            if (macrosSnapshot?.macros !== undefined) {
              for (const macro of macrosSnapshot.macros) {
                console.log(JSON.stringify({
                  id: `macro:${macro.id}`,
                  key: `/m ${macro.id}`,
                  source: "macro_json",
                  priority: "high",
                  label: macro.name,
                  description: macro.description,
                  category: macro.category,
                  hotkey: macro.hotkey,
                }))
              }
            }

            for (const input of inputs) {
              console.log(JSON.stringify({
                key: input.key,
                source: input.source,
                priority: input.priority,
                label: input.label,
                description: input.description,
              }))
            }
          })
        }),
    )
    .command(
      "available-keys-json",
      new Command()
        .description("Read currently available keybindings JSON from active game context")
        .option("--state-file <path:string>", "State file path", { default: DEFAULT_STATE_FILE })
        .option("--output-format <value:string>", "Output format: json|ai", {
          default: DEFAULT_OUTPUT_FORMAT,
        })
        .action(async (options) => {
          const outputFormat = parseOutputFormat(options.outputFormat)
          const state = await requireRunningState(resolveStateFilePath(options.stateFile))
          const snapshot = await loadAvailableKeysSnapshot(state)
          const payload = snapshot ?? { actions: [] }
          const summary = { actions_count: payload.actions?.length ?? 0 }
          console.log(renderCommandOutput({
            format: outputFormat,
            command: "available-keys-json",
            payload,
            summary,
          }))
        }),
    )
    .command(
      "available-macros-json",
      new Command()
        .description("Read currently available Lua action menu macros JSON")
        .option("--state-file <path:string>", "State file path", { default: DEFAULT_STATE_FILE })
        .option("--output-format <value:string>", "Output format: json|ai", {
          default: DEFAULT_OUTPUT_FORMAT,
        })
        .action(async (options) => {
          const outputFormat = parseOutputFormat(options.outputFormat)
          const state = await requireRunningState(resolveStateFilePath(options.stateFile))
          const snapshot = await loadAvailableMacrosSnapshot(state)
          const payload = snapshot ?? { macros: [] }
          const summary = { macros_count: payload.macros?.length ?? 0 }
          console.log(renderCommandOutput({
            format: outputFormat,
            command: "available-macros-json",
            payload,
            summary,
          }))
        }),
    )
    .command(
      "start",
      new Command()
        .description("Start a persistent tmux game session and cast recording")
        .option("--state-file <path:string>", "State file path", { default: DEFAULT_STATE_FILE })
        .option("--bin <path:string>", "Path to curses binary", {
          default: "./out/build/linux-curses/src/cataclysm-bn",
        })
        .option("--world <name:string>", "World to auto-load", { default: "" })
        .option("--seed <value:string>", "Deterministic seed string", { default: "" })
        .option("--artifact-dir <path:string>", "Artifact directory")
        .option("--session <name:string>", "tmux session name (default singleton: curses-cli)")
        .option("--userdir <path:string>", "Userdir path")
        .option("--tmux-bin <path:string>", "tmux binary", { default: "tmux" })
        .option("--tmux-socket <name:string>", "tmux socket name (default singleton: curses-cli)")
        .option("--width <value:number>", "Terminal width for tmux/asciinema", { default: 120 })
        .option("--height <value:number>", "Terminal height for tmux/asciinema", { default: 40 })
        .option("--render-webp <value:boolean>", "Render captures to webp", { default: true })
        .option("--webp-quality <value:number>", "Webp quality", { default: 70 })
        .option("--magick-bin <path:string>", "ImageMagick binary", { default: "magick" })
        .option("--output-format <value:string>", "Output format: json|ai", {
          default: DEFAULT_OUTPUT_FORMAT,
        })
        .option(
          "--record-cast <value:boolean>",
          "Deprecated: ignored, cast recording is always enabled",
          { default: true },
        )
        .option("--asciinema-bin <path:string>", "asciinema binary", { default: "asciinema" })
        .option(
          "--cast-idle-time-limit <seconds:number>",
          "Optional asciinema idle-time limit seconds",
        )
        .action(async (options) => {
          const outputFormat = parseOutputFormat(options.outputFormat)
          const runId = timestamp()
          const tmuxSocket = normalizeTmuxSocket(options.tmuxSocket)
          const resolvedBinPath = resolve(REPO_ROOT, options.bin)
          await ensureLaunchBinaryExists(resolvedBinPath)
          configureTmuxBinary(options.tmuxBin)
          configureTmuxSocket(tmuxSocket)
          await ensureTmuxAvailable()
          await ensureSingleSessionPerMachine(options.tmuxBin)
          const castAvailable = await isBinaryAvailable(options.asciinemaBin, ["--version"])
          if (!castAvailable) {
            throw new Error(
              `asciinema binary not found: ${options.asciinemaBin}. ` +
                "Cast recording is always required and written to /tmp.",
            )
          }

          await using rollback = new StartRollbackResource(options.tmuxBin, tmuxSocket)
          const stateFilePath = resolveStateFilePath(options.stateFile)
          const runRootDir = options.artifactDir
            ? resolve(REPO_ROOT, options.artifactDir)
            : resolve(TMP_CLI_ROOT, runId)
          const artifactDir = join(runRootDir, "artifacts")
          const capturesDir = join(artifactDir, "captures")
          await ensureDir(runRootDir)
          await ensureDir(capturesDir)
          const width = Math.max(40, Math.floor(options.width))
          const height = Math.max(12, Math.floor(options.height))

          const sessionName = options.session ?? SINGLETON_SESSION_NAME
          const userdir = options.userdir ?? join(runRootDir, "userdir")
          await ensureDir(userdir)
          const availableKeysJsonPath = join(userdir, "available_keys.json")
          const availableMacrosJsonPath = join(userdir, "available_macros.json")
          const aiStateJsonPath = join(userdir, "ai_state.json")
          const launchCommand = buildLaunchCommand({
            binPath: resolvedBinPath,
            userdir,
            world: options.world,
            seed: options.seed,
            availableKeysJson: availableKeysJsonPath,
            availableMacrosJson: availableMacrosJsonPath,
            aiHelperOutput: aiStateJsonPath,
          })

          await using tmuxSession = await createTmuxSession({
            tmuxBin: options.tmuxBin,
            tmuxSocket,
            session: {
              name: sessionName,
              command: launchCommand,
              cwd: REPO_ROOT,
              width,
              height,
            },
          })

          const castDir = join(runRootDir, "casts")
          await ensureDir(castDir)
          const castTimestamp = sessionTimestamp()
          const castFile = join(castDir, `curses-session_${castTimestamp}.cast`)
          const castLogFile = join(castDir, `curses-session_${castTimestamp}-asciinema.log`)
          const castArgs = [
            "record",
            "--overwrite",
            "--headless",
            "--window-size",
            `${width}x${height}`,
          ]
          if (options.castIdleTimeLimit !== undefined) {
            castArgs.push("--idle-time-limit", `${options.castIdleTimeLimit}`)
          }
          castArgs.push(
            "--command",
            `env -u TMUX TERM=xterm-256color COLORTERM=truecolor tmux -L ${tmuxSocket} -2 attach -t ${sessionName}`,
            castFile,
          )

          const castPid = await startDetachedCastProcess({
            asciinemaBin: options.asciinemaBin,
            castArgs,
            castLogFile,
          })
          rollback.setCast(castPid, castFile)
          await delay(250)

          const state: SessionState = {
            id: runId,
            artifact_dir: artifactDir,
            captures_dir: capturesDir,
            session_name: sessionName,
            userdir,
            bin_path: resolvedBinPath,
            world: options.world,
            seed: options.seed,
            available_keys_json: availableKeysJsonPath,
            available_macros_json: availableMacrosJsonPath,
            tmux_bin: options.tmuxBin,
            tmux_socket: tmuxSocket,
            width,
            height,
            magick_bin: options.magickBin,
            render_webp: options.renderWebp,
            webp_quality: options.webpQuality,
            record_cast: true,
            asciinema_bin: options.asciinemaBin,
            cast_idle_time_limit: options.castIdleTimeLimit,
            cast_file: castFile,
            cast_pid: castPid,
            cast_log_file: castLogFile,
            status: "running",
            started_at: new Date().toISOString(),
            captures: [],
          }

          await saveState(stateFilePath, state)
          tmuxSession.markCompleted()
          rollback.markCompleted()
          console.log(renderCommandOutput({
            format: outputFormat,
            command: "start",
            payload: state,
            summary: {
              state_id: state.id,
              artifact_dir: displayPath(state.artifact_dir),
              session_name: state.session_name,
              status: state.status,
            },
          }))
        }),
    )
    .command(
      "get-game-state",
      new Command()
        .description("Read current game state JSON from active session userdir")
        .option("--state-file <path:string>", "State file path", { default: DEFAULT_STATE_FILE })
        .option("--include-cast-stats <value:boolean>", "Include cast stats", { default: true })
        .option("--output-format <value:string>", "Output format: json|ai", {
          default: DEFAULT_OUTPUT_FORMAT,
        })
        .action(async (options) => {
          const outputFormat = parseOutputFormat(options.outputFormat)
          const state = await requireRunningState(resolveStateFilePath(options.stateFile))
          const gameStatePath = join(state.userdir, "ai_state.json")
          let gameState: unknown = null
          try {
            const content = await Deno.readTextFile(gameStatePath)
            gameState = JSON.parse(content)
          } catch (_error) {
            void _error
          }

          const castStats = options.includeCastStats
            ? await getCastStats(state.cast_file)
            : undefined
          const payload = outputFormat === "ai"
            ? {
              state_id: state.id,
              status: state.status,
              game_state_path: gameStatePath,
              ai_state_summary: summarizeAiState(gameState),
              cast_file: state.cast_file,
              cast_log_file: state.cast_log_file,
              cast_stats: castStats,
              available_keys_json: state.available_keys_json,
              available_macros_json: state.available_macros_json,
              started_at: state.started_at,
            }
            : {
              state_id: state.id,
              status: state.status,
              game_state_path: gameStatePath,
              game_state: gameState,
              cast_file: state.cast_file,
              cast_log_file: state.cast_log_file,
              cast_stats: castStats,
              available_keys_json: state.available_keys_json,
              available_macros_json: state.available_macros_json,
              started_at: state.started_at,
            }
          console.log(renderCommandOutput({
            format: outputFormat,
            command: "get-game-state",
            payload,
            summary: {
              state_id: state.id,
              status: state.status,
              include_full_game_state: outputFormat === "json",
            },
          }))
        }),
    )
    .command(
      "state-dump",
      new Command()
        .description("Print compact state dump (pane excerpt + inputs + ai_state summary)")
        .option("--state-file <path:string>", "State file path", { default: DEFAULT_STATE_FILE })
        .option("--lines <value:number>", "Pane lines to capture", { default: 140 })
        .option("--max-chars <value:number>", "Maximum JSON character budget", { default: 6000 })
        .option("--output-format <value:string>", "Output format: json|ai", {
          default: DEFAULT_OUTPUT_FORMAT,
        })
        .action(async (options) => {
          const outputFormat = parseOutputFormat(options.outputFormat)
          await withRunningSession(options.stateFile, async (state, session) => {
            const pane = stripAnsiFromPane(await session.capturePane(options.lines))
            const promptInputs = listAvailableInputs(pane)
            const keysSnapshot = await loadAvailableKeysSnapshot(state)
            const macrosSnapshot = await loadAvailableMacrosSnapshot(state)

            const gameStatePath = join(state.userdir, "ai_state.json")
            let gameState: unknown = null
            try {
              const content = await Deno.readTextFile(gameStatePath)
              gameState = JSON.parse(content)
            } catch (_error) {
              void _error
            }

            const payload = compactStateDump({
              state,
              pane,
              promptInputs,
              keysSnapshot,
              macrosSnapshot,
              gameState,
              maxChars: options.maxChars,
            })
            console.log(renderCommandOutput({
              format: outputFormat,
              command: "state-dump",
              payload,
              summary: {
                state_id: state.id,
                status: state.status,
                available_inputs: (payload.available_inputs as string[] | undefined)
                  ?.length ?? 0,
              },
            }))
          })
        }),
    )
    .command(
      "snapshot",
      new Command()
        .description("Print current pane snapshot text")
        .option("--state-file <path:string>", "State file path", { default: DEFAULT_STATE_FILE })
        .option("--lines <value:number>", "Lines to capture", { default: 120 })
        .action(async (options) => {
          await withRunningSession(options.stateFile, async (_state, session) => {
            const pane = stripAnsiFromPane(await session.capturePane(options.lines))
            console.log(pane)
          })
        }),
    )
    .command(
      "recover-ui",
      new Command()
        .description("Detect current UI mode and recover back to gameplay")
        .option("--state-file <path:string>", "State file path", { default: DEFAULT_STATE_FILE })
        .option("--expect-mode <value:string>", "Expected mode: any|gameplay", {
          default: "gameplay",
        })
        .option("--max-steps <value:number>", "Maximum recovery steps", { default: 8 })
        .option("--output-format <value:string>", "Output format: json|ai", {
          default: DEFAULT_OUTPUT_FORMAT,
        })
        .action(async (options) => {
          const outputFormat = parseOutputFormat(options.outputFormat)
          const expectedMode = parseExpectedUiMode(options.expectMode)
          await withRunningSession(options.stateFile, async (_state, session) => {
            const result = await ensureExpectedMode({
              session,
              expectedMode,
              autoRecover: true,
              recoverMaxSteps: options.maxSteps,
            })

            const payload = {
              expected_mode: expectedMode,
              mode: result.mode,
              recovered: result.recovered,
              recovery_steps: result.recovery_steps,
            }
            console.log(renderCommandOutput({
              format: outputFormat,
              command: "recover-ui",
              payload,
              screen: result.screen,
            }))
          })
        }),
    )
    .command(
      "capture",
      new Command()
        .description("Persist capture artifacts (txt, markdown, optional webp)")
        .option("--state-file <path:string>", "State file path", { default: DEFAULT_STATE_FILE })
        .option("--id <value:string>", "Capture id", { required: true })
        .option("--caption <value:string>", "Capture caption", { default: "" })
        .option("--lines <value:number>", "Lines to capture", { default: 350 })
        .option("--output-format <value:string>", "Output format: json|ai", {
          default: DEFAULT_OUTPUT_FORMAT,
        })
        .action(async (options) => {
          const outputFormat = parseOutputFormat(options.outputFormat)
          const stateFile = resolveStateFilePath(options.stateFile)
          await withRunningSession(options.stateFile, async (state, session) => {
            const entry = await captureIntoArtifacts({
              state,
              session,
              id: options.id,
              caption: options.caption,
              lines: options.lines,
            })
            state.captures.push(entry)
            await saveState(stateFile, state)
            console.log(renderCommandOutput({
              format: outputFormat,
              command: "capture",
              payload: entry,
              summary: { id: entry.id, has_screenshot: entry.screenshot_file !== undefined },
            }))
          })
        }),
    )
    .command(
      "send-inputs",
      new Command()
        .description(
          "Send multiple input ids sequentially (raw keys, engine_action, mouse, /alias, or arrows: ↑↓←→)",
        )
        .option("--state-file <path:string>", "State file path", { default: DEFAULT_STATE_FILE })
        .option("--ids-json <value:string>", "JSON array of input ids", { required: true })
        .option("--col <value:number>", "1-based column for coordinate/mouse ids")
        .option("--row <value:number>", "1-based row for coordinate/mouse ids")
        .option(
          "--strict-prompt-inputs <value:boolean>",
          "Reject unavailable key inputs while prompt options are visible",
          { default: true },
        )
        .option("--repeat <value:number>", "Repeat whole sequence count", { default: 1 })
        .option("--delay-ms <value:number>", "Delay between key sends", {
          default: DEFAULT_BENCH_DELAY_MS,
        })
        .option("--waypoint-clicks <value:number>", "Repeated LMB clicks for mouse waypoint", {
          default: 2,
        })
        .option("--expect-mode <value:string>", "Expected mode before sending: any|gameplay", {
          default: "any",
        })
        .option(
          "--require-final-mode <value:string>",
          "Required mode after sending: any|gameplay",
          {
            default: "any",
          },
        )
        .option(
          "--auto-recover-mode <value:boolean>",
          "Attempt automatic UI recovery before sending when mode mismatches",
          { default: true },
        )
        .option("--recover-max-steps <value:number>", "Maximum UI recovery steps", {
          default: 8,
        })
        .option("--output-format <value:string>", "Output format: json|ai", {
          default: DEFAULT_OUTPUT_FORMAT,
        })
        .action(async (options) => {
          const outputFormat = parseOutputFormat(options.outputFormat)
          const expectedMode = parseExpectedUiMode(options.expectMode)
          const requiredFinalMode = parseExpectedUiMode(options.requireFinalMode)
          await withRunningSession(options.stateFile, async (state, session) => {
            const pane = stripAnsiFromPane(await session.capturePane(220))
            let promptInputs = listAvailableInputs(pane)
            const availableKeysSnapshot = await loadAvailableKeysSnapshot(state)
            const availableMacrosSnapshot = await loadAvailableMacrosSnapshot(state)
            const ids = parseInputIdsJson(options.idsJson)
            const repeat = Math.max(1, options.repeat)
            const delayMs = Math.max(0, options.delayMs)
            const waypointClicks = Math.max(1, options.waypointClicks)
            const dispatched: Array<unknown> = []
            const modeBefore = await ensureExpectedMode({
              session,
              expectedMode,
              autoRecover: options.autoRecoverMode,
              recoverMaxSteps: options.recoverMaxSteps,
            })

            for (let index = 0; index < repeat; index += 1) {
              for (const id of ids) {
                const resolution = await resolveSessionInputWithRefresh({
                  state,
                  session,
                  paneLines: 220,
                  inputId: id,
                  strictPromptInputs: options.strictPromptInputs,
                  promptInputs,
                  availableKeysSnapshot,
                  availableMacrosSnapshot,
                  coordinate: {
                    col: options.col,
                    row: options.row,
                  },
                })
                promptInputs = resolution.promptInputs
                const resolvedInput = resolution.resolvedInput
                const compactId = parseCompactInputId(id)
                const waypointMode = compactId === "mouse:waypoint" ||
                  compactId === "engine_action:COORDINATE"

                if (resolvedInput.kind === "key") {
                  await session.sendKeys(resolvedInput.keys)
                  dispatched.push({ id, kind: "key", keys: resolvedInput.keys })
                } else if (waypointMode) {
                  const waypointResult = await sendWaypointClicks({
                    session,
                    col: resolvedInput.x,
                    row: resolvedInput.y,
                    delayMs,
                    clicks: waypointClicks,
                  })
                  dispatched.push({
                    id,
                    kind: "waypoint",
                    x: resolvedInput.x,
                    y: resolvedInput.y,
                    clicks_sent: waypointResult.clicks_sent,
                  })
                } else {
                  await session.sendMouse({
                    x: resolvedInput.x,
                    y: resolvedInput.y,
                    button: resolvedInput.button,
                    event: resolvedInput.event,
                  })
                  dispatched.push({
                    id,
                    kind: "mouse",
                    x: resolvedInput.x,
                    y: resolvedInput.y,
                    button: resolvedInput.button,
                    event: resolvedInput.event,
                  })
                }

                if (delayMs > 0) {
                  await delay(delayMs)
                }
              }
            }

            const modeAfter = await ensureExpectedMode({
              session,
              expectedMode: requiredFinalMode,
              autoRecover: options.autoRecoverMode,
              recoverMaxSteps: options.recoverMaxSteps,
            })

            const payload = outputFormat === "ai"
              ? {
                ids_count: ids.length,
                dispatched_count: dispatched.length,
                repeat,
                delay_ms: delayMs,
                expected_mode: expectedMode,
                required_final_mode: requiredFinalMode,
                pre_recovered: modeBefore.recovered,
                post_recovered: modeAfter.recovered,
              }
              : {
                ids,
                dispatched,
                repeat,
                delay_ms: delayMs,
                expected_mode: expectedMode,
                required_final_mode: requiredFinalMode,
                mode_before: modeBefore,
                mode_after: modeAfter,
              }
            const screen = await captureScreenExcerpt(session)
            console.log(renderCommandOutput({
              format: outputFormat,
              command: "send-inputs",
              payload,
              summary: {
                ids_count: ids.length,
                dispatched_count: dispatched.length,
                repeat,
              },
              screen,
            }))
          })
        }),
    )
    .command(
      "send-mouse",
      new Command()
        .description("Send one mouse event (xterm SGR) into curses pane")
        .option("--state-file <path:string>", "State file path", { default: DEFAULT_STATE_FILE })
        .option("--col <value:number>", "1-based column coordinate", { required: true })
        .option("--row <value:number>", "1-based row coordinate", { required: true })
        .option("--button <value:string>", "left|middle|right|wheel_up|wheel_down|none", {
          default: "left",
        })
        .option("--event <value:string>", "click|press|release|move", { default: "click" })
        .option("--repeat <value:number>", "Repeat count", { default: 1 })
        .option("--delay-ms <value:number>", "Delay between repeats", {
          default: DEFAULT_BENCH_DELAY_MS,
        })
        .option("--output-format <value:string>", "Output format: json|ai", {
          default: DEFAULT_OUTPUT_FORMAT,
        })
        .action(async (options) => {
          const outputFormat = parseOutputFormat(options.outputFormat)
          await withRunningSession(options.stateFile, async (_state, session) => {
            const allowedButtons: MouseButton[] = [
              "left",
              "middle",
              "right",
              "wheel_up",
              "wheel_down",
              "none",
            ]
            const allowedEvents: MouseEventType[] = ["click", "press", "release", "move"]
            if (!allowedButtons.includes(options.button as MouseButton)) {
              throw new Error(`invalid --button: ${options.button}`)
            }
            if (!allowedEvents.includes(options.event as MouseEventType)) {
              throw new Error(`invalid --event: ${options.event}`)
            }
            const button = options.button as MouseButton
            const event = options.event as MouseEventType

            const repeat = Math.max(1, options.repeat)
            const delayMs = Math.max(0, options.delayMs)
            for (let index = 0; index < repeat; index += 1) {
              await session.sendMouse({
                x: options.col,
                y: options.row,
                button,
                event,
              })
              if (delayMs > 0) {
                await delay(delayMs)
              }
            }

            const payload = {
              x: options.col,
              y: options.row,
              button,
              event,
              repeat,
              delay_ms: delayMs,
            }
            const screen = await captureScreenExcerpt(session)
            console.log(renderCommandOutput({
              format: outputFormat,
              command: "send-mouse",
              payload,
              summary: { repeat, button, event },
              screen,
            }))
          })
        }),
    )
    .command(
      "send-waypoint",
      new Command()
        .description("Click the same tile repeatedly to trigger waypoint movement")
        .option("--state-file <path:string>", "State file path", { default: DEFAULT_STATE_FILE })
        .option("--col <value:number>", "1-based column coordinate", { required: true })
        .option("--row <value:number>", "1-based row coordinate", { required: true })
        .option("--delay-ms <value:number>", "Delay between repeated clicks", {
          default: DEFAULT_BENCH_DELAY_MS,
        })
        .option("--waypoint-clicks <value:number>", "Repeated LMB clicks", {
          default: 2,
        })
        .option("--output-format <value:string>", "Output format: json|ai", {
          default: DEFAULT_OUTPUT_FORMAT,
        })
        .action(async (options) => {
          const outputFormat = parseOutputFormat(options.outputFormat)
          await withRunningSession(options.stateFile, async (_state, session) => {
            const delayMs = Math.max(0, options.delayMs)
            const waypointClicks = Math.max(1, options.waypointClicks)
            const waypointResult = await sendWaypointClicks({
              session,
              col: options.col,
              row: options.row,
              delayMs,
              clicks: waypointClicks,
            })

            const payload = {
              waypoint: {
                x: options.col,
                y: options.row,
              },
              clicks_sent: waypointResult.clicks_sent,
              delay_ms: delayMs,
              waypoint_clicks: waypointClicks,
            }
            const screen = await captureScreenExcerpt(session)
            console.log(renderCommandOutput({
              format: outputFormat,
              command: "send-waypoint",
              payload,
              summary: { clicks_sent: waypointResult.clicks_sent, waypoint_clicks: waypointClicks },
              screen,
            }))
          })
        }),
    )
    .command(
      "wait-text",
      new Command()
        .description("Wait until pane contains text")
        .option("--state-file <path:string>", "State file path", { default: DEFAULT_STATE_FILE })
        .option("--text <value:string>", "Text to wait for", { required: true })
        .option("--timeout-ms <value:number>", "Timeout ms", { default: 120000 })
        .option("--poll-interval-ms <value:number>", "Poll interval ms", { default: 120 })
        .option("--output-format <value:string>", "Output format: json|ai", {
          default: DEFAULT_OUTPUT_FORMAT,
        })
        .action(async (options) => {
          const outputFormat = parseOutputFormat(options.outputFormat)
          await withRunningSession(options.stateFile, async (_state, session) => {
            await session.waitForText(options.text, {
              timeoutMs: options.timeoutMs,
              pollIntervalMs: options.pollIntervalMs,
            })
            const payload = { ok: true, text: options.text }
            const screen = await captureScreenExcerpt(session)
            console.log(renderCommandOutput({
              format: outputFormat,
              command: "wait-text",
              payload,
              summary: { ok: true },
              screen,
            }))
          })
        }),
    )
    .command(
      "wait-text-gone",
      new Command()
        .description("Wait until pane no longer contains text")
        .option("--state-file <path:string>", "State file path", { default: DEFAULT_STATE_FILE })
        .option("--text <value:string>", "Text to wait to disappear", { required: true })
        .option("--timeout-ms <value:number>", "Timeout ms", { default: 120000 })
        .option("--poll-interval-ms <value:number>", "Poll interval ms", { default: 120 })
        .option("--output-format <value:string>", "Output format: json|ai", {
          default: DEFAULT_OUTPUT_FORMAT,
        })
        .action(async (options) => {
          const outputFormat = parseOutputFormat(options.outputFormat)
          await withRunningSession(options.stateFile, async (_state, session) => {
            await session.waitForTextGone(options.text, {
              timeoutMs: options.timeoutMs,
              pollIntervalMs: options.pollIntervalMs,
            })
            const payload = { ok: true, text_gone: options.text }
            const screen = await captureScreenExcerpt(session)
            console.log(renderCommandOutput({
              format: outputFormat,
              command: "wait-text-gone",
              payload,
              summary: { ok: true },
              screen,
            }))
          })
        }),
    )
    .command(
      "sleep",
      new Command()
        .description("Wait in-session workflow")
        .option("--ms <value:number>", "Milliseconds", { required: true })
        .option("--output-format <value:string>", "Output format: json|ai", {
          default: DEFAULT_OUTPUT_FORMAT,
        })
        .action(async (options) => {
          const outputFormat = parseOutputFormat(options.outputFormat)
          await delay(options.ms)
          const payload = { ok: true, slept_ms: options.ms }
          console.log(renderCommandOutput({
            format: outputFormat,
            command: "sleep",
            payload,
            summary: { ok: true },
          }))
        }),
    )
    .command(
      "stop",
      new Command()
        .description("Stop session and finalize manifest/index artifacts")
        .option("--state-file <path:string>", "State file path", { default: DEFAULT_STATE_FILE })
        .option("--output-format <value:string>", "Output format: json|ai", {
          default: DEFAULT_OUTPUT_FORMAT,
        })
        .action(async (options) => {
          const outputFormat = parseOutputFormat(options.outputFormat)
          const stateFile = resolveStateFilePath(options.stateFile)
          const state = await loadState(stateFile)
          let finalScreen: string | undefined
          {
            configureTmuxBinary(state.tmux_bin)
            configureTmuxSocket(normalizeTmuxSocket(state.tmux_socket))
            await using _cleanup = new StopCleanupResource({
              tmuxBin: state.tmux_bin,
              tmuxSocket: normalizeTmuxSocket(state.tmux_socket),
              sessionName: state.session_name,
              castPid: state.cast_pid,
              castFile: state.cast_file,
            })

            const hasSession = await TmuxSession.hasSession(state.session_name, REPO_ROOT)
            if (hasSession) {
              const session = await TmuxSession.attach(state.session_name, REPO_ROOT)
              const finalPane = stripAnsiFromPane(await session.capturePane(500))
              finalScreen = trimMiddle(finalPane, 3200)
              await Deno.writeTextFile(join(state.artifact_dir, "final-pane.txt"), finalPane)
            }
          }

          state.status = "stopped"
          state.finished_at = new Date().toISOString()

          const castStats = await getCastStats(state.cast_file)
          try {
            state.cast_file = await copyFileIntoArtifactDir(state.cast_file, state.artifact_dir)
          } catch (error) {
            const message = error instanceof Error ? error.message : String(error)
            console.error(`warning: failed to copy cast file into artifact dir: ${message}`)
          }
          try {
            state.cast_log_file = await copyFileIntoArtifactDir(
              state.cast_log_file,
              state.artifact_dir,
            )
          } catch (error) {
            const message = error instanceof Error ? error.message : String(error)
            console.error(`warning: failed to copy cast log into artifact dir: ${message}`)
          }

          const metadata = {
            generated_at: new Date().toISOString(),
            mode: "mcp_cli",
            session_id: state.id,
            session_name: state.session_name,
            started_at: state.started_at,
            finished_at: state.finished_at,
            binary: state.bin_path,
            world: state.world,
            available_keys_json: state.available_keys_json,
            available_macros_json: state.available_macros_json,
            cast_file: state.cast_file,
            cast_log_file: state.cast_log_file,
            cast_stats: castStats,
            captures: state.captures,
          }

          await Deno.writeTextFile(
            join(state.artifact_dir, "manifest.json"),
            JSON.stringify(metadata, null, 2),
          )

          await writeIndex({
            outputPath: join(state.artifact_dir, "index.md"),
            sessionName: "live-curses-mcp-session",
            captures: state.captures,
            castFile: state.cast_file,
          })

          await saveState(stateFile, state)
          const payload = {
            artifact_dir: displayPath(state.artifact_dir),
            cast_file: state.cast_file,
            cast_log_file: state.cast_log_file,
            cast_stats: castStats,
            captures: state.captures.length,
            available_keys_json: state.available_keys_json,
            available_macros_json: state.available_macros_json,
          }
          console.log(renderCommandOutput({
            format: outputFormat,
            command: "stop",
            payload,
            summary: {
              captures: state.captures.length,
              has_cast: state.cast_file !== undefined,
            },
            screen: finalScreen,
          }))
        }),
    )
    .parse(Deno.args)
}
