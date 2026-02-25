const TEXT_DECODER = new TextDecoder()
let tmuxBinary = "tmux"
let tmuxSocketName: string | undefined

type RunTmuxOptions = {
  cwd?: string
  allowFailure?: boolean
}

export type WaitOptions = {
  timeoutMs?: number
  pollIntervalMs?: number
  lines?: number
}

export type MouseButton = "left" | "middle" | "right" | "wheel_up" | "wheel_down" | "none"
export type MouseEventType = "press" | "release" | "click" | "move"

export type MouseEventOptions = {
  x: number
  y: number
  button?: MouseButton
  event?: MouseEventType
}

const delay = (ms: number): Promise<void> => new Promise((resolve) => setTimeout(resolve, ms))

const withSocketArgs = (args: string[]): string[] =>
  tmuxSocketName === undefined || tmuxSocketName.length === 0
    ? args
    : ["-L", tmuxSocketName, ...args]

const formatCommand = (args: string[]): string => [tmuxBinary, ...withSocketArgs(args)].join(" ")

const TMUX_KEY_ALIASES: Record<string, string> = {
  ESC: "Escape",
  RETURN: "Enter",
  TAB: "Tab",
  BACKTAB: "BTab",
  NPAGE: "NPage",
  PPAGE: "PPage",
  PAGEUP: "PageUp",
  PAGEDOWN: "PageDown",
  SPACE: "Space",
}

const normalizeTmuxKey = (key: string): string => {
  const trimmed = key.trim()
  if (trimmed.length === 0) {
    return trimmed
  }

  const upper = trimmed.toUpperCase()
  const alias = TMUX_KEY_ALIASES[upper]
  if (alias !== undefined) {
    return alias
  }

  const ctrlMatch = /^CTRL\+([A-Za-z])$/.exec(upper)
  if (ctrlMatch !== null) {
    return `C-${ctrlMatch[1]}`
  }

  return trimmed
}

const isTmuxNamedKey = (key: string): boolean => {
  return /^(Enter|Escape|Tab|BTab|Up|Down|Left|Right|Home|End|PageUp|PageDown|BSpace|Space|Delete|Insert|NPage|PPage|F\d+|C-[A-Za-z]|M-[A-Za-z]|S-[A-Za-z])$/
    .test(key)
}

const controlKeyHex = (key: string): string | undefined => {
  const match = /^C-([A-Za-z])$/.exec(key)
  if (match === null) {
    return undefined
  }

  const asciiCode = match[1].toUpperCase().charCodeAt(0)
  const controlCode = asciiCode - 64
  if (controlCode < 0 || controlCode > 31) {
    return undefined
  }
  return controlCode.toString(16).padStart(2, "0")
}

const normalizeMouseCoordinate = (value: number): number => {
  const normalized = Math.floor(value)
  return normalized < 1 ? 1 : normalized
}

const buttonCodeForPress = (button: MouseButton): number => {
  switch (button) {
    case "left":
      return 0
    case "middle":
      return 1
    case "right":
      return 2
    case "wheel_up":
      return 64
    case "wheel_down":
      return 65
    case "none":
      return 3
  }
}

const buttonCodeForMove = (button: MouseButton): number => {
  if (button === "none") {
    return 35
  }
  return buttonCodeForPress(button) + 32
}

const sgrMouseFrame = (code: number, x: number, y: number, pressed: boolean): string => {
  return `\x1b[<${code};${x};${y}${pressed ? "M" : "m"}`
}

const mouseFrames = (options: MouseEventOptions): string[] => {
  const button = options.button ?? "left"
  const event = options.event ?? "click"
  const x = normalizeMouseCoordinate(options.x)
  const y = normalizeMouseCoordinate(options.y)

  if (event === "move") {
    return [sgrMouseFrame(buttonCodeForMove(button), x, y, true)]
  }

  if (event === "release") {
    return [sgrMouseFrame(buttonCodeForPress(button), x, y, false)]
  }

  const pressFrame = sgrMouseFrame(buttonCodeForPress(button), x, y, true)
  if (event === "press" || button === "wheel_up" || button === "wheel_down") {
    return [pressFrame]
  }

  return [pressFrame, sgrMouseFrame(buttonCodeForPress(button), x, y, false)]
}

export const configureTmuxBinary = (path: string): void => {
  tmuxBinary = path
}

export const configureTmuxSocket = (socketName: string | undefined): void => {
  tmuxSocketName = socketName
}

export const ensureTmuxAvailable = async (): Promise<void> => {
  try {
    const output = await new Deno.Command(tmuxBinary, {
      args: ["-V"],
      stdout: "null",
      stderr: "null",
    }).output()
    if (!output.success) {
      throw new Error(`failed to execute ${tmuxBinary} -V`)
    }
  } catch (error) {
    if (error instanceof Deno.errors.NotFound) {
      throw new Error(
        `tmux binary not found: ${tmuxBinary}. Install tmux or pass --tmux-bin <path>.`,
      )
    }
    throw error
  }
}

