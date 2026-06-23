#!/usr/bin/env -S deno run --allow-read --allow-write --allow-net

/**
 * @module
 *
 * Bundles pinned Cataclysm: Bright Nights registry mods into release data.
 */

import { Command } from "@cliffy/command"
import { associateBy } from "@std/collections"
import { copy, emptyDir, ensureDir, exists, walk } from "@std/fs"
import { dirname, join } from "@std/path"
import * as v from "@valibot/valibot"
import { zip } from "@deno-library/compress"

const registryUrl = "https://mods.cataclysmbn.org/generated/mods.json"
const headers = { "User-Agent": "Cataclysm-BN-release-bundler" }

const LockSchema = v.array(v.object({
  id: v.string(),
  version: v.string(),
  url: v.pipe(v.string(), v.url()),
}))
const RegistrySchema = v.array(v.looseObject({
  id: v.string(),
  version: v.string(),
  display_name: v.string(),
  package_type: v.optional(v.picklist(["mod", "soundpack"])),
  homepage: v.optional(v.string()),
  license: v.optional(v.string()),
  source: v.looseObject({ extract_path: v.optional(v.string()) }),
}))

type LockEntry = v.InferOutput<typeof LockSchema>[number]
type RegistryEntry = v.InferOutput<typeof RegistrySchema>[number]
type BundleEntry = RegistryEntry & { lock: LockEntry }

type BundleOptions = {
  locks: LockEntry[]
  registry: RegistryEntry[]
  outputRoot: string
  cacheDir?: string
  excludedPackageTypes?: Set<string>
}

const loadJson = async <T>(schema: v.GenericSchema<T>, pathOrUrl: string): Promise<T> => {
  const text = pathOrUrl.startsWith("http")
    ? await (await fetch(pathOrUrl, { headers })).text()
    : await Deno.readTextFile(pathOrUrl)
  return v.parse(schema, JSON.parse(text))
}

const pathParts = (path: string) => path.split(/[\\/]/).filter((part) => part && part !== ".")
const markerFiles = { mod: ["modinfo.json"], soundpack: ["soundpack.txt", "soundpack.json"] }
const packageType = (entry: RegistryEntry) => entry.package_type ?? "mod"
const logEntry = (entry: BundleEntry) =>
  console.log(`${entry.id}: ${entry.display_name} (${packageType(entry)})`)

export const selectEntries = (
  locks: LockEntry[],
  registry: RegistryEntry[],
  excludedPackageTypes = new Set<string>(),
): BundleEntry[] => {
  const registryById = associateBy(registry, (entry) => entry.id)
  return locks.map((lock) => {
    const found = registryById[lock.id]
    if (!found) throw new Error(`${lock.id} is not present in the mod registry`)
    if (found.version !== lock.version) {
      throw new Error(
        `${lock.id} version mismatch: lockfile has ${lock.version}, registry has ${found.version}`,
      )
    }
    return { ...found, lock }
  }).filter((entry) => !excludedPackageTypes.has(packageType(entry)))
}

const targetDir = (entry: BundleEntry, outputRoot: string) =>
  join(
    outputRoot,
    "data",
    packageType(entry) === "soundpack" ? "sound" : "mods",
    entry.source.extract_path ? pathParts(entry.source.extract_path).at(-1)! : entry.id,
  )

const download = async (entry: BundleEntry, cacheDir: string) => {
  const response = await fetch(entry.lock.url, { headers })
  if (!response.ok || !response.body) {
    throw new Error(`failed to download ${entry.id}: ${response.status} ${response.statusText}`)
  }

  const path = join(cacheDir, `${entry.id}.zip`)
  await ensureDir(dirname(path))
  await response.body.pipeTo((await Deno.create(path)).writable)
  return path
}

const findSingleDir = async (root: string) => {
  const dirs = (await Array.fromAsync(Deno.readDir(root)))
    .filter(({ isDirectory }) => isDirectory)
    .map(({ name }) => join(root, name))
  if (dirs.length !== 1) throw new Error("archive has multiple roots")
  return dirs[0]
}

