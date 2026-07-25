import { assertEquals, assertThrows } from "@std/assert"
import { exists } from "@std/fs"
import { join } from "@std/path"
import { BlobWriter, terminateWorkers, TextReader, ZipWriter } from "@zip-js/zip-js"

import { bundle, selectEntries } from "./bundle-registry-mods.ts"

const zipUrl = async (files: Record<string, string>): Promise<string> => {
  const writer = new ZipWriter(new BlobWriter("application/zip"))
  await Promise.all(
    Object.entries(files).map(([path, text]) => writer.add(path, new TextReader(text))),
  )
  const bytes = new Uint8Array(await (await writer.close()).arrayBuffer())
  const binary = [...bytes].map((byte) => String.fromCharCode(byte)).join("")
  return `data:application/zip;base64,${btoa(binary)}`
}

Deno.test("selectEntries rejects stale lock versions", () => {
  assertThrows(
    () =>
      selectEntries(
        [{ id: "demo", version: "1.0.0", url: "https://example.com/demo.zip" }],
        [{ id: "demo", version: "2.0.0", display_name: "Demo", package_type: "mod", source: {} }],
      ),
    Error,
    "version mismatch",
  )
})

Deno.test("bundle extracts mods, skips excluded soundpacks, and writes credits", async () => {
  const outputRoot = await Deno.makeTempDir()
  try {
    const locks = [
      {
        id: "demo_mod",
        version: "1.0.0",
        url: await zipUrl({ "ArchiveRoot/NestedMod/modinfo.json": "[]" }),
      },
      {
        id: "demo_sound",
        version: "1.0.0",
        url: await zipUrl({ "SoundRoot/soundpack.txt": "NAME" }),
      },
    ]
    const registry = [
      {
        id: "demo_mod",
        version: "1.0.0",
        display_name: "Demo Mod",
        package_type: "mod" as const,
        license: "CC0",
        source: { extract_path: "NestedMod" },
      },
      {
        id: "demo_sound",
        version: "1.0.0",
        display_name: "Demo Sound",
        package_type: "soundpack" as const,
        license: "CC0",
        source: {},
      },
    ]

    const entries = await bundle({
      locks,
      registry,
      outputRoot,
      excludedPackageTypes: new Set(["soundpack"]),
    })

    assertEquals(entries.map((entry) => entry.id), ["demo_mod"])
    assertEquals(await exists(join(outputRoot, "data/mods/NestedMod/modinfo.json")), true)
    assertEquals(await exists(join(outputRoot, "data/sound/SoundRoot/soundpack.txt")), false)
    const credits = await Deno.readTextFile(
      join(outputRoot, "data/mods/external-mods-CREDITS.txt"),
    )
    assertEquals(credits.includes("Demo Mod (demo_mod)"), true)
  } finally {
    await Deno.remove(outputRoot, { recursive: true })
    await terminateWorkers()
  }
})
