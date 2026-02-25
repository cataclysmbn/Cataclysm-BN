#!/usr/bin/env -S deno run --allow-run --allow-read --allow-write

import { ensureDir } from "@std/fs"
import { dirname, fromFileUrl, join, relative, resolve } from "@std/path"
import { Command } from "@cliffy/command"
import {
  buildLaunchCommand,
  listAvailableInputs,
  resolveInputKey,
  sanitizeId,
  sessionTimestamp,
  timestamp,
  writeCodeBlockCapture,
  writeIndex,
} from "./common.ts"
import type { CaptureEntry } from "./common.ts"
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

const startDetachedCastProcess = async (
  asciinemaBin: string,
  castArgs: string[],
  castLogFile: string,
): Promise<number> => {
  const escapedArgs = [shellEscape(asciinemaBin), ...castArgs.map(shellEscape)].join(" ")
  const launchScript = `nohup env -u TMUX ${escapedArgs} >${
    shellEscape(castLogFile)
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

type AvailableKeysSnapshot = {
  actions?: Array<{
    id: string
    name: string
    key: string
    requires_coordinate?: boolean
    mouse_capable?: boolean
  }>
}

const loadAvailableKeysSnapshot = async (
  state: SessionState,
): Promise<AvailableKeysSnapshot | undefined> => {
  if (state.available_keys_json === undefined || state.available_keys_json.length === 0) {
    return undefined
  }

  try {
    const content = await Deno.readTextFile(state.available_keys_json)
    return JSON.parse(content) as AvailableKeysSnapshot
  } catch {
    return undefined
  }
}

type ResolvedSessionInput =
  | {
    kind: "key"
    key: string
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
    return "key:/"
  }

  const compactToken = normalized.slice(1)
  const keyAlias = COMPACT_KEY_ALIASES[compactToken.toLowerCase()]
  if (keyAlias !== undefined) {
    return `key:${keyAlias}`
  }

  return `engine_action:${compactToken}`
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

const resolveSessionInput = async (
  state: SessionState,
  inputId: string,
  coordinate: CoordinateOptions = {},
): Promise<ResolvedSessionInput> => {
  const normalized = parseCompactInputId(inputId)

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
    return {
      kind: "key",
      key: resolveInputKey(normalized),
    }
  }

  const actionId = normalized.slice("engine_action:".length)
  if (actionId.length === 0) {
    throw new Error("engine_action input id cannot be empty")
  }

  const snapshot = await loadAvailableKeysSnapshot(state)
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
    key: resolveInputKey(`key:${matched.key}`),
  }
}

const renderTextCaptureWebp = async (
  magickBin: string,
  textPath: string,
  screenshotPath: string,
  quality: number,
): Promise<void> => {
  const output = await new Deno.Command(magickBin, {
    args: [
      "-background",
      "#101010",
      "-fill",
      "#f5f5f5",
      "-pointsize",
      "16",
      "-interline-spacing",
      "2",
      `label:@${textPath}`,
      "-strip",
      "-colorspace",
      "sRGB",
      "-quality",
      `${quality}`,
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
      screenshotPath,
    ],
    stdout: "piped",
    stderr: "piped",
  }).output()

  if (!output.success) {
    const decoder = new TextDecoder()
    const stderr = decoder.decode(output.stderr).trim()
    throw new Error(
      stderr.length > 0
        ? `failed to render webp screenshot with ${magickBin}: ${stderr}`
        : `failed to render webp screenshot with ${magickBin}`,
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
    configureTmuxSocket(state.tmux_socket)
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

const captureIntoArtifacts = async (
  state: SessionState,
  session: TmuxSession,
  id: string,
  caption: string,
  lines: number,
): Promise<CaptureEntry> => {
  const captureNumber = state.captures.length + 1
  const fileName = `${String(captureNumber).padStart(2, "0")}-${sanitizeId(id)}.txt`
  const absPath = join(state.captures_dir, fileName)
  const content = stripAnsiFromPane(await session.capturePane(lines))
  await Deno.writeTextFile(absPath, content)

  const codeBlockFileName = fileName.replace(/\.txt$/, ".md")
  const absCodeBlockPath = join(state.captures_dir, codeBlockFileName)
  await writeCodeBlockCapture(absCodeBlockPath, content)

  let screenshotPath: string | undefined
  if (state.render_webp && await isBinaryAvailable(state.magick_bin)) {
    const screenshotName = fileName.replace(/\.txt$/, ".webp")
    const absScreenshotPath = join(state.captures_dir, screenshotName)
    await renderTextCaptureWebp(state.magick_bin, absPath, absScreenshotPath, state.webp_quality)
    screenshotPath = displayPath(absScreenshotPath)
  }

  return {
    id,
    caption,
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
  configureTmuxSocket(state.tmux_socket)
  const session = await TmuxSession.attach(state.session_name, REPO_ROOT)
  await handler(state, session)
}

class PendingTmuxSessionResource {
  #tmuxBin: string
  #tmuxSocket: string | undefined
  #sessionName: string
  #cwd: string
  #completed = false

  constructor(tmuxBin: string, tmuxSocket: string | undefined, sessionName: string, cwd: string) {
    this.#tmuxBin = tmuxBin
    this.#tmuxSocket = tmuxSocket
    this.#sessionName = sessionName
    this.#cwd = cwd
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

const createTmuxSession = async (
  tmuxBin: string,
  tmuxSocket: string | undefined,
  options: {
    name: string
    command: string
    cwd: string
    width: number
    height: number
  },
): Promise<PendingTmuxSessionResource> => {
  configureTmuxBinary(tmuxBin)
  configureTmuxSocket(tmuxSocket)
  await TmuxSession.start(options)

  try {
    const attached = await TmuxSession.attach(options.name, options.cwd)
    await attached.capturePane(1)
  } catch (error) {
    try {
      await TmuxSession.killByName(options.name, options.cwd)
    } catch {
    }
    throw error
  }

  return new PendingTmuxSessionResource(tmuxBin, tmuxSocket, options.name, options.cwd)
}

const sendWaypointClicks = async (
  session: TmuxSession,
  col: number,
  row: number,
  delayMs: number,
  clicks: number,
): Promise<{ clicks_sent: number }> => {
  const clickCount = Math.max(1, clicks)
  for (let index = 0; index < clickCount; index += 1) {
    await session.sendMouse({
      x: col,
      y: row,
      button: "left",
      event: "click",
    })
    if (delayMs > 0 && index + 1 < clickCount) {
      await delay(delayMs)
    }
  }

  return {
    clicks_sent: clickCount,
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

  constructor(
    tmuxBin: string,
    tmuxSocket: string | undefined,
    sessionName: string,
    castPid: number | undefined,
    castFile: string | undefined,
  ) {
    this.#tmuxBin = tmuxBin
    this.#tmuxSocket = tmuxSocket
    this.#sessionName = sessionName
    this.#castPid = castPid
    this.#castFile = castFile
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
        .action(() => {
          console.log(JSON.stringify(
            {
              deprecated: true,
              message:
                "Static key catalog was removed. Use 'inputs-jsonl' or 'available-keys-json' for runtime keys.",
            },
            null,
            2,
          ))
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
                    "Use send-input --id mouse:waypoint --col <x> --row <y> to click a tile waypoint.",
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

            for (const input of inputs) {
              console.log(JSON.stringify(input))
            }
          })
        }),
    )
    .command(
      "available-keys-json",
      new Command()
        .description("Read currently available keybindings JSON from active game context")
        .option("--state-file <path:string>", "State file path", { default: DEFAULT_STATE_FILE })
        .action(async (options) => {
          const state = await requireRunningState(resolveStateFilePath(options.stateFile))
          const snapshot = await loadAvailableKeysSnapshot(state)
          if (snapshot === undefined) {
            console.log(JSON.stringify({ actions: [] }, null, 2))
            return
          }

          console.log(JSON.stringify(snapshot, null, 2))
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
          const runId = timestamp()
          const tmuxSocket = options.tmuxSocket ?? SINGLETON_TMUX_SOCKET
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
          const artifactDir = options.artifactDir
            ? resolve(REPO_ROOT, options.artifactDir)
            : resolve(TMP_CLI_ROOT, "runs", `live-${runId}`)
          const capturesDir = join(artifactDir, "captures")
          await ensureDir(capturesDir)
          const width = Math.max(40, Math.floor(options.width))
          const height = Math.max(12, Math.floor(options.height))

          const sessionName = options.session ?? SINGLETON_SESSION_NAME
          const defaultUserdirRoot = resolve(TMP_CLI_ROOT, "userdirs")
          await ensureDir(defaultUserdirRoot)
          const userdir = options.userdir ??
            await Deno.makeTempDir({ dir: defaultUserdirRoot, prefix: "userdir-" })
          const availableKeysJsonPath = join(userdir, "available_keys.json")
          const aiStateJsonPath = join(userdir, "ai_state.json")
          const launchCommand = buildLaunchCommand(
            resolve(REPO_ROOT, options.bin),
            userdir,
            options.world,
            options.seed,
            availableKeysJsonPath,
            aiStateJsonPath,
          )

          await using tmuxSession = await createTmuxSession(
            options.tmuxBin,
            tmuxSocket,
            {
              name: sessionName,
              command: launchCommand,
              cwd: REPO_ROOT,
              width,
              height,
            },
          )

          const castDir = resolve(TMP_CLI_ROOT, "casts")
          await ensureDir(castDir)
          const castFile = join(castDir, `curses-session_${sessionTimestamp()}.cast`)
          const castLogFile = join(castDir, `curses-session_${sessionTimestamp()}-asciinema.log`)
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

          const castPid = await startDetachedCastProcess(
            options.asciinemaBin,
            castArgs,
            castLogFile,
          )
          rollback.setCast(castPid, castFile)
          await delay(250)

          const state: SessionState = {
            id: runId,
            artifact_dir: artifactDir,
            captures_dir: capturesDir,
            session_name: sessionName,
            userdir,
            bin_path: options.bin,
            world: options.world,
            seed: options.seed,
            available_keys_json: availableKeysJsonPath,
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
          console.log(JSON.stringify(state, null, 2))
        }),
    )
    .command(
      "get-game-state",
      new Command()
        .description("Read current game state JSON from active session userdir")
        .option("--state-file <path:string>", "State file path", { default: DEFAULT_STATE_FILE })
        .option("--include-cast-stats <value:boolean>", "Include cast stats", { default: true })
        .action(async (options) => {
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
          console.log(JSON.stringify(
            {
              state_id: state.id,
              status: state.status,
              game_state_path: gameStatePath,
              game_state: gameState,
              cast_file: state.cast_file,
              cast_log_file: state.cast_log_file,
              cast_stats: castStats,
              available_keys_json: state.available_keys_json,
              started_at: state.started_at,
            },
            null,
            2,
          ))
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
      "capture",
      new Command()
        .description("Persist capture artifacts (txt, markdown, optional webp)")
        .option("--state-file <path:string>", "State file path", { default: DEFAULT_STATE_FILE })
        .option("--id <value:string>", "Capture id", { required: true })
        .option("--caption <value:string>", "Capture caption", { default: "" })
        .option("--lines <value:number>", "Lines to capture", { default: 350 })
        .action(async (options) => {
          const stateFile = resolveStateFilePath(options.stateFile)
          await withRunningSession(options.stateFile, async (state, session) => {
            const entry = await captureIntoArtifacts(
              state,
              session,
              options.id,
              options.caption,
              options.lines,
            )
            state.captures.push(entry)
            await saveState(stateFile, state)
            console.log(JSON.stringify(entry, null, 2))
          })
        }),
    )
    .command(
      "send-input",
      new Command()
        .description(
          "Send one input id (raw key, engine_action:<id>, mouse:waypoint, /alias, or arrows: ↑↓←→)",
        )
        .option("--state-file <path:string>", "State file path", { default: DEFAULT_STATE_FILE })
        .option("--id <value:string>", "Input id or raw key", { required: true })
        .option("--col <value:number>", "1-based column for coordinate/mouse inputs")
        .option("--row <value:number>", "1-based row for coordinate/mouse inputs")
        .option("--delay-ms <value:number>", "Delay between repeated waypoint clicks", {
          default: 40,
        })
        .option("--waypoint-clicks <value:number>", "Repeated LMB clicks for mouse waypoint", {
          default: 2,
        })
        .action(async (options) => {
          await withRunningSession(options.stateFile, async (state, session) => {
            const resolvedInput = await resolveSessionInput(state, options.id, {
              col: options.col,
              row: options.row,
            })
            const compactId = parseCompactInputId(options.id)
            const waypointMode = compactId === "mouse:waypoint" ||
              compactId === "engine_action:COORDINATE"
            const movementDelayMs = Math.max(0, options.delayMs)
            const waypointClicks = Math.max(1, options.waypointClicks)

            if (resolvedInput.kind === "key") {
              await session.sendKeys([resolvedInput.key])
              console.log(JSON.stringify({ id: options.id, key: resolvedInput.key }, null, 2))
              return
            }

            if (waypointMode) {
              const waypointResult = await sendWaypointClicks(
                session,
                resolvedInput.x,
                resolvedInput.y,
                movementDelayMs,
                waypointClicks,
              )
              console.log(JSON.stringify(
                {
                  id: options.id,
                  waypoint: {
                    x: resolvedInput.x,
                    y: resolvedInput.y,
                  },
                  clicks_sent: waypointResult.clicks_sent,
                },
                null,
                2,
              ))
              return
            }

            await session.sendMouse({
              x: resolvedInput.x,
              y: resolvedInput.y,
              button: resolvedInput.button,
              event: resolvedInput.event,
            })
            console.log(JSON.stringify(
              {
                id: options.id,
                mouse: {
                  x: resolvedInput.x,
                  y: resolvedInput.y,
                  button: resolvedInput.button,
                  event: resolvedInput.event,
                },
              },
              null,
              2,
            ))
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
        .option("--repeat <value:number>", "Repeat whole sequence count", { default: 1 })
        .option("--delay-ms <value:number>", "Delay between key sends", { default: 40 })
        .option("--waypoint-clicks <value:number>", "Repeated LMB clicks for mouse waypoint", {
          default: 2,
        })
        .action(async (options) => {
          await withRunningSession(options.stateFile, async (state, session) => {
            const ids = parseInputIdsJson(options.idsJson)
            const repeat = Math.max(1, options.repeat)
            const delayMs = Math.max(0, options.delayMs)
            const waypointClicks = Math.max(1, options.waypointClicks)
            const dispatched: Array<unknown> = []

            for (let index = 0; index < repeat; index += 1) {
              for (const id of ids) {
                const resolvedInput = await resolveSessionInput(state, id, {
                  col: options.col,
                  row: options.row,
                })
                const compactId = parseCompactInputId(id)
                const waypointMode = compactId === "mouse:waypoint" ||
                  compactId === "engine_action:COORDINATE"

                if (resolvedInput.kind === "key") {
                  await session.sendKeys([resolvedInput.key])
                  dispatched.push({ id, kind: "key", key: resolvedInput.key })
                } else if (waypointMode) {
                  const waypointResult = await sendWaypointClicks(
                    session,
                    resolvedInput.x,
                    resolvedInput.y,
                    delayMs,
                    waypointClicks,
                  )
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

            console.log(JSON.stringify(
              {
                ids,
                dispatched,
                repeat,
                delay_ms: delayMs,
              },
              null,
              2,
            ))
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
        .option("--delay-ms <value:number>", "Delay between repeats", { default: 40 })
        .action(async (options) => {
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

            console.log(JSON.stringify(
              {
                x: options.col,
                y: options.row,
                button,
                event,
                repeat,
                delay_ms: delayMs,
              },
              null,
              2,
            ))
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
          default: 40,
        })
        .option("--waypoint-clicks <value:number>", "Repeated LMB clicks", {
          default: 2,
        })
        .action(async (options) => {
          await withRunningSession(options.stateFile, async (_state, session) => {
            const delayMs = Math.max(0, options.delayMs)
            const waypointClicks = Math.max(1, options.waypointClicks)
            const waypointResult = await sendWaypointClicks(
              session,
              options.col,
              options.row,
              delayMs,
              waypointClicks,
            )

            console.log(JSON.stringify(
              {
                waypoint: {
                  x: options.col,
                  y: options.row,
                },
                clicks_sent: waypointResult.clicks_sent,
                delay_ms: delayMs,
                waypoint_clicks: waypointClicks,
              },
              null,
              2,
            ))
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
        .action(async (options) => {
          await withRunningSession(options.stateFile, async (_state, session) => {
            await session.waitForText(options.text, {
              timeoutMs: options.timeoutMs,
              pollIntervalMs: options.pollIntervalMs,
            })
            console.log(JSON.stringify({ ok: true, text: options.text }, null, 2))
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
        .action(async (options) => {
          await withRunningSession(options.stateFile, async (_state, session) => {
            await session.waitForTextGone(options.text, {
              timeoutMs: options.timeoutMs,
              pollIntervalMs: options.pollIntervalMs,
            })
            console.log(JSON.stringify({ ok: true, text_gone: options.text }, null, 2))
          })
        }),
    )
    .command(
      "sleep",
      new Command()
        .description("Wait in-session workflow")
        .option("--ms <value:number>", "Milliseconds", { required: true })
        .action(async (options) => {
          await delay(options.ms)
          console.log(JSON.stringify({ ok: true, slept_ms: options.ms }, null, 2))
        }),
    )
    .command(
      "stop",
      new Command()
        .description("Stop session and finalize manifest/index artifacts")
        .option("--state-file <path:string>", "State file path", { default: DEFAULT_STATE_FILE })
        .option("--status <value:string>", "Final status: passed or failed", { default: "passed" })
        .option("--failure <value:string>", "Failure message", { default: "" })
        .action(async (options) => {
          const stateFile = resolveStateFilePath(options.stateFile)
          const state = await loadState(stateFile)
          {
            configureTmuxBinary(state.tmux_bin)
            configureTmuxSocket(state.tmux_socket)
            await using _cleanup = new StopCleanupResource(
              state.tmux_bin,
              state.tmux_socket,
              state.session_name,
              state.cast_pid,
              state.cast_file,
            )

            const hasSession = await TmuxSession.hasSession(state.session_name, REPO_ROOT)
            if (hasSession) {
              const session = await TmuxSession.attach(state.session_name, REPO_ROOT)
              const finalPane = stripAnsiFromPane(await session.capturePane(500))
              await Deno.writeTextFile(join(state.artifact_dir, "final-pane.txt"), finalPane)
            }
          }

          state.status = "stopped"
          state.finished_at = new Date().toISOString()

          const castStats = await getCastStats(state.cast_file)

          const finalStatus = options.status === "failed" ? "failed" : "passed"
          const failure = options.failure.length > 0 ? options.failure : undefined

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
            status: finalStatus,
            failure,
            cast_file: state.cast_file,
            cast_log_file: state.cast_log_file,
            cast_stats: castStats,
            captures: state.captures,
          }

          await Deno.writeTextFile(
            join(state.artifact_dir, "manifest.json"),
            JSON.stringify(metadata, null, 2),
          )

          await writeIndex(
            join(state.artifact_dir, "index.md"),
            "live-curses-mcp-session",
            state.captures,
            finalStatus,
            state.cast_file,
            failure,
          )

          await saveState(stateFile, state)
          console.log(JSON.stringify(
            {
              artifact_dir: displayPath(state.artifact_dir),
              cast_file: state.cast_file,
              cast_log_file: state.cast_log_file,
              cast_stats: castStats,
              captures: state.captures.length,
              status: finalStatus,
            },
            null,
            2,
          ))
        }),
    )
    .parse(Deno.args)
}
