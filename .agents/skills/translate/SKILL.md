---
name: translate
description: Add the remaining English, Korean, and Japanese translations for documentation files added since main. Use with optional explicit docs file paths to translate a selected documentation change across docs/en, docs/ko, and docs/ja.
---

# Documentation translations

For every selected documentation path, add or update its corresponding documents in `docs/en`, `docs/ko`, and `docs/ja`. Do not assume that English is the source language: detect the supplied or newly added file's language and translate its added content into the other two languages.

## Select documentation paths

- With explicit file paths, use those paths.
- Without explicit paths, find newly added documentation files with:

  ```sh
  git diff --name-only --diff-filter=A main -- docs/en docs/ko docs/ja
  ```

- Group selected files by the part after `docs/<language>/`. Each group represents one document that needs its remaining translations.

## Translate

For each selected document group:

- Inspect the source change and every existing counterpart before editing.
- Add any missing counterparts in `docs/en`, `docs/ko`, and `docs/ja`.
- Where a counterpart already exists, translate the newly added source content into it without replacing unrelated translated content.
- Preserve Markdown structure, code blocks, links, commands, identifiers, and intentional English technical terms.
- Match the terminology and style of adjacent text in each target document.

## Commit and verify

Treat the selected source files and all of their English, Korean, and Japanese counterparts as one requested change. Unless the user requests separate commits, stage and commit all of them together; a pre-existing selected source change is not a reason to omit it.

```sh
deno fmt <touched Markdown files>
git diff --check
```

Review the final diff and report the added or updated language files and validation performed.
