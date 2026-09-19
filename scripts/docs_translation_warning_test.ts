import { assertEquals } from "@std/assert"
import { findTranslationWarnings, renderTranslationBlock } from "./docs_translation_warning.ts"

Deno.test("findTranslationWarnings() flags stale and missing language variants", () => {
  const warnings = findTranslationWarnings(
    [
      { filename: "docs/en/foo.md", status: "modified" },
      { filename: "docs/en/bar.md", status: "modified" },
    ],
    ["en", "ja", "ko"],
    [
      { lang: "en", path: "foo.md" },
      { lang: "ko", path: "foo.md" },
      { lang: "en", path: "bar.md" },
    ],
  )

  assertEquals(warnings, [
    { path: "bar.md", changed: ["en"], stale: [], missing: ["ja", "ko"] },
    { path: "foo.md", changed: ["en"], stale: ["ko"], missing: ["ja"] },
  ])
})

Deno.test("findTranslationWarnings() flags a single changed doc path", () => {
  const warnings = findTranslationWarnings(
    [{ filename: "docs/en/foo.md", status: "modified" }],
    ["en", "ja", "ko"],
    [],
  )

  assertEquals(warnings, [
    { path: "foo.md", changed: ["en"], stale: [], missing: ["ja", "ko"] },
  ])
})

Deno.test("findTranslationWarnings() ignores docs updated in every language", () => {
  const warnings = findTranslationWarnings(
    [
      { filename: "docs/en/foo.md", status: "modified" },
      { filename: "docs/ja/foo.md", status: "added" },
      { filename: "docs/ko/foo.md", status: "modified" },
      { filename: "docs/en/bar.md", status: "modified" },
      { filename: "docs/ja/bar.md", status: "added" },
      { filename: "docs/ko/bar.md", status: "modified" },
    ],
    ["en", "ja", "ko"],
    [],
  )

  assertEquals(warnings, [])
})

Deno.test("findTranslationWarnings() treats renames as old and new doc paths", () => {
  const warnings = findTranslationWarnings(
    [{ filename: "docs/en/new.md", previous_filename: "docs/en/old.md", status: "renamed" }],
    ["en", "ko"],
    [
      { lang: "en", path: "old.md" },
      { lang: "ko", path: "old.md" },
    ],
  )

  assertEquals(warnings, [
    { path: "new.md", changed: ["en"], stale: [], missing: ["ko"] },
    { path: "old.md", changed: ["en"], stale: ["ko"], missing: [] },
  ])
})

Deno.test("renderTranslationBlock() renders a PR body table", () => {
  assertEquals(
    renderTranslationBlock([
      { path: "i18n/guides/mods.md", changed: ["en"], stale: ["ja", "ko"], missing: [] },
      { path: "mod/dialogue.md", changed: ["en"], stale: [], missing: ["ja", "ko"] },
    ]),
    [
      "<!-- BEGIN docs comment -->",
      "",
      "## Translations",
      "",
      "| post                | changed | stale  | missing |",
      "| ------------------- | ------- | ------ | ------- |",
      "| i18n/guides/mods.md | en      | ja, ko |         |",
      "| mod/dialogue.md     | en      |        | ja, ko  |",
      "",
      "<!-- END docs comment -->",
    ].join("\n"),
  )
})
