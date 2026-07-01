import { $ } from "@david/dax"
import { assert, assertEquals, assertStringIncludes } from "@std/assert"
import { ensureDir } from "@std/fs"
import { dirname, fromFileUrl, join } from "@std/path"
import { main } from "./reuse_runtime_artifact.ts"

const repoRoot = dirname(dirname(fromFileUrl(import.meta.url)))

const withTempCwd = async (fn: () => Promise<void>): Promise<void> => {
  const cwd = Deno.cwd()
  const dir = await Deno.makeTempDir()
  try {
    Deno.chdir(dir)
    await fn()
  } finally {
    Deno.chdir(cwd)
    await Deno.remove(dir, { recursive: true })
  }
}

const writeFixtureFiles = async (): Promise<void> => {
  await ensureDir("data/raw")
  await Deno.writeTextFile("data/raw/keybindings.json", '{"updated":true}\n')
  await Deno.writeTextFile(
    "files.json",
    JSON.stringify([
      { filename: "data/raw/keybindings.json", status: "modified" },
      { filename: "data/removed.json", status: "removed" },
    ]),
  )
}

Deno.test("reuses a Linux tar artifact and mirrors changed and removed data", async () => {
  await withTempCwd(async () => {
    await ensureDir("base/cataclysmbn-experimental/data/raw")
    await Deno.writeTextFile("base/cataclysmbn-experimental/VERSION.txt", "base\n")
    await Deno.writeTextFile("base/cataclysmbn-experimental/data/raw/keybindings.json", "old\n")
    await Deno.writeTextFile("base/cataclysmbn-experimental/data/removed.json", "remove\n")
    await $`tar -C base -czf linux-base.tar.gz cataclysmbn-experimental`
    await writeFixtureFiles()

    const out = await main({
      platform: "linux",
      artifact: "linux-tiles-x64",
      ext: "tar.gz",
      versionLabel: "pr-test",
      baseSha: "base",
      headSha: "head",
      filesJson: "files.json",
      asset: "linux-base.tar.gz",
    })

    const listing = await $`tar -tzf ${out}`.text()
    assertStringIncludes(listing, "cataclysmbn-pr-test/data/raw/keybindings.json")
    assert(!listing.includes("data/removed.json"))
    assertEquals(
      await $`tar -xOzf ${out} cataclysmbn-pr-test/data/raw/keybindings.json`.text(),
      '{"updated":true}',
    )
  })
})

Deno.test("reuses a Windows release zip whose contents are archived at the root", async () => {
  await withTempCwd(async () => {
    await ensureDir("ziproot/data/raw")
    await Deno.writeTextFile("ziproot/VERSION.txt", "base\n")
    await Deno.writeTextFile("ziproot/data/raw/keybindings.json", "old\n")
    await Deno.writeTextFile("ziproot/data/removed.json", "remove\n")
    await $`zip -qr ../windows-base.zip .`.cwd("ziproot")
    await writeFixtureFiles()

    const out = await main({
      platform: "windows",
      artifact: "windows-tiles-x64-msvc",
      ext: "zip",
      versionLabel: "experimental",
      baseSha: "base",
      headSha: "head",
      filesJson: "files.json",
      asset: "windows-base.zip",
    })

    assertEquals(
      await Deno.readTextFile(join(out, "data/raw/keybindings.json")),
      '{"updated":true}\n',
    )
    assertEquals(await Deno.stat(join(out, "VERSION.txt")).then(() => true), true)
    await Deno.stat(join(out, "data/removed.json")).then(
      () => Promise.reject(new Error("removed data remained in Windows artifact")),
      () => Promise.resolve(),
    )
  })
})

Deno.test({
  name: "reuses the experimental Linux artifact when CATA_TEST_NIGHTLY_ARTIFACTS=1",
  ignore: Deno.env.get("CATA_TEST_NIGHTLY_ARTIFACTS") !== "1",
  fn: async () => {
    await withTempCwd(async () => {
      await ensureDir("data/raw/keybindings")
      await Deno.copyFile(
        join(repoRoot, "data/raw/keybindings/keybindings.json"),
        "data/raw/keybindings/keybindings.json",
      )
      await Deno.writeTextFile(
        "files.json",
        JSON.stringify([{ filename: "data/raw/keybindings/keybindings.json", status: "modified" }]),
      )
      const out = await main({
        platform: "linux",
        artifact: "linux-tiles-x64",
        ext: "tar.gz",
        versionLabel: "pr-test",
        baseSha: "commit",
        headSha: "head",
        filesJson: "files.json",
      })
      assertStringIncludes(
        await $`tar -tzf ${out}`.text(),
        "cataclysmbn-pr-test/data/raw/keybindings/keybindings.json",
      )
    })
  },
})

Deno.test({
  name:
    "reuses and re-signs the experimental Android artifact through the zip library when CATA_TEST_NIGHTLY_ARTIFACTS=1",
  ignore: Deno.env.get("CATA_TEST_NIGHTLY_ARTIFACTS") !== "1",
  fn: async () => {
    await withTempCwd(async () => {
      await ensureDir("data/raw/keybindings")
      await Deno.copyFile(
        join(repoRoot, "data/raw/keybindings/keybindings.json"),
        "data/raw/keybindings/keybindings.json",
      )
      await Deno.writeTextFile(
        "files.json",
        JSON.stringify([
          { filename: "data/raw/keybindings/keybindings.json", status: "modified" },
          { filename: "data/raw/keybindings/messages.json", status: "removed" },
        ]),
      )
      const out = await main({
        platform: "android",
        artifact: "android-x64",
        ext: "apk",
        versionLabel: "experimental",
        baseSha: "commit",
        headSha: "head",
        filesJson: "files.json",
      })
      assertEquals(
        await $`unzip -p ${out} assets/data/raw/keybindings/keybindings.json`.text(),
        await Deno.readTextFile("data/raw/keybindings/keybindings.json").then((text) =>
          text.trim()
        ),
      )
      assert(
        !(await $`unzip -l ${out}`.text()).includes("assets/data/raw/keybindings/messages.json"),
      )
      await $`apksigner verify ${out}`
    })
  },
})
