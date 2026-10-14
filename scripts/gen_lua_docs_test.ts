import { assertEquals } from "@std/assert"
import { luaDocArgs } from "./gen_lua_docs.ts"

Deno.test("lua docs are generated in a profile of their own", () => {
  assertEquals(luaDocArgs("/tmp/profile", "lua.md", "types.lua"), [
    "--userdir",
    "/tmp/profile/",
    "--configdir",
    "/tmp/profile/config/",
    "--lua-doc",
    "lua.md",
    "--lua-types",
    "types.lua",
  ])
})
