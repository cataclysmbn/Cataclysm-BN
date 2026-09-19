import { assertEquals } from "@std/assert"
import { join } from "@std/path"

import { getAllMods } from "../build-scripts/get_all_mods.ts"

Deno.test("getAllMods discovers JSONC metadata and preserves dependency selection order", async () => {
  const root = await Deno.makeTempDir()
  const modsRoot = join(root, "data/mods")
  try {
    await Deno.mkdir(join(modsRoot, "dependent"), { recursive: true })
    await Deno.mkdir(join(modsRoot, "base"), { recursive: true })
    await Deno.mkdir(join(modsRoot, "blacklisted"), { recursive: true })
    await Deno.writeTextFile(
      join(modsRoot, "dependent/modinfo.jsonc"),
      '[\n  // JSONC metadata\n  { "type": "MOD_INFO", "id": "dependent", "dependencies": ["base"], },\n]',
    )
    await Deno.writeTextFile(
      join(modsRoot, "base/modinfo.json"),
      '[{ "type": "MOD_INFO", "id": "base" }]',
    )
    await Deno.writeTextFile(
      join(modsRoot, "blacklisted/modinfo.jsonc"),
      '[{ "type": "MOD_INFO", "id": "blacklisted" }]',
    )

    assertEquals(await getAllMods(modsRoot, new Set(["blacklisted"])), ["base", "dependent"])
    assertEquals(await getAllMods(modsRoot, new Set(["base", "blacklisted"])), [])
    await Deno.writeTextFile(join(root, "blacklist"), "base\r\nblacklisted\r\n")
    const result = await new Deno.Command(Deno.execPath(), {
      args: [
        "run",
        "--allow-read",
        new URL("../build-scripts/get_all_mods.ts", import.meta.url).href,
        "blacklist",
      ],
      cwd: root,
      stdout: "piped",
      stderr: "piped",
    }).output()
    assertEquals(result.code, 0, new TextDecoder().decode(result.stderr))
    assertEquals(new TextDecoder().decode(result.stdout), "\n")
  } finally {
    await Deno.remove(root, { recursive: true })
  }
})
