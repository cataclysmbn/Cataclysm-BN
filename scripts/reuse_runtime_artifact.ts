#!/usr/bin/env -S deno run --allow-read --allow-write --allow-run --allow-env
/**
 * @module
 * Reuse exact-base release artifacts for PRs that only change runtime data.
 */

import { Command } from "@cliffy/command"
import { $ } from "@david/dax"
import { zip } from "@deno-library/compress"
import { Octokit, type RestEndpointMethodTypes } from "@octokit/rest"
import { copy, ensureDir, exists, walk } from "@std/fs"
import { dirname, join, normalize, relative, SEPARATOR } from "@std/path"
import { TarStream, type TarStreamInput, UntarStream } from "@std/tar"
import * as v from "@valibot/valibot"
import { BlobWriter, Uint8ArrayReader, ZipWriter } from "@zip-js/zip-js"

const allowed = ["data/", "docs/", "scripts/"]
const excluded = ["data/cataicon.ico", "data/shaders/"]
const fileSchema = v.object({
  filename: v.string(),
  previous_filename: v.optional(v.string()),
  status: v.optional(v.string()),
})
type PrFile = v.InferOutput<typeof fileSchema>
type ReleaseAsset =
  RestEndpointMethodTypes["repos"]["getReleaseByTag"]["response"]["data"]["assets"][number]

export type Options = {
  platform?: "linux" | "windows" | "macos" | "android"
  artifact?: string
  ext?: string
  versionLabel?: string
  baseSha?: string
  headSha?: string
  filesJson?: string
  asset?: string
}

const requiredAssets = [
  "cbn-linux-tiles-x64-experimental.tar.gz",
  "cbn-windows-tiles-x64-msvc-experimental.zip",
  "cbn-osx-tiles-x64-experimental.dmg",
  "cbn-android-x64-experimental.apk",
]

const repoInfo = (): { owner: string; repo: string } => {
  const [owner, repo] = (Deno.env.get("GITHUB_REPOSITORY") ?? "cataclysmbn/Cataclysm-BN").split("/")
  if (!owner || !repo) {
    throw new Error("GITHUB_REPOSITORY must be owner/repo")
  }
  return { owner, repo }
}

const github = (): Octokit =>
  new Octokit({ auth: Deno.env.get("GITHUB_TOKEN") ?? Deno.env.get("GH_TOKEN") })

const prFiles = async (options: Options): Promise<PrFile[]> => {
  if (options.filesJson) {
    return v.parse(v.array(fileSchema), JSON.parse(await Deno.readTextFile(options.filesJson)))
  }
  const pullNumber = Number(Deno.env.get("PR_NUMBER"))
  if (!Number.isInteger(pullNumber)) {
    throw new Error("PR_NUMBER must be set")
  }
  const { owner, repo } = repoInfo()
  const octokit = github()
  const files = await octokit.paginate(octokit.rest.pulls.listFiles, {
    owner,
    repo,
    pull_number: pullNumber,
    per_page: 100,
  })
  return v.parse(v.array(fileSchema), files)
}

const reusable = (files: PrFile[]): boolean => {
  const names = files.flatMap((file) => [file.filename, file.previous_filename]).filter((name) =>
    name
  )
  return names.length > 0 &&
    names.every((name) =>
      allowed.some((prefix) => name!.startsWith(prefix)) &&
      !excluded.some((prefix) => name!.startsWith(prefix))
    )
}

const dataChanges = (files: PrFile[]): PrFile[] =>
  files.filter((file) =>
    file.filename.startsWith("data/") || file.previous_filename?.startsWith("data/")
  )

const experimentalRelease = async () => {
  const { owner, repo } = repoInfo()
  return await github().rest.repos.getReleaseByTag({ owner, repo, tag: "experimental" })
}

const releaseAssetBytes = async (asset: ReleaseAsset): Promise<Uint8Array> => {
  const { owner, repo } = repoInfo()
  const response = await github().request("GET /repos/{owner}/{repo}/releases/assets/{asset_id}", {
    owner,
    repo,
    asset_id: asset.id,
    headers: { accept: "application/octet-stream" },
  })
  if (response.data instanceof ArrayBuffer) {
    return new Uint8Array(response.data)
  }
  if (response.data instanceof Uint8Array) {
    return response.data
  }
  throw new Error(`unexpected release asset response for ${asset.name}`)
}

const releaseAsset = async (options: Options): Promise<string> => {
  if (options.asset) {
    return options.asset
  }
  if (!options.artifact || !options.ext) {
    throw new Error("artifact and ext are required")
  }
  const { data: release } = await experimentalRelease()
  if (!release.body?.includes(options.baseSha ?? "")) {
    throw new Error("experimental release does not match PR base")
  }
  const name = `cbn-${options.artifact}-experimental.${options.ext}`
  const asset = release.assets.find((candidate) => candidate.name === name)
  if (!asset) {
    throw new Error(`missing experimental release asset: ${name}`)
  }
  await Deno.writeFile(name, await releaseAssetBytes(asset))
  return name
}

