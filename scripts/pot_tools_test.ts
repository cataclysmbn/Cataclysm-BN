import { assert, assertEquals, assertStringIncludes } from "@std/assert"
import { fromFileUrl, join } from "@std/path"
import PO from "pofile"

const tool = fromFileUrl(new URL("./pot_tools.ts", import.meta.url))
const run = (args: string[]) =>
  new Deno.Command(Deno.execPath(), {
    args: ["run", "--allow-read", "--allow-write", tool, ...args],
    stdout: "piped",
    stderr: "piped",
  }).output()

Deno.test("POT tools preserve contexts, flags, whitespace, comments and plural merging", async () => {
  const dir = await Deno.makeTempDir()
  try {
    const first = join(dir, "first.pot")
    const second = join(dir, "second.pot")
    const output = join(dir, "output.pot")
    const header = 'msgid ""\nmsgstr ""\n"Content-Type: text/plain; charset=UTF-8\\n"\n\n'
    const text = `${"long message ".repeat(8)}  \\ literal\nnext line\tend`
    const item = Object.assign(new PO.Item(), {
      msgid: text,
      msgstr: [""],
      references: ["data/first.json:12"],
      extractedComments: ["First explanation", "Second line"],
      flags: { "no-c-format": true },
    })
    await Deno.writeTextFile(first, header + item.toString() + '\n\nmsgid "same"\nmsgstr ""\n')
    await Deno.writeTextFile(
      second,
      header + "#. Another explanation\n#: data/second.json:42\n#, no-c-format, fuzzy\n" +
        `msgid ${JSON.stringify(text)}\nmsgid_plural "plural"\nmsgstr[0] ""\nmsgstr[1] ""\n\n` +
        'msgctxt ""\nmsgid "same"\nmsgstr ""\n',
    )
    const concat = await run(["concat", first, second, output])
    assert(concat.success, new TextDecoder().decode(concat.stderr))
    assertEquals(PO.parse(await Deno.readTextFile(output)).items.length, 4)
    const dedup = await run(["dedup", output])
    assert(dedup.success, new TextDecoder().decode(dedup.stderr))
    const items = PO.parse(await Deno.readTextFile(output)).items
    assertEquals(items.length, 3)
    assertEquals(items[0].msgid, text)
    assertEquals(items[0].msgid_plural, "plural")
    assertEquals(items[0].msgstr, ["", ""])
    assertEquals(items[0].references, ["data/first.json", "data/second.json"])
    assertEquals(items[0].extractedComments, [
      "First explanation",
      "Second line",
      "Another explanation",
    ])
    assertEquals(Object.keys(items[0].flags).map((flag) => flag.trim()).sort(), [
      "fuzzy",
      "no-c-format",
    ])
    assertEquals(items[1].msgctxt, null)
    assertEquals(items[2].msgctxt, "")
    assertStringIncludes(new TextDecoder().decode(dedup.stdout), "unexpected flags")
    assert((await run(["unicode-check", output])).success)
    await Deno.writeFile(output, new Uint8Array([0xff]))
    assert(!(await run(["unicode-check", output])).success)
  } finally {
    await Deno.remove(dir, { recursive: true })
  }
})
