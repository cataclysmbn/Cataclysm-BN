#!/usr/bin/env -S deno test --allow-read --allow-write=out --allow-run=/usr/bin/bash,/usr/bin/git --allow-env
/**
 * @module
 * Native integration tests for the game-data JSON formatter's routing contract.
 *
 * Run on Linux with the permissions in the shebang, for example:
 * deno test --allow-read --allow-write=out --allow-run=/usr/bin/bash,/usr/bin/git \
 *   --allow-env scripts/format_json_test.ts --filter 'format-json:'
 *
 * Tests without those permissions are ignored (the existing autofix Deno test lane
 * does not allow writing or subprocesses). All fixtures, build outputs and profiles
 * live in a disposable repository under out/; no staged files or real data are used.
 */
import { assertEquals } from "@std/assert"
import { copy, ensureDir, exists } from "@std/fs"
import { dirname, fromFileUrl, join } from "@std/path"

const repoRoot = fromFileUrl(new URL("../", import.meta.url))
const outDir = join(repoRoot, "out")
const encoder = new TextEncoder()
const decoder = new TextDecoder()
const permissions: Deno.PermissionDescriptor[] = [
  { name: "read", path: repoRoot },
  { name: "write", path: outDir },
  { name: "run", command: "/usr/bin/bash" },
  { name: "run", command: "/usr/bin/git" },
  { name: "env" },
]
const enabled = Deno.build.os === "linux" &&
  (await Promise.all(permissions.map((permission) => Deno.permissions.query(permission))))
    .every(({ state }) => state === "granted")

interface Sandbox {
  root: string
  buildDir: string
  env: Record<string, string>
}

const runCommand = async (options: {
  command: string
  args: string[]
  cwd: string
  env: Record<string, string>
}): Promise<Deno.CommandOutput> => {
  const output = await new Deno.Command(options.command, {
    args: options.args,
    cwd: options.cwd,
    env: options.env,
    clearEnv: true,
    stdout: "piped",
    stderr: "piped",
  }).output()
  console.log(`${options.command} ${options.args.join(" ")} => ${output.code}`)
  console.log(decoder.decode(output.stdout) + decoder.decode(output.stderr))
  return output
}

const withRepository = async (fn: (sandbox: Sandbox) => Promise<void>): Promise<void> => {
  await ensureDir(outDir)
  const privateDir = await Deno.makeTempDir({ dir: outDir, prefix: "format-json-test-" })
  try {
    const root = join(privateDir, "repo")
    const profile = join(privateDir, "profile")
    await ensureDir(profile)
    const buildDir = join(root, "out/build/json-format")
    const env = {
      // Pin system Ninja rather than an experimental executable from a personal PATH.
      PATH: "/usr/bin:/bin",
      HOME: profile,
      XDG_CONFIG_HOME: join(profile, "config"),
      XDG_CACHE_HOME: join(profile, "cache"),
      XDG_DATA_HOME: join(profile, "data"),
      XDG_STATE_HOME: join(profile, "state"),
      GIT_CONFIG_NOSYSTEM: "1",
      GIT_CONFIG_GLOBAL: "/dev/null",
      CCACHE_DISABLE: "1",
      CCACHE_DIR: join(profile, "ccache"),
      CMAKE_GENERATOR: "Ninja",
      CMAKE_BUILD_PARALLEL_LEVEL: "2",
      CATA_JSON_FORMAT_BUILD_DIR: buildDir,
      LC_ALL: "C",
    }
    // Clone metadata only: no commits, staging, hooks, checkout or full data corpus.
    const clone = await runCommand({
      command: "/usr/bin/git",
      args: ["clone", "--quiet", "--shared", "--no-checkout", "--template=", repoRoot, root],
      cwd: privateDir,
      env,
    })
    assertEquals(clone.code, 0, "private repository setup must succeed before testing routing")
    // Copy rather than symlink: CMake writes generated headers into its source tree.
    for (
      const path of [
        "CMakeLists.txt",
        "cmake_uninstall.cmake.in",
        "CMakeModules",
        "src",
        "tools/format",
        "lang/CMakeLists.txt",
        "data/CMakeLists.txt",
        "build-scripts/fmt.sh",
        "build-scripts/format-json.sh",
      ]
    ) {
      const destination = join(root, path)
      await ensureDir(dirname(destination))
      await copy(join(repoRoot, path), destination)
    }
    await ensureDir(join(root, "nested cwd"))
    await fn({ root, buildDir, env })
  } finally {
    await Deno.remove(privateDir, { recursive: true })
  }
}

const writeFixture = async (root: string, path: string, text: string): Promise<Uint8Array> => {
  const bytes = encoder.encode(text)
  await ensureDir(dirname(join(root, path)))
  await Deno.writeFile(join(root, path), bytes)
  return bytes
}

