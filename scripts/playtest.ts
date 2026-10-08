#!/usr/bin/env -S deno run --allow-read --allow-write --allow-run --allow-env

/**
 * @module
 * Download and launch a PR's latest tiles build in an isolated temporary directory.
 * Requires authenticated gh, plus tar (Linux), hdiutil/ditto (macOS), or adb (Android).
 */
import { Command } from "@cliffy/command"
import { exists, walk } from "@std/fs"
import { dirname, join, resolve } from "@std/path"
import * as v from "@valibot/valibot"

const repo = "cataclysmbn/Cataclysm-BN"
const pullSchema = v.object({ number: v.number(), headRefOid: v.string(), title: v.string() })
const artifactSchema = v.object({
  id: v.number(),
  name: v.string(),
  expired: v.boolean(),
  workflow_run: v.object({ id: v.number(), head_sha: v.string() }),
})
type Pull = v.InferOutput<typeof pullSchema>
type Artifact = v.InferOutput<typeof artifactSchema>
export type Platform = "linux" | "windows" | "macos" | "android"
export type Runner = (
  command: string,
  args: string[],
  cwd?: string,
  inherit?: boolean,
) => Promise<string>

export const run: Runner = async (command, args, cwd, inherit = false) => {
  const result = await new Deno.Command(command, {
    args,
    cwd,
    stdout: inherit ? "inherit" : "piped",
    stderr: "inherit",
  }).output()
  if (!result.success) throw new Error(`${command} failed (exit ${result.code})`)
  return inherit ? "" : new TextDecoder().decode(result.stdout).trim()
}

export const platformFor = (os: string): Platform => {
  switch (os.toLowerCase()) {
    case "linux":
    case "windows":
    case "android":
      return os.toLowerCase() as Platform
    case "mac":
    case "macos":
    case "osx":
    case "darwin":
      return "macos"
    default:
      throw new Error(`Unsupported OS: ${os}. Use linux, windows, macos, or android.`)
  }
}

export const artifactName = (pr: number, platform: Platform, arch: string): string => {
  const builds = {
    linux: "linux-tiles-x64",
    windows: "windows-tiles-x64-msvc",
    macos: `osx-tiles-${arch === "aarch64" ? "arm" : "x64"}`,
    android: "android-x64",
  }
  return `${builds[platform]}-pr-${pr}`
}

