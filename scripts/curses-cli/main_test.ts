import { assertEquals, assertStringIncludes } from "@std/assert"
import { join } from "@std/path"
import {
  buildLaunchCommand,
  detectPromptInputs,
  detectUiMode,
  listAvailableInputs,
  parseOutputFormat,
  renderCommandOutput,
  resolveInputKey,
  sanitizeId,
  writeCodeBlockCapture,
  writeIndex,
} from "./common.ts"

Deno.test("pr_verify: sanitizeId normalizes unsafe characters", () => {
  assertEquals(sanitizeId("open help/screen"), "open-help-screen")
  assertEquals(sanitizeId("already-safe_id"), "already-safe_id")
  assertEquals(sanitizeId("a---b"), "a-b")
})

Deno.test("pr_verify: buildLaunchCommand includes base args, optional world, and seed", () => {
  const withoutWorld = buildLaunchCommand({
    binPath: "/tmp/cata",
    userdir: "/tmp/user",
    world: "",
  })
  assertStringIncludes(withoutWorld, "TERM=xterm-256color")
  assertStringIncludes(withoutWorld, "COLORTERM=truecolor")
  assertStringIncludes(withoutWorld, "--basepath")
  assertStringIncludes(withoutWorld, "--userdir")
  assertStringIncludes(withoutWorld, "--no-blinking")

  const withWorld = buildLaunchCommand({
    binPath: "/tmp/cata",
    userdir: "/tmp/user",
    world: "fixture_world",
  })
  assertStringIncludes(withWorld, "--world")
  assertStringIncludes(withWorld, "fixture_world")

  const withSeed = buildLaunchCommand({
    binPath: "/tmp/cata",
    userdir: "/tmp/user",
    world: "",
    seed: "seed-01",
  })
  assertStringIncludes(withSeed, "--seed")
  assertStringIncludes(withSeed, "seed-01")

  const withAvailableKeysJson = buildLaunchCommand({
    binPath: "/tmp/cata",
    userdir: "/tmp/user",
    world: "",
    availableKeysJson: "/tmp/available_keys.json",
  })
  assertStringIncludes(withAvailableKeysJson, "CATA_AVAILABLE_KEYS_JSON")
  assertStringIncludes(withAvailableKeysJson, "/tmp/available_keys.json")

  const withAvailableMacrosJson = buildLaunchCommand({
    binPath: "/tmp/cata",
    userdir: "/tmp/user",
    world: "",
    availableMacrosJson: "/tmp/available_macros.json",
  })
  assertStringIncludes(withAvailableMacrosJson, "CATA_AVAILABLE_MACROS_JSON")
  assertStringIncludes(withAvailableMacrosJson, "/tmp/available_macros.json")
})

Deno.test("pr_verify: resolveInputKey handles action ids and raw key ids", () => {
  assertEquals(resolveInputKey("key:!"), "!")
  assertEquals(resolveInputKey("key:/"), "/")
  assertEquals(resolveInputKey("key:Enter"), "Enter")
  assertEquals(resolveInputKey("Escape"), "Escape")
})

Deno.test("pr_verify: parseOutputFormat supports ai aliases", () => {
  assertEquals(parseOutputFormat(undefined), "ai")
  assertEquals(parseOutputFormat("json"), "json")
  assertEquals(parseOutputFormat("ai"), "ai")
  assertEquals(parseOutputFormat("yaml"), "ai")
})

Deno.test("pr_verify: renderCommandOutput emits ai yaml wrapper", () => {
  const rendered = renderCommandOutput({
    format: "ai",
    command: "state-dump",
    payload: { available_inputs: ["w"] },
    summary: { state_id: "run-1" },
    screen: "line 1\nline 2",
  })

  assertStringIncludes(rendered, "command: 'state-dump'")
  assertStringIncludes(rendered, "payload:")
  assertStringIncludes(rendered, "available_inputs: ['w']")
  assertStringIncludes(rendered, "screen:")
  assertStringIncludes(rendered, "|-\n    line 1")
})

Deno.test("pr_verify: detectPromptInputs catches safe mode prompt inputs", () => {
  const pane =
    "Spotted fat zombie--safe mode is on! (Press ! to turn it off, press ' to ignore monster)"
  const inputs = detectPromptInputs(pane)
  assertEquals(inputs.some((input) => input.id === "key:!"), true)
  assertEquals(inputs.some((input) => input.id === "key:'"), true)
})

Deno.test("pr_verify: detectPromptInputs extracts parenthesized choice prompts", () => {
  const pane = "Really step into window frame? (Y)es  (N)o"
  const inputs = detectPromptInputs(pane)
  assertEquals(inputs.some((input) => input.id === "key:Y"), true)
  assertEquals(inputs.some((input) => input.id === "key:N"), true)
})

