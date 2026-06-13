import { assertEquals } from "@std/assert"
import {
  findTranslationWarnings,
  parseLocalizedDocPath,
  renderWarningComment,
} from "./docs_translation_warning.ts"

Deno.test("parseLocalizedDocPath() accepts localized markdown docs", () => {
  assertEquals(parseLocalizedDocPath("docs/en/game/foo.md"), { lang: "en", path: "game/foo.md" })
  assertEquals(parseLocalizedDocPath("docs/media.css"), undefined)
  assertEquals(parseLocalizedDocPath("docs/en/_data.yml"), undefined)
})

Deno.test("findTranslationWarnings() flags stale and missing language variants", async () => {
  const existing = new Set(["docs/en/foo.md", "docs/ko/foo.md"])
  const warnings = await findTranslationWarnings(
    [{ filename: "docs/en/foo.md", status: "modified" }],
    ["en", "ja", "ko"],
    (lang, path) => Promise.resolve(existing.has(`docs/${lang}/${path}`)),
  )

  assertEquals(warnings, [{ path: "foo.md", changed: ["en"], stale: ["ko"], missing: ["ja"] }])
})

Deno.test("findTranslationWarnings() ignores docs updated in every language", async () => {
  const warnings = await findTranslationWarnings(
    [
      { filename: "docs/en/foo.md", status: "modified" },
      { filename: "docs/ja/foo.md", status: "added" },
      { filename: "docs/ko/foo.md", status: "modified" },
    ],
    ["en", "ja", "ko"],
    () => Promise.resolve(false),
  )

  assertEquals(warnings, [])
})

Deno.test("findTranslationWarnings() treats renames as old and new doc paths", async () => {
  const existing = new Set(["docs/en/old.md", "docs/ko/old.md"])
  const warnings = await findTranslationWarnings(
    [{ filename: "docs/en/new.md", previous_filename: "docs/en/old.md", status: "renamed" }],
    ["en", "ko"],
    (lang, path) => Promise.resolve(existing.has(`docs/${lang}/${path}`)),
  )

  assertEquals(warnings, [
    { path: "new.md", changed: ["en"], stale: [], missing: ["ko"] },
    { path: "old.md", changed: ["en"], stale: ["ko"], missing: [] },
  ])
})

Deno.test("renderWarningComment() stays terse", () => {
  assertEquals(
    renderWarningComment([{ path: "foo.md", changed: ["en"], stale: ["ko"], missing: ["ja"] }]),
    "<!-- docs-translation-sync-warning -->\n⚠️ Following langs are not updated:\n- `foo.md`: changed `en`; stale `ko`; missing `ja`",
  )
})