export const canReuseArtifacts = async (options: Options): Promise<boolean> => {
  const files = await prFiles(options)
  if (!reusable(files)) {
    return false
  }
  const { data: release } = await experimentalRelease()
  const names = new Set(release.assets.map((asset) => asset.name))
  return Boolean(options.baseSha && release.body?.includes(options.baseSha)) &&
    requiredAssets.every((name) => names.has(name))
}

const safeEntryPath = (path: string): string => {
  const normalized = normalize(path).replaceAll("\\", "/")
  if (normalized.startsWith("../") || normalized === ".." || normalized.startsWith("/")) {
    throw new Error(`unsafe archive path: ${path}`)
  }
  return normalized
}

const writeVersion = async (root: string, options: Options): Promise<void> => {
  for await (const entry of walk(root, { includeDirs: false, match: [/VERSION\.txt$/] })) {
    await Deno.writeTextFile(
      entry.path,
      [
        `build type: ${options.artifact}`,
        `build number: ${options.versionLabel}`,
        `binary commit sha: ${options.baseSha}`,
        `content commit sha: ${options.headSha}`,
        `content commit url: https://github.com/${
          Deno.env.get("GITHUB_REPOSITORY") ?? "cataclysmbn/Cataclysm-BN"
        }/commit/${options.headSha}`,
        "",
      ].join("\n"),
    )
  }
}

const removePath = async (path: string): Promise<void> => {
  await Deno.remove(path, { recursive: true }).catch((error) => {
    if (!(error instanceof Deno.errors.NotFound)) {
      throw error
    }
  })
}

const overlayTree = async (root: string, prefix: string, changes: PrFile[]): Promise<void> => {
  for (const change of changes) {
    for (
      const old of [
        change.previous_filename,
        change.status === "removed" ? change.filename : undefined,
      ]
    ) {
      if (old) {
        await removePath(join(root, prefix, safeEntryPath(old)))
      }
    }
    if (change.status !== "removed" && change.filename.startsWith("data/")) {
      const dst = join(root, prefix, safeEntryPath(change.filename))
      await ensureDir(dirname(dst))
      await Deno.copyFile(change.filename, dst)
    }
  }
}

const tarOverlay = async (asset: string, options: Options, changes: PrFile[]): Promise<string> => {
  const out = `cbn-${options.artifact}-${options.versionLabel}.tar.gz`
  const root = await Deno.makeTempDir()
  try {
    const file = await Deno.open(asset)
    for await (
      const entry of file.readable
        .pipeThrough(new DecompressionStream("gzip"))
        .pipeThrough(new UntarStream())
    ) {
      const path = join(root, safeEntryPath(entry.path))
      if (entry.header.typeflag === "5") {
        await ensureDir(path)
      } else if (entry.header.typeflag === "2") {
        await ensureDir(dirname(path))
        await Deno.symlink(entry.header.linkname, path)
      } else if (entry.readable) {
        await ensureDir(dirname(path))
        await entry.readable.pipeTo((await Deno.create(path)).writable)
      }
    }

    const source = (await Array.fromAsync(Deno.readDir(root))).find((entry) => entry.isDirectory)
      ?.name
    if (!source) {
      throw new Error("missing root directory in Linux artifact")
    }
    const target = `cataclysmbn-${options.versionLabel}`
    if (source !== target) {
      await Deno.rename(join(root, source), join(root, target))
    }
    await overlayTree(root, `${target}/`, changes)
    await writeVersion(root, options)

    const entries: TarStreamInput[] = []
    for await (
      const entry of walk(join(root, target), { includeDirs: true, followSymlinks: false })
    ) {
      const archivePath = relative(root, entry.path).split(SEPARATOR).join("/")
      const info = await Deno.lstat(entry.path)
      if (info.isDirectory) {
        entries.push({ type: "directory", path: archivePath })
      } else if (info.isSymlink) {
        entries.push({
          type: "symlink",
          path: archivePath,
          linkname: await Deno.readLink(entry.path),
        })
      } else if (info.isFile) {
        entries.push({
          type: "file",
          path: archivePath,
          size: info.size,
          readable: (await Deno.open(entry.path)).readable,
        })
      }
    }
    await ReadableStream.from(entries)
      .pipeThrough(new TarStream())
      .pipeThrough(new CompressionStream("gzip"))
      .pipeTo((await Deno.create(out)).writable)
  } finally {
    await removePath(root)
  }
  return out
}

const writeZipTree = async (out: string, root: string): Promise<void> => {
  const writer = new ZipWriter(new BlobWriter("application/zip"))
  try {
    for await (const entry of walk(root, { includeDirs: false })) {
      const name = relative(root, entry.path).split(SEPARATOR).join("/")
      await writer.add(name, new Uint8ArrayReader(await Deno.readFile(entry.path)))
    }
    await Deno.writeFile(out, new Uint8Array(await (await writer.close()).arrayBuffer()))
  } catch (error) {
    await writer.close().catch(() => undefined)
    throw error
  }
}