Deno.test("pr_verify: detectPromptInputs ignores sidebar hint pseudo-prompts", () => {
  const pane = "Press } to open sidebar options"
  const inputs = detectPromptInputs(pane)
  assertEquals(inputs.some((input) => input.id === "key:}"), false)
})

Deno.test("pr_verify: detectPromptInputs ignores HUD stat tokens like 0(W)", () => {
  const pane = [
    "Stam : █████   Speed: 100     Move : 0(W)",
    "Str  : 10      Dex  : 11      Power: --",
    "Int  : 9       Per  : 8       Safe : On",
  ].join("\n")
  const inputs = detectPromptInputs(pane)
  assertEquals(inputs.some((input) => input.id === "key:W"), false)
})

Deno.test("pr_verify: detectUiMode classifies gameplay and overmap panes", () => {
  assertEquals(
    detectUiMode("Press } to open sidebar options\nNW: North: NE:\nWest: East:"),
    "gameplay",
  )
  assertEquals(
    detectUiMode("[MOTD] [New Game] [Load] [World]\nPlay Now! (Default Scenario)"),
    "main_menu",
  )
  assertEquals(
    detectUiMode("Press } to open sidebar options\nMAIN MENU\nSave and Quit"),
    "in_game_menu",
  )
  assertEquals(
    detectUiMode("Use movement keys to pan.\nPress W to preview route."),
    "overmap",
  )
})

Deno.test("pr_verify: detectUiMode classifies prompts and loading", () => {
  assertEquals(detectUiMode("Examine where? (Direction button)"), "direction_prompt")
  assertEquals(detectUiMode("You can't do that!  Press [ESC]!"), "modal_prompt")
  assertEquals(detectUiMode("PICK Wgt 5.6/37.0\n[BACKTAB] Prev"), "modal_prompt")
  assertEquals(detectUiMode("Loading files"), "loading")
})

Deno.test("pr_verify: listAvailableInputs merges prompt and catalog options", () => {
  const pane = "Press any key to continue"
  const inputs = listAvailableInputs(pane, true)
  assertEquals(inputs.some((input) => input.id === "key:Space"), true)
  assertEquals(inputs.some((input) => input.id === "key:k"), false)
})

Deno.test({
  name: "pr_verify: writeIndex writes captures and cast block",
  async fn() {
    const readPermission = await Deno.permissions.query({ name: "read" })
    const writePermission = await Deno.permissions.query({ name: "write" })
    if (readPermission.state !== "granted" || writePermission.state !== "granted") {
      return
    }

    const tempDir = await Deno.makeTempDir({ prefix: "pr_verify_index_test_" })
    const indexPath = join(tempDir, "index.md")

    try {
      await writeIndex({
        outputPath: indexPath,
        sessionName: "live-curses-mcp-session",
        captures: [
          {
            id: "loaded",
            caption: "Gameplay screen after load",
            text_file: "artifacts/pr-verify/123/captures/01-loaded.txt",
            code_block_file: "artifacts/pr-verify/123/captures/01-loaded.md",
            screenshot_file: "artifacts/pr-verify/123/captures/01-loaded.webp",
          },
        ],
        castFile: "artifacts/pr-verify/123/session.cast",
      })

      const rendered = await Deno.readTextFile(indexPath)
      assertStringIncludes(rendered, "# PR Verify Artifact")
      assertStringIncludes(rendered, "Session: live-curses-mcp-session")
      assertStringIncludes(rendered, "Gameplay screen after load")
      assertStringIncludes(rendered, "code block")
      assertStringIncludes(rendered, "session.cast")
    } finally {
      await Deno.remove(tempDir, { recursive: true })
    }
  },
})

Deno.test({
  name: "pr_verify: writeCodeBlockCapture creates fenced text block",
  async fn() {
    const readPermission = await Deno.permissions.query({ name: "read" })
    const writePermission = await Deno.permissions.query({ name: "write" })
    if (readPermission.state !== "granted" || writePermission.state !== "granted") {
      return
    }

    const tempDir = await Deno.makeTempDir({ prefix: "pr_verify_code_block_test_" })
    const outputPath = join(tempDir, "capture.md")

    try {
      await writeCodeBlockCapture(outputPath, "line 1\nline 2")
      const rendered = await Deno.readTextFile(outputPath)
      assertStringIncludes(rendered, "```text")
      assertStringIncludes(rendered, "line 1")
      assertStringIncludes(rendered, "line 2")
      assertStringIncludes(rendered, "```")
    } finally {
      await Deno.remove(tempDir, { recursive: true })
    }
  },
})