const symlinkFixture = async (sandbox: Sandbox, target: string, path: string): Promise<void> => {
  // Create only these private out/ fixtures through the already-authorized Bash boundary.
  const result = await runCommand({
    command: "/usr/bin/bash",
    args: [
      "-c",
      'ln -s -- "$1" "$2"',
      "fixture",
      target,
      join(sandbox.root, path),
    ],
    cwd: sandbox.root,
    env: sandbox.env,
  })
  assertEquals(result.code, 0, "symlink fixture setup must succeed")
}

const format = (sandbox: Sandbox, args: string[]): Promise<Deno.CommandOutput> =>
  runCommand({
    command: "/usr/bin/bash",
    args: [join(sandbox.root, "build-scripts/format-json.sh"), ...args],
    cwd: join(sandbox.root, "nested cwd"),
    env: sandbox.env,
  })

Deno.test({
  name: "format-json: non-game JSON stays byte-exact without a formatter build",
  ignore: !enabled,
  fn: () =>
    withRepository(async (sandbox) => {
      const fixtures = {
        "clients/rust/bn-protocol/tests/fixtures/response.json":
          '{"jsonrpc":"2.0","id":7,"result":null}\r\n',
        "schemas/protocol.json": '{"$schema":"https://json-schema.org/draft/2020-12/schema"}\n',
        "data-extra/settings.json": '{"enabled":true}\n',
      }
      const before = Object.fromEntries(
        await Promise.all(
          Object.entries(fixtures).map(async (
            [path, text],
          ) => [path, await writeFixture(sandbox.root, path, text)]),
        ),
      )
      assertEquals(await exists(sandbox.buildDir), false, "test requires a fresh build directory")
      const result = await format(sandbox, [
        "clients/rust/bn-protocol/tests/fixtures/response.json",
        "./schemas/protocol.json",
        join(sandbox.root, "data-extra/settings.json"),
        "data/../clients/rust/bn-protocol/tests/fixtures/response.json",
      ])
      assertEquals(result.code, 0, "an all-non-game selection must be a successful no-op")
      const after = Object.fromEntries(
        await Promise.all(
          Object.keys(fixtures).map(async (
            path,
          ) => [path, await Deno.readFile(join(sandbox.root, path))]),
        ),
      )
      const buildCreated = await exists(sandbox.buildDir)
      if (buildCreated) {
        const cache = await Deno.readTextFile(join(sandbox.buildDir, "CMakeCache.txt"))
        console.log(cache.split("\n").find((line) => line.startsWith("CMAKE_MAKE_PROGRAM:")))
      }
      assertEquals(
        { files: after, buildCreated },
        { files: before, buildCreated: false },
        "non-game fixtures, schemas and config must not be parsed, rewritten or trigger a build",
      )
    }),
})

Deno.test({
  name: "format-json: ignored paths and an empty explicit selection do not build",
  ignore: !enabled,
  fn: () =>
    withRepository(async (sandbox) => {
      const ignored = ["data/names/fixture.json", ".vscode/settings.json", "data/json/readme.txt"]
      const bytes = encoder.encode("not JSON; ignored inputs must never be parsed\r\n")
      for (const path of ignored) {
        await writeFixture(sandbox.root, path, decoder.decode(bytes))
      }
      await ensureDir(join(sandbox.root, "data/json/directory.json"))
      const nonGame = "clients/protocol/malformed.json"
      await writeFixture(sandbox.root, nonGame, decoder.decode(bytes))
      const outside = join(sandbox.root, "../outside.schema.json")
      await Deno.writeFile(outside, bytes)
      // Resolve both directory and file aliases, but never route excluded targets to the formatter.
      await symlinkFixture(sandbox, join(sandbox.root, "clients/protocol"), "data/json/escape")
      await symlinkFixture(sandbox, join(sandbox.root, "data/names"), "data/json/names-alias")
      await symlinkFixture(sandbox, join(sandbox.root, nonGame), "data/json/fixture-link.json")
      await symlinkFixture(sandbox, "fixture-link.json", "data/json/fixture-chain.json")
      await symlinkFixture(sandbox, join(sandbox.root, ignored[0]), "data/json/names-link.json")
      await symlinkFixture(sandbox, join(sandbox.root, ignored[1]), "data/json/config-link.json")
      await symlinkFixture(sandbox, outside, "data/json/outside-link.json")
      await symlinkFixture(sandbox, "cycle-b.json", "data/json/cycle-a.json")
      await symlinkFixture(sandbox, "cycle-a.json", "data/json/cycle-b.json")
      await symlinkFixture(sandbox, "missing.json", "data/json/dangling.json")
      const result = await format(sandbox, [
        ignored[0],
        `./${ignored[1]}`,
        join(sandbox.root, ignored[2]),
        "data/json/missing.json",
        "data/json/directory.json",
        "data/json/../names/fixture.json",
        "data/json/names-alias/fixture.json",
        "data/json/escape/malformed.json",
        "data/json/fixture-link.json",
        "data/json/fixture-chain.json",
        "data/json/names-link.json",
        "data/json/config-link.json",
        "data/json/outside-link.json",
        "data/json/cycle-a.json",
        "data/json/dangling.json",
        outside,
      ])
      assertEquals(result.code, 0)
      for (const path of [...ignored, nonGame]) {
        assertEquals(await Deno.readFile(join(sandbox.root, path)), bytes)
      }
      assertEquals(await Deno.readFile(outside), bytes)
      assertEquals(await exists(sandbox.buildDir), false)
      const invalidMode = await runCommand({
        command: "/usr/bin/bash",
        args: [join(sandbox.root, "build-scripts/fmt.sh"), "invalid-mode"],
        cwd: sandbox.root,
        env: sandbox.env,
      })
      assertEquals(invalidMode.code, 2, "fmt.sh must retain its usage-error exit status")
      assertEquals(await exists(sandbox.buildDir), false)
    }),
})