const findExtractDir = async (entry: BundleEntry, root: string) => {
  const extract = pathParts(entry.source.extract_path ?? "")
  if (!extract.length) return await findSingleDir(root)
  for await (const { path } of walk(root, { includeFiles: false })) {
    if (pathParts(path).slice(-extract.length).join("/") === extract.join("/")) return path
  }
  throw new Error(`${entry.id} archive did not contain ${entry.source.extract_path}`)
}

const extract = async (entry: BundleEntry, archive: string, outputRoot: string) => {
  const temp = await Deno.makeTempDir({ prefix: "cbn-registry-unzip-" })
  const target = targetDir(entry, outputRoot)
  try {
    await zip.uncompress(archive, temp)
    await emptyDir(target)
    await copy(await findExtractDir(entry, temp), target, { overwrite: true })
    return target
  } finally {
    await Deno.remove(temp, { recursive: true })
  }
}

const verify = async (entry: BundleEntry, target: string) => {
  const markers = markerFiles[packageType(entry)]
  const found = await Promise.all(markers.map((marker) => exists(join(target, marker))))
  if (!found.some(Boolean)) throw new Error(`${entry.id} did not extract ${markers.join(" or ")}`)
}

const writeCredits = async (entries: BundleEntry[], outputRoot: string) => {
  const path = join(outputRoot, "data", "mods", "external-mods-CREDITS.txt")
  const lines = entries.flatMap((entry) => [
    `${entry.display_name} (${entry.id})`,
    `Source: ${entry.lock.url}`,
    `Homepage: ${entry.homepage ?? "unknown"}`,
    `License: ${entry.license ?? "unknown"}`,
    "",
  ])
  await ensureDir(dirname(path))
  await Deno.writeTextFile(
    path,
    ["Bundled registry mods", "=====================", "", ...lines].join("\n"),
  )
}

export const bundle = async (options: BundleOptions): Promise<BundleEntry[]> => {
  const entries = selectEntries(options.locks, options.registry, options.excludedPackageTypes)
  const cacheDir = options.cacheDir ?? await Deno.makeTempDir({ prefix: "cbn-registry-mods-" })
  try {
    for (const entry of entries) {
      logEntry(entry)
      const target = await extract(entry, await download(entry, cacheDir), options.outputRoot)
      await verify(entry, target)
      console.log(`bundled ${entry.id} -> ${target}`)
    }
    await writeCredits(entries, options.outputRoot)
    return entries
  } finally {
    if (!options.cacheDir) await Deno.remove(cacheDir, { recursive: true })
  }
}

const main = () =>
  new Command()
    .name("bundle-registry-mods")
    .description("Bundle pinned BN mod registry entries into release data.")
    .option("--manifest <path:string>", "External registry lockfile path", {
      default: "data/mods/external.json",
    })
    .option("--registry-url <url:string>", "Registry JSON URL", { default: registryUrl })
    .option("--output-root <path:string>", "Output root to populate", { default: "." })
    .option("--cache-dir <path:string>", "Directory for downloaded archives")
    .option("--exclude-package-type <packageType:string>", "Package type to skip", {
      collect: true,
    })
    .option("--dry-run", "Validate and print selected entries without downloading")
    .action(async ({ manifest, registryUrl, excludePackageType, dryRun, outputRoot, cacheDir }) => {
      const [locks, registry] = await Promise.all([
        loadJson(LockSchema, manifest),
        loadJson(RegistrySchema, registryUrl),
      ])
      const excludedPackageTypes = new Set(excludePackageType ?? [])
      if (dryRun) return selectEntries(locks, registry, excludedPackageTypes).forEach(logEntry)
      await bundle({ locks, registry, outputRoot, cacheDir, excludedPackageTypes })
    })
    .parse(Deno.args)

if (import.meta.main) await main()
