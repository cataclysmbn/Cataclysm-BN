import { assertEquals } from "@std/assert"
import { join } from "@std/path"
import { playtest, type Runner } from "./playtest.ts"

Deno.test("playtest installs the Android PR APK before launching its experimental package", async () => {
  const root = await Deno.makeTempDir()
  try {
    const sha = "a".repeat(40)
    const calls: string[][] = []
    const execute: Runner = async (command, args) => {
      if (command === "gh") {
        if (args[0] === "pr") {
          return JSON.stringify({ number: 123, headRefOid: sha, title: "Test PR" })
        }
        if (args[0] === "api") {
          return JSON.stringify([{
            artifacts: [{
              id: 10,
              name: "android-x64-pr-123",
              expired: false,
              workflow_run: { id: 20, head_sha: sha },
            }],
          }])
        }
        const directory = args[args.indexOf("--dir") + 1]
        await Deno.writeTextFile(join(directory, "game.apk"), "fixture")
        return ""
      }
      assertEquals(command, "adb")
      calls.push(args)
      return ""
    }
    await playtest("123", "android", execute, root)
    assertEquals(calls, [
      ["install", "-r", join(root, `PR-123-${sha}`, "android", "build-10", "game.apk")],
      ["shell", "monkey", "-p", "com.cataclysmbnteam.cataclysmbn.experimental", "1"],
    ])
  } finally {
    await Deno.remove(root, { recursive: true })
  }
})