Deno.test({
  name: "format-json: game-data paths and no-argument traversal retain native formatting",
  ignore: !enabled,
  fn: () =>
    withRepository(async (sandbox) => {
      const paths = [
        "data/json/relative.json",
        "data/mods/fixture/dot.json",
        "data/help/with space.json",
      ]
      const input = '[{"type":"GENERIC","id":"formatter_scope"}]\n'
      const expected = '[\n  {\n    "type": "GENERIC",\n    "id": "formatter_scope"\n  }\n]\n'
      const untouched = ["data/names/fixture.json", ".vscode/settings.json", "clients/config.json"]
      const linkedPaths = ["data/json/linked target.data", "data/json/chain target"]
      const gamePaths = [...paths, ...linkedPaths]
      for (const path of untouched) {
        await writeFixture(sandbox.root, path, input)
      }
      for (const path of gamePaths) {
        await writeFixture(sandbox.root, path, input)
      }
      await symlinkFixture(sandbox, join(sandbox.root, "data/json"), "game-data-alias")
      await symlinkFixture(sandbox, join(sandbox.root, linkedPaths[0]), "data/json/game-link.json")
      await symlinkFixture(sandbox, "../json/chain target", "data/json/chain-hop")
      await symlinkFixture(sandbox, "../data/json/chain-hop", "clients/chain-link.json")
      await symlinkFixture(
        sandbox,
        join(sandbox.root, untouched[2]),
        "data/json/non-game-link.json",
      )
      const args = [
        paths[0],
        `./${paths[1]}`,
        join(sandbox.root, paths[2]),
        "data/json/../json/relative.json",
        "game-data-alias/relative.json",
        "./data/json/game-link.json",
        join(sandbox.root, "clients/chain-link.json"),
      ]
      const explicit = await runCommand({
        command: "/usr/bin/bash",
        args: [join(sandbox.root, "build-scripts/fmt.sh"), "json", ...args],
        cwd: join(sandbox.root, "nested cwd"),
        env: sandbox.env,
      })
      assertEquals(explicit.code, 0, "native 'needs linting' status 1 remains nonfatal")
      for (const path of gamePaths) {
        assertEquals(await Deno.readTextFile(join(sandbox.root, path)), expected)
      }
      assertEquals(
        (await format(sandbox, args)).code,
        0,
        "already-formatted input remains successful",
      )
      for (const path of gamePaths) {
        await writeFixture(sandbox.root, path, input)
      }
      assertEquals((await format(sandbox, [])).code, 0)
      for (const path of paths) {
        assertEquals(await Deno.readTextFile(join(sandbox.root, path)), expected)
        await writeFixture(sandbox.root, path, input)
      }
      // Only explicit .json aliases select these non-.json target names.
      for (const path of linkedPaths) {
        assertEquals(await Deno.readTextFile(join(sandbox.root, path)), input)
      }
      const all = await runCommand({
        command: "/usr/bin/bash",
        args: [join(sandbox.root, "build-scripts/fmt.sh"), "json", "--all"],
        cwd: join(sandbox.root, "nested cwd"),
        env: sandbox.env,
      })
      assertEquals(all.code, 0)
      for (const path of paths) {
        assertEquals(await Deno.readTextFile(join(sandbox.root, path)), expected)
      }
      for (const path of linkedPaths) {
        assertEquals(await Deno.readTextFile(join(sandbox.root, path)), input)
      }
      for (const path of untouched) {
        assertEquals(await Deno.readFile(join(sandbox.root, path)), encoder.encode(input))
      }
    }),
})
