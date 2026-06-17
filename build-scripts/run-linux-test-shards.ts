#!/usr/bin/env -S deno run --allow-read --allow-write --allow-run --allow-env
/**
 * @module
 *
 * Runs Cataclysm: Bright Nights Linux tests as filename-tag shards.
 *
 * The runner discovers Catch2 filename tags, builds deterministic shards, starts the heaviest shards
 * first, and defaults local runs to the detected CPU count while CI can pass explicit limits.
 */

import { Command } from "@cliffy/command"
import { assertEquals } from "@std/assert"
import { pooledMap } from "@std/async"
import { emptyDir, ensureDir } from "@std/fs"
import { basename, join } from "@std/path"

type Mode = "auto" | "file-tags" | "tiles" | "legacy"

type Options = {
  mode: Mode
  jobs: number | "auto"
  slowShards: number
  nonSlowShards: number | "auto"
  dryRun: boolean
  testBin: string
  testOpts: string[]
}

type Shard = {
  name: string
  filters: string[]
  estimatedSeconds: number
  compute: "cpu" | "gpu_software"
}

type CommandResult = {
  code: number
  stdout: string
  stderr: string
}

const defaultTestBin = "./out/build/linux-full/tests/cata_test-tiles"
const defaultTestOpts = [
  "--min-duration",
  "20",
  "--use-colour",
  "no",
  "--rng-seed",
  "1",
  "--error-format=github-action",
  "--gpu-backend=software",
]

const fixedShards: Shard[] = [
  {
    name: "00-vehicle-rails-basic",
    filters: ["vehicle_rail_movement_basic*"],
    estimatedSeconds: 215,
    compute: "cpu",
  },
  {
    name: "09-vehicle-rails-fork",
    filters: ["vehicle_rail_movement_fork"],
    estimatedSeconds: 180,
    compute: "cpu",
  },
  {
    name: "19-vehicle-rails-shifting",
    filters: ["vehicle_rail_movement_shifting*"],
    estimatedSeconds: 200,
    compute: "cpu",
  },
  {
    name: "20-visibility",
    filters: [
      "[#vision_test] ~[.]",
      "[#shadowcasting_test] ~[.]",
      "[#zlevel_visibility_cache_test] ~[.]",
    ],
    estimatedSeconds: 250,
    compute: "gpu_software",
  },
  {
    name: "21-vehicle-efficiency",
    filters: ["[#vehicle_efficiency_test] ~[.]"],
    estimatedSeconds: 260,
    compute: "cpu",
  },
  {
    name: "22-starting-items",
    filters: ["starting_items"],
    estimatedSeconds: 220,
    compute: "cpu",
  },
  {
    name: "29-vehicle-rails-other",
    filters: ["vehicle_rail_movement_derailed,vehicle_rail_movement_ramp"],
    estimatedSeconds: 120,
    compute: "cpu",
  },
]

const tagWeights = new Map<string, number>([
  ["[#enchantment_test]", 350],
  ["[#vehicle_test]", 150],
  ["[#vehicle_ramp_test]", 100],
  ["[#map_test]", 100],
  ["[#crafting_test]", 120],
  ["[#weather_test]", 80],
  ["[#json_test]", 70],
  ["[#overmap_test]", 55],
  ["[#generic_factory_test]", 35],
  ["[#catalua_test]", 30],
  ["[#vehicle_drag_test]", 18],
])