const windowsOverlay = async (
  asset: string,
  options: Options,
  changes: PrFile[],
): Promise<string> => {
  const out = `cataclysmbn-${options.versionLabel}`
  await removePath(out)
  await ensureDir(out)
  await zip.uncompress(asset, out)
  await overlayTree(".", `${out}/`, changes)
  await writeVersion(out, options)
  return out
}

const ensureDebugKeystore = async (): Promise<string> => {
  const keystore = join(Deno.env.get("HOME")!, ".android/debug.keystore")
  if (!await exists(keystore)) {
    await ensureDir(dirname(keystore))
    await $`keytool -genkeypair -keystore ${keystore} -storepass android -keypass android -alias androiddebugkey -keyalg RSA -keysize 2048 -validity 10000 -dname ${"CN=Android Debug,O=Android,C=US"}`
  }
  return keystore
}

const apkOverlay = async (asset: string, options: Options, changes: PrFile[]): Promise<string> => {
  const work = await Deno.makeTempDir()
  const root = join(work, "apk")
  const unsigned = join(work, "unsigned.apk")
  const aligned = join(work, "aligned.apk")
  const out = `cbn-${options.artifact}-${options.versionLabel}.apk`
  try {
    await zip.uncompress(asset, root)
    await overlayTree(root, "assets/", changes)
    await writeZipTree(unsigned, root)
    await $`zipalign -f 4 ${unsigned} ${aligned}`
    await $`apksigner sign --ks ${await ensureDebugKeystore()} --ks-pass pass:android --out ${out} ${aligned}`
    await $`apksigner verify ${out}`
  } finally {
    await removePath(work)
  }
  return out
}

const dmgOverlay = async (asset: string, options: Options, changes: PrFile[]): Promise<string> => {
  const out = `cbn-${options.artifact}-${options.versionLabel}.dmg`
  const root = await Deno.makeTempDir()
  const plist = await $`hdiutil attach -nobrowse -readonly -plist ${asset}`.text()
  const mount = plist.match(/<key>mount-point<\/key>\s*<string>([^<]+)<\/string>/)?.[1]
  if (!mount) {
    throw new Error("missing DMG mount point")
  }
  try {
    const app = Array.from(Deno.readDirSync(mount)).find((entry) =>
      entry.isDirectory && entry.name.endsWith(".app")
    )?.name
    if (!app) {
      throw new Error("missing app bundle in DMG")
    }
    await copy(join(mount, app), join(root, app), { overwrite: true })
    await overlayTree(root, `${app}/Contents/Resources/`, changes)
    await writeVersion(root, options)
    await $`hdiutil create -volname ${"Cataclysm BN"} -srcfolder ${
      join(root, app)
    } -ov -format UDZO ${out}`
  } finally {
    await $`hdiutil detach ${mount}`.noThrow()
    await removePath(root)
  }
  return out
}

export const main = async (options: Options): Promise<string> => {
  if (!options.platform || !options.artifact || !options.ext || !options.versionLabel) {
    throw new Error("platform, artifact, ext, and version-label are required")
  }
  const files = await prFiles(options)
  if (!reusable(files)) {
    throw new Error("PR is not data/docs/scripts-only")
  }
  const asset = await releaseAsset(options)
  const changes = dataChanges(files)
  const output = await {
    linux: tarOverlay,
    windows: windowsOverlay,
    android: apkOverlay,
    macos: dmgOverlay,
  }[options.platform](asset, options, changes)
  const githubOutput = Deno.env.get("GITHUB_OUTPUT")
  if (githubOutput) {
    await Deno.writeTextFile(githubOutput, `path=${output}\n`, { append: true })
  }
  return output
}

const writeReuseOutput = async (reuse: boolean): Promise<void> => {
  const githubOutput = Deno.env.get("GITHUB_OUTPUT")
  if (githubOutput) {
    await Deno.writeTextFile(githubOutput, `reuse=${reuse}\n`, { append: true })
  }
  console.log(reuse ? "true" : "false")
}

if (import.meta.main) {
  await new Command()
    .name("reuse-runtime-artifact")
    .description("Merge PR runtime data changes into exact-base release artifacts")
    .option("--platform <platform:string>", "Artifact platform")
    .option("--artifact <artifact:string>", "Artifact name")
    .option("--ext <ext:string>", "Artifact extension")
    .option("--version-label <versionLabel:string>", "Output version label")
    .option("--base-sha <baseSha:string>", "PR base SHA", { default: Deno.env.get("BASE_SHA") })
    .option("--head-sha <headSha:string>", "PR head SHA", { default: Deno.env.get("HEAD_SHA") })
    .option("--files-json <filesJson:string>", "Local PR files fixture")
    .option("--asset <asset:string>", "Local base artifact fixture")
    .option("--check-reuse", "Only check whether exact-base runtime artifacts can be reused")
    .action(async ({ checkReuse, ...options }) => {
      if (checkReuse) {
        await writeReuseOutput(await canReuseArtifacts(options as Options))
        return
      }
      await main(options as Options)
    })
    .parse(Deno.args)
}
