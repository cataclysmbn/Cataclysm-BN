import { assertEquals, assertStringIncludes } from "@std/assert"
import PO from "pofile"
import { parse as parseJsonc } from "@std/jsonc"
import { injectJsoncTranslatorComments } from "./extract_json_strings.ts"
import { dirname, fromFileUrl, join } from "@std/path"

const repoRoot = dirname(dirname(fromFileUrl(import.meta.url)))
const fixtureDir = join(repoRoot, "scripts/testdata/extract_json_strings/input")
const expectedSnapshot = join(repoRoot, "scripts/testdata/extract_json_strings/expected.pot")

const decoder = new TextDecoder()

const runDenoExtractor = async (args: string[]) => {
  const result = await new Deno.Command("deno", {
    args: [
      "run",
      "--allow-read",
      "--allow-write",
      "--allow-run",
      "scripts/extract_json_strings.ts",
      ...args,
    ],
    cwd: repoRoot,
    stdout: "piped",
    stderr: "piped",
  }).output()
  const stdout = decoder.decode(result.stdout)
  const stderr = decoder.decode(result.stderr)
  assertEquals(
    result.code,
    0,
    `deno run scripts/extract_json_strings.ts ${args.join(" ")}\n${stdout}\n${stderr}`,
  )
  return { stdout, stderr }
}

const normalizePot = (pot: string) => {
  return pot
    .replace(/^"POT-Creation-Date: .*\\n"$/m, '"POT-Creation-Date: <normalized>\\n"')
    .replaceAll(`${repoRoot}/`, "")
}

Deno.test("Deno extractor matches fixture snapshot", async () => {
  const tempDir = await Deno.makeTempDir()
  const denoPot = join(tempDir, "deno.pot")
  try {
    await runDenoExtractor([
      "-p",
      "Extractor Fixture Test",
      "-i",
      "scripts/testdata/extract_json_strings/input",
      "-o",
      denoPot,
    ])

    const rawOutput = await Deno.readTextFile(denoPot)
    assertEquals(rawOutput.includes(repoRoot), false)
    const denoOutput = normalizePot(rawOutput)
    const snapshot = await Deno.readTextFile(expectedSnapshot)

    assertEquals(denoOutput, snapshot)
    await runDenoExtractor([
      "-p",
      "Extractor Fixture Test",
      "-i",
      fixtureDir,
      "-o",
      denoPot,
      "--tracked-only",
    ])
    assertEquals(normalizePot(await Deno.readTextFile(denoPot)), snapshot)
  } finally {
    await Deno.remove(tempDir, { recursive: true })
  }
})

Deno.test("Lua extraction handles modern syntax and literals without scanning comments or strings", async () => {
  const tempDir = await Deno.makeTempDir()
  try {
    const source = String.raw`
local quotient = 5 // 2
local bits = (3 & 1) | (2 << 1)
goto finish
--~ Translator note
local message = gettext("quoted \"text\" and \\n")
gettext([=[
long
text]=])
gettext("caf\195\169")
gettext("caf\xC3\xA9")
gettext("\u{1F600}")
gettext("joined\z  \nlines")
vgettext("one", "many", math.max(1, 2))
vpgettext("count", "entry", "entries", 2)
gettext "shorthand"
-- gettext("line comment")
--[=[ gettext("block comment") ]=]
local ignored = 'gettext("inside literal")'
gettext("dynamic " .. ignored)
--~ Separated translator comment
-- unrelated regular comment
gettext("no comment")
-- ~ Spaced translator note
gettext("spaced comment")
::finish::
`
    await Deno.writeTextFile(join(tempDir, "messages.lua"), source)
    const output = join(tempDir, "out.pot")
    const result = await runDenoExtractor(["-i", tempDir, "-o", output])
    const entries = PO.parse(await Deno.readTextFile(output)).items
    assertEquals(entries.map((entry) => entry.msgid), [
      'quoted "text" and \\n',
      "long\ntext",
      "café",
      "café",
      "\u{1F600}",
      "joined\nlines",
      "one",
      "entry",
      "shorthand",
      "no comment",
      "spaced comment",
    ])
    assertEquals(entries[0].extractedComments, ["~ Translator note"])
    assertEquals(entries[6].msgid_plural, "many")
    assertEquals(entries[7].msgctxt, "count")
    assertEquals(entries[7].msgid_plural, "entries")
    assertEquals(entries[9].extractedComments, ["~ Separated translator comment"])
    assertEquals(entries[10].extractedComments, ["~ Spaced translator note"])
    assertStringIncludes(result.stdout, "argument to translation call should be string")
  } finally {
    await Deno.remove(tempDir, { recursive: true })
  }
})

Deno.test("only the literal none sentinel suppresses an item", async () => {
  const dir = await Deno.makeTempDir()
  try {
    await Deno.writeTextFile(
      join(dir, "items.json"),
      JSON.stringify([
        { type: "GENERIC", name: "none", description: "skip me" },
        { type: "GENERIC", name: { str_sp: "none" }, description: "keep me" },
      ]),
    )
    const output = join(dir, "out.pot")
    await runDenoExtractor(["-i", dir, "-o", output])
    const items = PO.parse(await Deno.readTextFile(output)).items
    assertEquals(items.map((entry) => entry.msgid), ["none", "keep me"])
    assertEquals(items[0].msgid_plural, "none")
  } finally {
    await Deno.remove(dir, { recursive: true })
  }
})

Deno.test("JSONC translator comments preserve structure and attach only to the next string", () => {
  const source = `//~ before root
[
  /*
  //~ not a translator comment
  */
  {
    //~ first
    "name": "one", // regular comment
    "description": "two",
    //~ colon key
    "key:with:colon": "three",
    //~ before an object is unsupported, but must not corrupt JSON
    "nested": { "str": "four" }
  }
]`
  assertEquals(parseJsonc(injectJsoncTranslatorComments(source)), [{
    name: { "//~": "first", str: "one" },
    description: "two",
    "key:with:colon": { "//~": "colon key", str: "three" },
    nested: { str: "four" },
  }])
})

Deno.test("excluded directories do not exclude sibling prefixes", async () => {
  const tempDir = await Deno.makeTempDir()
  try {
    for (const name of ["mod", "mod-extra"]) {
      await Deno.mkdir(join(tempDir, name))
      await Deno.writeTextFile(
        join(tempDir, name, "modinfo.jsonc"),
        JSON.stringify([{ type: "MOD_INFO", id: name, name }]),
      )
    }
    const output = join(tempDir, "out.pot")
    await runDenoExtractor(["-i", tempDir, "-E", join(tempDir, "mod"), "-o", output])
    assertEquals(PO.parse(await Deno.readTextFile(output)).items.map((entry) => entry.msgid), [
      "mod-extra",
    ])
  } finally {
    await Deno.remove(tempDir, { recursive: true })
  }
})
