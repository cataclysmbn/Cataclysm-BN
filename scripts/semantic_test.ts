import { assertEquals } from "@std/assert"
import { join } from "@std/path"

import { extractModinfo } from "./semantic.ts"

Deno.test("extractModinfo parses JSONC modinfo files", async () => {
  const directory = await Deno.makeTempDir()
  const path = join(directory, "modinfo.jsonc")
  try {
    await Deno.writeTextFile(
      path,
      '[\n  // A JSONC comment\n  { "type": "MOD_INFO", "id": "jsonc_mod", },\n]',
    )
    assertEquals(await extractModinfo(path), "jsonc_mod")
  } finally {
    await Deno.remove(directory, { recursive: true })
  }
})