const runTmux = async (args: string[], options: RunTmuxOptions = {}): Promise<string> => {
  const resolvedArgs = withSocketArgs(args)
  const output = await new Deno.Command(tmuxBinary, {
    args: resolvedArgs,
    cwd: options.cwd,
    stdout: "piped",
    stderr: "piped",
  }).output()

  if (!output.success && !options.allowFailure) {
    const stderr = TEXT_DECODER.decode(output.stderr).trim()
    const stdout = TEXT_DECODER.decode(output.stdout).trim()
    throw new Error(
      [
        `tmux command failed (${output.code}): ${formatCommand(args)}`,
        stderr.length > 0 ? `stderr:\n${stderr}` : "",
        stdout.length > 0 ? `stdout:\n${stdout}` : "",
      ]
        .filter((line) => line.length > 0)
        .join("\n"),
    )
  }

  return TEXT_DECODER.decode(output.stdout)
}

export type StartSessionOptions = {
  name: string
  command: string
  width?: number
  height?: number
  cwd?: string
}

export class TmuxSession {
  readonly name: string
  readonly target: string
  readonly cwd?: string

  private constructor(name: string, target: string, cwd?: string) {
    this.name = name
    this.target = target
    this.cwd = cwd
  }

  static async hasSession(name: string, cwd?: string): Promise<boolean> {
    const args = withSocketArgs(["has-session", "-t", name])
    const output = await new Deno.Command(tmuxBinary, {
      args,
      cwd,
      stdout: "null",
      stderr: "null",
    }).output()
    return output.success
  }

  static async start(options: StartSessionOptions): Promise<TmuxSession> {
    const width = options.width ?? 120
    const height = options.height ?? 40

    if (await TmuxSession.hasSession(options.name, options.cwd)) {
      throw new Error(`tmux session already exists: ${options.name}`)
    }

    await runTmux(
      [
        "new-session",
        "-d",
        "-s",
        options.name,
        "-e",
        "TERM=xterm-256color",
        "-e",
        "COLORTERM=truecolor",
        "-x",
        `${width}`,
        "-y",
        `${height}`,
        options.command,
      ],
      { cwd: options.cwd },
    )

    return new TmuxSession(options.name, `${options.name}:0.0`, options.cwd)
  }

  static async attach(name: string, cwd?: string): Promise<TmuxSession> {
    if (!await TmuxSession.hasSession(name, cwd)) {
      throw new Error(`tmux session not found: ${name}`)
    }

    return new TmuxSession(name, `${name}:0.0`, cwd)
  }

  static async killByName(name: string, cwd?: string): Promise<void> {
    await runTmux(["kill-session", "-t", name], { cwd, allowFailure: true })
  }

  async sendKeys(keys: string[]): Promise<void> {
    if (keys.length === 0) {
      return
    }

    for (const key of keys) {
      const normalizedKey = normalizeTmuxKey(key)
      const controlHex = controlKeyHex(normalizedKey)
      const args = controlHex !== undefined
        ? ["send-keys", "-t", this.target, "-H", controlHex]
        : isTmuxNamedKey(normalizedKey)
        ? ["send-keys", "-t", this.target, normalizedKey]
        : ["send-keys", "-t", this.target, "-l", normalizedKey]
      await runTmux(args, { cwd: this.cwd })
    }
  }

  async sendMouse(options: MouseEventOptions): Promise<void> {
    const frames = mouseFrames(options)
    for (const frame of frames) {
      await runTmux(["send-keys", "-t", this.target, "-l", frame], { cwd: this.cwd })
    }
  }

  async capturePane(lines = 250): Promise<string> {
    return await runTmux(
      ["capture-pane", "-p", "-t", this.target, "-S", `-${lines}`],
      { cwd: this.cwd },
    )
  }

  async waitForText(text: string, options: WaitOptions = {}): Promise<void> {
    const timeoutMs = options.timeoutMs ?? 60_000
    const pollIntervalMs = options.pollIntervalMs ?? 100
    const lines = options.lines ?? 300
    const startAt = Date.now()

    while (Date.now() - startAt <= timeoutMs) {
      const pane = await this.capturePane(lines)
      if (pane.includes(text)) {
        return
      }
      await delay(pollIntervalMs)
    }

    throw new Error(`timeout waiting for text: ${text}`)
  }

  async waitForTextGone(text: string, options: WaitOptions = {}): Promise<void> {
    const timeoutMs = options.timeoutMs ?? 60_000
    const pollIntervalMs = options.pollIntervalMs ?? 100
    const lines = options.lines ?? 300
    const startAt = Date.now()

    while (Date.now() - startAt <= timeoutMs) {
      const pane = await this.capturePane(lines)
      if (!pane.includes(text)) {
        return
      }
      await delay(pollIntervalMs)
    }

    throw new Error(`timeout waiting for text to disappear: ${text}`)
  }

  async dismissPrompts(prompts: string[], limit = 6): Promise<number> {
    let dismissed = 0

    for (let index = 0; index < limit; index += 1) {
      const pane = await this.capturePane()
      const hasPrompt = prompts.some((prompt) => pane.includes(prompt))
      if (!hasPrompt) {
        break
      }
      await this.sendKeys(["Space"])
      dismissed += 1
      await delay(120)
    }

    return dismissed
  }

  async kill(): Promise<void> {
    await TmuxSession.killByName(this.name, this.cwd)
  }
}
