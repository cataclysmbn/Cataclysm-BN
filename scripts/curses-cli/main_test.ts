import { assertEquals, assertRejects, assertStringIncludes } from "@std/assert"
import { join } from "@std/path"
import {
  buildLaunchCommand,
  detectPromptInputs,
  listAvailableInputs,
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
  const withoutWorld = buildLaunchCommand("/tmp/cata", "/tmp/user", "")
  assertStringIncludes(withoutWorld, "TERM=xterm-256color")
  assertStringIncludes(withoutWorld, "COLORTERM=truecolor")
  assertStringIncludes(withoutWorld, "--basepath")
  assertStringIncludes(withoutWorld, "--userdir")

  const withWorld = buildLaunchCommand("/tmp/cata", "/tmp/user", "fixture_world")
  assertStringIncludes(withWorld, "--world")
  assertStringIncludes(withWorld, "fixture_world")

  const withSeed = buildLaunchCommand("/tmp/cata", "/tmp/user", "", "seed-01")
  assertStringIncludes(withSeed, "--seed")
  assertStringIncludes(withSeed, "seed-01")

  const withAvailableKeysJson = buildLaunchCommand(
    "/tmp/cata",
    "/tmp/user",
    "",
    "",
    "/tmp/available_keys.json",
  )
  assertStringIncludes(withAvailableKeysJson, "CATA_AVAILABLE_KEYS_JSON")
  assertStringIncludes(withAvailableKeysJson, "/tmp/available_keys.json")
})

Deno.test("pr_verify: resolveInputKey handles action ids and raw key ids", () => {
  assertEquals(resolveInputKey("key:!"), "!")
  assertEquals(resolveInputKey("Escape"), "Escape")
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

Deno.test("pr_verify: listAvailableInputs merges prompt and catalog options", () => {
  const pane = "Press any key to continue"
  const inputs = listAvailableInputs(pane, true)
  assertEquals(inputs.some((input) => input.id === "key:Space"), true)
  assertEquals(inputs.some((input) => input.id === "key:k"), false)
})

Deno.test({
  name: "pr_verify: writeIndex writes captures and failure block",
  async fn() {
    const readPermission = await Deno.permissions.query({ name: "read" })
    const writePermission = await Deno.permissions.query({ name: "write" })
    if (readPermission.state !== "granted" || writePermission.state !== "granted") {
      return
    }

    const tempDir = await Deno.makeTempDir({ prefix: "pr_verify_index_test_" })
    const indexPath = join(tempDir, "index.md")

    try {
      await writeIndex(
        indexPath,
        "live-curses-mcp-session",
        [
          {
            id: "loaded",
            caption: "Gameplay screen after load",
            text_file: "artifacts/pr-verify/123/captures/01-loaded.txt",
            code_block_file: "artifacts/pr-verify/123/captures/01-loaded.md",
            screenshot_file: "artifacts/pr-verify/123/captures/01-loaded.webp",
          },
        ],
        "failed",
        "artifacts/pr-verify/123/session.cast",
        "timeout waiting for text",
      )

      const rendered = await Deno.readTextFile(indexPath)
      assertStringIncludes(rendered, "# PR Verify Artifact")
      assertStringIncludes(rendered, "Session: live-curses-mcp-session")
      assertStringIncludes(rendered, "Status: failed")
      assertStringIncludes(rendered, "Gameplay screen after load")
      assertStringIncludes(rendered, "timeout waiting for text")
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