export const resolvePull = async (selector: string, execute: Runner): Promise<Pull> => {
  try {
    return v.parse(
      pullSchema,
      JSON.parse(
        await execute("gh", [
          "pr",
          "view",
          selector.replace(/^#(?=\d+$)/, ""),
          "--repo",
          repo,
          "--json",
          "number,headRefOid,title",
        ]),
      ),
    )
  } catch {
    const pulls = v.parse(
      v.array(pullSchema),
      JSON.parse(
        await execute("gh", [
          "pr",
          "list",
          "--repo",
          repo,
          "--state",
          "all",
          "--search",
          selector,
          "--limit",
          "100",
          "--json",
          "number,headRefOid,title",
        ]),
      ),
    )
    const matches = pulls.filter((pull) => pull.title === selector)
    if (matches.length !== 1) {
      throw new Error(
        `PR name must match exactly one title; use a PR number, URL, or branch instead.`,
      )
    }
    return matches[0]
  }
}

export const selectArtifact = (artifacts: Artifact[], pull: Pull, name: string): Artifact => {
  const matching = artifacts.filter((artifact) =>
    artifact.name === name && !artifact.expired &&
    artifact.workflow_run.head_sha === pull.headRefOid
  ).toSorted((a, b) => b.id - a.id)
  if (!matching.length) {
    throw new Error(
      `No unexpired ${name} artifact for ${pull.headRefOid}. Check the PR's matrix build; it may be pending, skipped, failed, or expired.`,
    )
  }
  return matching[0]
}

export const validateArchivePaths = (listing: string): void => {
  const unsafe = listing.split("\n").find((path) =>
    path.startsWith("/") || path.split("/").includes("..")
  )
  if (unsafe !== undefined) throw new Error(`Unsafe archive path: ${unsafe}`)
}

const findOne = async (root: string, matches: (entry: string) => boolean): Promise<string> => {
  const paths = await Array.fromAsync(
    walk(root, { includeDirs: false }),
    (entry) => entry,
  )
  const found = paths.filter((entry) => matches(entry.name)).map((entry) => entry.path)
  if (found.length !== 1) {
    throw new Error(`Expected one game or package in ${root}, found ${found.length}`)
  }
  return found[0]
}

export const playtest = async (
  selector: string,
  os: string = Deno.build.os,
  execute: Runner = run,
  root = Deno.build.os === "windows"
    ? join(Deno.env.get("TEMP") ?? Deno.cwd(), "cataclysm-bn", "artifacts")
    : "/tmp/cataclysm-bn/artifacts",
): Promise<void> => {
  const platform = platformFor(os)
  const host = platformFor(Deno.build.os)
  if (platform !== host && platform !== "android") {
    throw new Error(
      `Cannot launch ${platform} builds on ${host}. Run this command on the target OS.`,
    )
  }
  if (platform === "linux" && Deno.build.arch !== "x86_64") {
    throw new Error("The Linux PR artifact requires an x86_64 host.")
  }
  const pull = await resolvePull(selector, execute)
  const name = artifactName(pull.number, platform, Deno.build.arch)
  const pages = v.parse(
    v.array(v.object({ artifacts: v.array(artifactSchema) })),
    JSON.parse(
      await execute("gh", [
        "api",
        `repos/${repo}/actions/artifacts?name=${name}&per_page=100`,
        "--paginate",
        "--slurp",
      ]),
    ),
  )
  const artifact = selectArtifact(pages.flatMap((page) => page.artifacts), pull, name)
  const directory = resolve(root, `PR-${pull.number}-${pull.headRefOid}`, platform)
  await Deno.mkdir(directory, { recursive: true })
  // A fresh staging directory prevents partial downloads from being reused as complete builds.
  const cached = join(directory, `build-${artifact.id}`)
  let staging = cached
  if (!await exists(cached)) {
    staging = await Deno.makeTempDir({ dir: directory, prefix: "download-" })
    console.log(`Downloading ${name} to ${staging}. PR artifacts execute untrusted code.`)
    try {
      await execute("gh", [
        "run",
        "download",
        String(artifact.workflow_run.id),
        "--repo",
        repo,
        "--name",
        name,
        "--dir",
        staging,
      ])
      if (platform === "linux") {
        const archive = await findOne(staging, (name) => name.endsWith(".tar.gz"))
        validateArchivePaths(await execute("tar", ["-tzf", archive]))
        await execute("tar", [
          "-xzf",
          archive,
          "-C",
          staging,
          "--keep-old-files",
          "--no-same-owner",
          "--no-same-permissions",
        ])
      } else if (platform === "macos") {
        const dmg = await findOne(staging, (name) => name.endsWith(".dmg"))
        const mount = join(staging, "mount")
        await Deno.mkdir(mount)
        await execute("hdiutil", ["attach", dmg, "-nobrowse", "-readonly", "-mountpoint", mount])
        try {
          const apps = []
          for await (const entry of Deno.readDir(mount)) {
            if (entry.isDirectory && entry.name.endsWith(".app")) apps.push(entry.name)
          }
          if (apps.length !== 1) throw new Error("Expected one app in the disk image")
          await execute("ditto", [join(mount, apps[0]), join(staging, apps[0])])
        } finally {
          await execute("hdiutil", ["detach", mount])
        }
      }
      await Deno.rename(staging, cached)
      staging = cached
    } catch (error) {
      await Deno.remove(staging, { recursive: true })
      throw error
    }
  }
  // Keep this directory (including saves/config) after the game exits.
  if (platform === "android") {
    const apk = await findOne(staging, (name) => name.endsWith(".apk"))
    await execute("adb", ["install", "-r", apk], undefined, true)
    await execute(
      "adb",
      ["shell", "monkey", "-p", "com.cataclysmbnteam.cataclysmbn.experimental", "1"],
      undefined,
      true,
    )
  } else if (platform === "macos") {
    const apps = []
    for await (const entry of Deno.readDir(staging)) {
      if (entry.isDirectory && entry.name.endsWith(".app")) apps.push(join(staging, entry.name))
    }
    await execute("open", ["-W", "-a", apps[0]], undefined, true)
  } else {
    const binary = await findOne(
      staging,
      (name) => name === (platform === "windows" ? "cataclysm-bn-tiles.exe" : "cataclysm-bn-tiles"),
    )
    if (platform === "linux") await Deno.chmod(binary, 0o755)
    console.log(`Launching ${binary}`)
    await execute(binary, [], dirname(binary), true)
  }
}

export const createPlaytestCommand = (launch = playtest) =>
  new Command()
    .name("just playtest")
    .description(
      "Download and launch a PR tiles build. Requires gh auth login; PR code is untrusted.",
    )
    .option("--os <os:string>", "Target OS: linux, windows, macos, or android (default: host).")
    .arguments("<pr:string>")
    .action(async ({ os }, pr) => await launch(pr, os))

if (import.meta.main) {
  try {
    await createPlaytestCommand().parse(Deno.args)
  } catch (error) {
    console.error(error instanceof Error ? error.message : String(error))
    Deno.exit(1)
  }
}