const specialTagPattern =
  /^\[#(vehicle_(efficiency|rails)|vision|shadowcasting|zlevel_visibility_cache)_test\]$/

const parsePositiveInt = (name: string, value: string): number => {
  if (!/^[1-9][0-9]*$/.test(value)) {
    throw new Error(`${name} must be a positive integer: ${value}`)
  }
  return Number(value)
}

const commandOutput = async (
  command: string,
  args: string[],
  env: Record<string, string> = {},
): Promise<CommandResult> => {
  const result = await new Deno.Command(command, { args, env, stdout: "piped", stderr: "piped" })
    .output()
  const decoder = new TextDecoder()
  return {
    code: result.code,
    stdout: decoder.decode(result.stdout),
    stderr: decoder.decode(result.stderr),
  }
}

const detectCpuCount = async (): Promise<number> => {
  for (const [command, args] of [["nproc", []], ["getconf", ["_NPROCESSORS_ONLN"]]] as const) {
    try {
      const result = await commandOutput(command, [...args])
      if (result.code === 0) {
        return parsePositiveInt(command, result.stdout.trim())
      }
    } catch {
      // Try the next detector.
    }
  }
  return 4
}

const normalizedOptions = async (
  options: Options,
): Promise<Options & { jobs: number; nonSlowShards: number }> => {
  const jobs = options.jobs === "auto" ? await detectCpuCount() : options.jobs
  const nonSlowShards = options.nonSlowShards === "auto"
    ? Math.max(4, Math.min(8, Math.ceil(jobs * 0.75)))
    : options.nonSlowShards
  return { ...options, jobs, nonSlowShards }
}

const ensureFilenameTags = (testOpts: string[]): string[] =>
  testOpts.includes("--filenames-as-tags") ? testOpts : ["--filenames-as-tags", ...testOpts]

export const extractFileTags = (text: string): string[] =>
  [...text.matchAll(/\[#.*?_test\]/g)].map((match) => match[0]).toSorted()
    .filter((tag, index, tags) => index === 0 || tag !== tags[index - 1])

const distributeWorkItems = (items: Shard[], shardCount: number): Shard[] => {
  const bins = Array.from({ length: shardCount }, (_, index) => ({
    name: `${String(index + 1).padStart(2, "0")}-cpu`,
    filters: [] as string[],
    estimatedSeconds: 0,
    compute: "cpu" as const,
  }))
  const sortedItems = items.toSorted((a, b) =>
    b.estimatedSeconds - a.estimatedSeconds || a.name.localeCompare(b.name)
  )
  for (const item of sortedItems) {
    bins.sort((a, b) => a.estimatedSeconds - b.estimatedSeconds || a.name.localeCompare(b.name))
    const bin = bins[0]
    bin.filters.push(...item.filters)
    bin.estimatedSeconds += item.estimatedSeconds
  }
  return bins.filter((bin) => bin.filters.length > 0)
}

const tagWorkItem = (tag: string, prefix: "slow" | "non-slow"): Shard => ({
  name: tag,
  filters: [prefix === "slow" ? `[slow] ~starting_items ${tag}` : `~[slow] ~[.] ${tag}`],
  estimatedSeconds: tagWeights.get(tag) ?? 10,
  compute: "cpu",
})

export const buildShardPlan = (
  allTags: string[],
  slowTags: string[],
  nonSlowShards: number,
  _slowShards: number,
): Shard[] => {
  const visibility = fixedShards.filter((shard) => shard.compute === "gpu_software")
  const cpuFixed = fixedShards.filter((shard) => shard.compute === "cpu")
  const nonSlowTags = allTags.filter((tag) => !specialTagPattern.test(tag))
  const slowItems = slowTags.length > 0 ? slowTags.map((tag) => tagWorkItem(tag, "slow")) : [{
    name: "slow",
    filters: ["[slow] ~starting_items"],
    estimatedSeconds: 30,
    compute: "cpu" as const,
  }]
  const cpuItems = [
    ...cpuFixed,
    ...nonSlowTags.map((tag) => tagWorkItem(tag, "non-slow")),
    ...slowItems,
  ]
  return [
    ...visibility,
    ...distributeWorkItems(cpuItems, nonSlowShards),
  ].toSorted((a, b) => b.estimatedSeconds - a.estimatedSeconds || a.name.localeCompare(b.name))
}

const prepareShardDir = async (): Promise<{ path: string; cleanup: () => Promise<void> }> => {
  const configured = Deno.env.get("CATA_TEST_SHARD_DIR")
  if (configured) {
    await emptyDir(configured)
    return { path: configured, cleanup: async () => {} }
  }
  const path = await Deno.makeTempDir({
    dir: Deno.env.get("RUNNER_TEMP") ?? "/tmp",
    prefix: "cata-test-shards.",
  })
  return { path, cleanup: async () => await Deno.remove(path, { recursive: true }) }
}

const writeShardFiles = async (shardDir: string, shards: Shard[]): Promise<void> => {
  await ensureDir(shardDir)
  await Promise.all(
    shards.map((shard) =>
      Deno.writeTextFile(join(shardDir, shard.name), `${shard.filters.join("\n")}\n`)
    ),
  )
}

const discoverTags = async (
  options: Options & { jobs: number; nonSlowShards: number },
  shardDir: string,
) => {
  const testOpts = ensureFilenameTags(options.testOpts)
  const discoveryEnv = {
    CATA_TEST_COMPUTE_ACCELERATION: Deno.env.get("CATA_TEST_COMPUTE_ACCELERATION") ?? "cpu",
  }
  const all = await commandOutput(options.testBin, [
    ...testOpts,
    `--user-dir=${join(shardDir, "discovery")}`,
    "--list-tags",
  ], discoveryEnv)
  if (all.stderr) {
    console.error(all.stderr)
  }
  const slow = await commandOutput(
    options.testBin,
    [
      ...testOpts,
      `--user-dir=${join(shardDir, "slow-discovery")}`,
      "--list-tags",
      "[slow] ~starting_items",
    ],
    discoveryEnv,
  )
  if (slow.stderr) {
    console.error(slow.stderr)
  }
  return { allTags: extractFileTags(all.stdout), slowTags: extractFileTags(slow.stdout), testOpts }
}

const runShard = async (
  options: Options & { jobs: number; nonSlowShards: number },
  testOpts: string[],
  shard: Shard,
): Promise<number> => {
  const explicitCompute = Deno.env.get("CATA_TEST_COMPUTE_ACCELERATION")
  const compute = explicitCompute ??
    (shard.compute === "gpu_software"
      ? Deno.env.get("CATA_TEST_VISIBILITY_COMPUTE_ACCELERATION") ?? "gpu_software"
      : "cpu")
  const userDirPrefix = Deno.env.get("CATA_TEST_USER_DIR_PREFIX") ?? "test_user_dir"
  const userDir = `${userDirPrefix}_${shard.name}`
  const filter = shard.filters.join(",")
  const start = Date.now()
  console.log(`Starting shard ${shard.name} with ${compute} compute`)
  const result = await commandOutput(options.testBin, [
    ...testOpts,
    `--user-dir=${userDir}`,
    filter,
  ], {
    CATA_TEST_COMPUTE_ACCELERATION: compute,
  })
  if (result.stdout) {
    console.log(result.stdout.trimEnd())
  }
  if (result.stderr) {
    console.error(result.stderr.trimEnd())
  }
  const elapsed = Math.round((Date.now() - start) / 1000)
  console.log(`Finished shard ${shard.name} in ${elapsed}s with status ${result.code}`)
  return result.code
}

const run = async (options: Options): Promise<number> => {
  const parsed = await normalizedOptions(options)
  if (!["auto", "file-tags", "tiles", "legacy"].includes(parsed.mode)) {
    throw new Error(`Unknown shard mode: ${parsed.mode}`)
  }
  const mode = parsed.mode === "legacy" ? "legacy" : "file-tags"
  const shardDir = await prepareShardDir()
  try {
    if (mode === "legacy") {
      const shard: Shard = {
        name: "00-legacy",
        filters: ["[slow] ~starting_items,~[slow] ~[.],starting_items"],
        estimatedSeconds: 1,
        compute: "cpu",
      }
      if (parsed.dryRun) {
        console.log(`${shard.name}: ${shard.filters.join(",")}`)
        return 0
      }
      return await runShard(parsed, parsed.testOpts, shard)
    }

    const { allTags, slowTags, testOpts } = await discoverTags(parsed, shardDir.path)
    const shards = buildShardPlan(allTags, slowTags, parsed.nonSlowShards, parsed.slowShards)
    await writeShardFiles(shardDir.path, shards)
    if (parsed.dryRun) {
      for (const shard of shards.toSorted((a, b) => a.name.localeCompare(b.name))) {
        console.log(`${basename(shard.name)}: ${shard.filters.join(",")}`)
      }
      return 0
    }

    let status = 0
    for await (
      const code of pooledMap(parsed.jobs, shards, (shard) => runShard(parsed, testOpts, shard))
    ) {
      if (code !== 0 && status === 0) {
        status = code
      }
    }
    return status
  } finally {
    await shardDir.cleanup()
  }
}

Deno.test("extractFileTags returns sorted unique file tags", () => {
  assertEquals(extractFileTags("x [#b_test] [foo] [#a_test] [#b_test]"), ["[#a_test]", "[#b_test]"])
})

Deno.test("buildShardPlan keeps visibility out of CPU shards", () => {
  const shards = buildShardPlan(["[#vision_test]", "[#map_test]"], [], 2, 1)
  const cpuFilters = shards.filter((shard) => shard.name.endsWith("cpu")).flatMap((
    shard,
  ) => shard.filters)
  assertEquals(cpuFilters.some((filter) => filter.includes("vision_test")), false)
  assertEquals(cpuFilters.some((filter) => filter.includes("map_test")), true)
})

Deno.test("buildShardPlan keeps visibility on its GPU shard", () => {
  const shards = buildShardPlan(["[#enchantment_test]", "[#tiny_test]"], [], 2, 1)
  const visibility = shards.find((shard) => shard.name === "20-visibility")
  assertEquals(visibility?.compute, "gpu_software")
})

if (import.meta.main) {
  try {
    const { options, args, literal } = await new Command()
      .name("run-linux-test-shards")
      .description("Run Cataclysm: Bright Nights Linux tests as filename-tag shards")
      .option("--mode <mode:string>", "auto, file-tags, tiles, or legacy", { default: "auto" })
      .option("--jobs <jobs:string>", "concurrent shard jobs, or auto", { default: "auto" })
      .option("--slow-shards <slowShards:string>", "accepted for compatibility", { default: "4" })
      .option("--non-slow-shards <nonSlowShards:string>", "generated CPU shards, or auto", {
        default: "auto",
      })
      .option("--dry-run", "print shard filters without running tests")
      .arguments("[testBin:string] [...testOpts:string]")
      .parse(Deno.args)
    const jobs = options.jobs === "auto" ? "auto" : parsePositiveInt("--jobs", options.jobs)
    const nonSlowShards = options.nonSlowShards === "auto"
      ? "auto"
      : parsePositiveInt("--non-slow-shards", options.nonSlowShards)
    const testOpts = [
      ...args.slice(1).filter((arg): arg is string => arg !== undefined),
      ...literal,
    ]
    Deno.exit(
      await run({
        mode: options.mode as Mode,
        jobs,
        slowShards: parsePositiveInt("--slow-shards", options.slowShards),
        nonSlowShards,
        dryRun: Boolean(options.dryRun),
        testBin: args[0] ?? defaultTestBin,
        testOpts: testOpts.length > 0 ? testOpts : defaultTestOpts,
      }),
    )
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error)
    console.error(`Error: ${message}`)
    Deno.exit(1)
  }
}
