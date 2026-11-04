#!/usr/bin/env -S deno run --allow-run --allow-env --allow-write

/**
 * Generates the Lua reference and annotations with the game executable.
 *
 * The game saves its options at startup, so it runs against a throwaway profile instead of
 * rewriting the options of whoever builds it.
 */

/** Arguments that keep the game's user and config directories inside `profile`. */
export const isolatedProfileArgs = (profile: string): string[] => [
  "--userdir",
  `${profile}/`,
  "--configdir",
  `${profile}/config/`,
]

export const luaDocArgs = (profile: string, reference: string, annotations: string): string[] => [
  ...isolatedProfileArgs(profile),
  "--lua-doc",
  reference,
  "--lua-types",
  annotations,
]

if (import.meta.main) {
  const exe = Deno.env.get("CATA_EXE") ?? "./cataclysm-bn-tiles"
  const profile = await Deno.makeTempDir({ prefix: "cata-docs-" })
  try {
    const { success, code } = await new Deno.Command(exe, {
      args: luaDocArgs(profile, "docs/en/mod/lua/reference/lua.md", "lua_annotations.lua"),
      stdout: "inherit",
      stderr: "inherit",
    }).output()
    if (!success) Deno.exit(code)
  } finally {
    await Deno.remove(profile, { recursive: true })
  }
}
