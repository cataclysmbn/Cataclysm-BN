import { assertEquals, assertRejects, assertThrows } from "@std/assert"
import { join } from "@std/path"
import {
  artifactName,
  createPlaytestCommand,
  platformFor,
  playtest,
  resolvePull,
  run,
  type Runner,
  selectArtifact,
  validateArchivePaths,
} from "./playtest.ts"

const pull = { number: 123, headRefOid: "a".repeat(40), title: "Test PR" }
const artifact = {
  id: 10,
  name: "linux-tiles-x64-pr-123",
  expired: false,
  workflow_run: { id: 20, head_sha: pull.headRefOid },
}

Deno.test("playtest CLI forwards --os and PR URL, defaulting OS to the host", async () => {
  const url = "https://github.com/cataclysmbn/Cataclysm-BN/pull/123"
  const calls: (string | undefined)[][] = []
  const launch = (pr: string, os?: string): Promise<void> => {
    calls.push([pr, os])
    return Promise.resolve()
  }
  await createPlaytestCommand(launch).parse(["--os", "linux", url])
  await createPlaytestCommand(launch).parse([url])
  assertEquals(calls, [[url, "linux"], [url, undefined]])
})

Deno.test("playtest selects the newest unexpired artifact for the exact PR head", () => {
  const artifacts = [
    artifact,
    { ...artifact, id: 11 },
    { ...artifact, id: 12, expired: true },
    { ...artifact, id: 13, workflow_run: { id: 21, head_sha: "b".repeat(40) } },
    { ...artifact, id: 14, name: "linux-tiles-x64-pr-124" },
  ]
  assertEquals(selectArtifact(artifacts, pull, artifact.name).id, 11)
  assertThrows(() => selectArtifact(artifacts, { ...pull, headRefOid: "missing" }, artifact.name))
})

Deno.test("playtest maps OS aliases and macOS architecture to tiles artifacts", () => {
  assertEquals(platformFor("Darwin"), "macos")
  assertEquals(platformFor("osx"), "macos")
  assertEquals(artifactName(123, "macos", "aarch64"), "osx-tiles-arm-pr-123")
  assertEquals(artifactName(123, "macos", "x86_64"), "osx-tiles-x64-pr-123")
  assertEquals(artifactName(123, "windows", "x86_64"), "windows-tiles-x64-msvc-pr-123")
  assertEquals(artifactName(123, "android", "aarch64"), "android-x64-pr-123")
  assertThrows(() => platformFor("unknown"))
})

Deno.test("playtest rejects absolute and parent-traversing archive paths", () => {
  validateArchivePaths("cataclysmbn-pr-123/\ncataclysmbn-pr-123/cataclysm-bn-tiles")
  assertThrows(() => validateArchivePaths("/home/user/file"), Error, "Unsafe archive path")
  assertThrows(() => validateArchivePaths("game/../../file"), Error, "Unsafe archive path")
})

Deno.test("playtest resolves a PR number without title search", async () => {
  const execute: Runner = (_command, args) => {
    assertEquals(args.slice(0, 3), ["pr", "view", "123"])
    return Promise.resolve(JSON.stringify(pull))
  }
  assertEquals(await resolvePull("#123", execute), pull)
})

Deno.test("playtest title lookup rejects ambiguous and inexact matches", async () => {
  const execute = (pulls: typeof pull[]): Runner => (_command, args) => {
    if (args[1] === "view") return Promise.reject(new Error("Not a branch"))
    assertEquals(args[1], "list")
    return Promise.resolve(JSON.stringify(pulls))
  }
  assertEquals(await resolvePull(pull.title, execute([pull])), pull)
  await assertRejects(() => resolvePull(pull.title, execute([pull, { ...pull, number: 124 }])))
  await assertRejects(() => resolvePull("Test", execute([pull])))
})

Deno.test({
  name: "playtest downloads, extracts, launches with game cwd, and preserves cached saves",
  ignore: Deno.build.os !== "linux" || Deno.build.arch !== "x86_64",
  fn: async () => {
    const root = await Deno.makeTempDir()
    try {
      const source = join(root, "source")
      const game = join(source, "cataclysmbn-pr-123")
      await Deno.mkdir(game, { recursive: true })
      await Deno.writeTextFile(join(game, "cataclysm-bn-tiles"), "#!/bin/sh\npwd > launched\n")
      const archive = join(root, "game.tar.gz")
      await run("tar", ["-czf", archive, "-C", source, "cataclysmbn-pr-123"])
      let downloads = 0
      let binary = ""
      const execute: Runner = async (command, args, cwd, inherit) => {
        if (command === "gh") {
          if (args[0] === "pr") return JSON.stringify(pull)
          if (args[0] === "api") return JSON.stringify([{ artifacts: [artifact] }])
          assertEquals(args.slice(0, 3), ["run", "download", "20"])
          const dir = args[args.indexOf("--dir") + 1]
          await Deno.copyFile(archive, join(dir, "game.tar.gz"))
          downloads++
          return ""
        }
        if (command !== "tar") binary = command
        return await run(command, args, cwd, inherit)
      }
      await playtest("123", "linux", execute, root)
      assertEquals(downloads, 1)
      const expected = join(
        root,
        `PR-123-${pull.headRefOid}`,
        "linux",
        "build-10",
        "cataclysmbn-pr-123",
      )
      assertEquals(await Deno.readTextFile(join(expected, "launched")), `${expected}\n`)
      assertEquals(binary, join(expected, "cataclysm-bn-tiles"))
      await Deno.writeTextFile(join(expected, "save"), "keep me")
      await playtest("123", "linux", execute, root)
      assertEquals(downloads, 1)
      assertEquals(await Deno.readTextFile(join(expected, "save")), "keep me")
    } finally {
      await Deno.remove(root, { recursive: true })
    }
  },
})

Deno.test({
  name: "playtest removes incomplete downloads and retries instead of caching them",
  ignore: Deno.build.os !== "linux" || Deno.build.arch !== "x86_64",
  fn: async () => {
    const root = await Deno.makeTempDir()
    try {
      let downloads = 0
      const execute: Runner = (command, args) => {
        assertEquals(command, "gh")
        if (args[0] === "pr") return Promise.resolve(JSON.stringify(pull))
        if (args[0] === "api") return Promise.resolve(JSON.stringify([{ artifacts: [artifact] }]))
        downloads++
        return Promise.reject(new Error("Download failed"))
      }
      await assertRejects(() => playtest("123", "linux", execute, root), Error, "Download failed")
      await assertRejects(() => playtest("123", "linux", execute, root), Error, "Download failed")
      assertEquals(downloads, 2)
      const directory = join(root, `PR-123-${pull.headRefOid}`, "linux")
      assertEquals(await Array.fromAsync(Deno.readDir(directory)), [])
    } finally {
      await Deno.remove(root, { recursive: true })
    }
  },
})
