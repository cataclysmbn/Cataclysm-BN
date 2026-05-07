---
name: i18n-context
description: Add gettext context for ambiguous user-facing strings and repair affected PO translations.
compatibility: Requires git, rg, gettext tools, and Python.
---

# I18n Context

Use this when one English msgid has multiple meanings and a locale needs different translations.

## Hard requirements

- Do not reduce requested word/context coverage for review risk, churn, or convenience.
- If a requested meaning needs loader support for contextual translation, add loader support.
- Do not open or update a PR while `msgfmt` or printf checks fail for the touched locale.
- Do not dismiss `msgfmt`/printf failures as pre-existing when this task touches that locale.

## Workflow

- Find every shared msgid before editing:
  - `rg -n 'msgid "<word>"|msgctxt .*\nmsgid "<word>"' lang/po/<lang>.po`
  - `rg -n '"<word>"|translate_marker\( "<word>" \)|_\( "<word>" \)' data src`
- Split meanings at the source, following #8729 and #8741:
  - C++ runtime strings: `pgettext( "short specific context", "text" )`.
  - C++ marker-only strings: `translate_marker_context( "short specific context", "text" )`.
  - JSON translated strings: `{ "ctxt": "short specific context", "str": "text" }` or matching plural fields.
  - Snippet arrays: `{ "text": { "ctxt": "short specific context", "str": "text" } }`.
  - If a JSON field is currently `std::string`, convert it to `translation` so contextual JSON can load.
- Keep context names terse and UI-specific, e.g. `time format`, `map extra`, `music genre`, `damage type`.
- Update `lang/po/<lang>.po` for each new `msgctxt`; do not rely on the old shared `msgstr`.
- Preserve old uncontexted entries only when some source still uses the shared msgid.
- For Korean, check existing glossary first:
  - `rg -C2 -i '<term>' lang/po/ko.po | rg -v '^(#:|--)' | head -n 20`
- Validate after editing:
  - `python3 lang/extract_json_strings.py -i data/json -o /tmp/catabn-i18n-context.pot --tracked-only`
  - `msgfmt -f -c -o /tmp/ko.mo lang/po/ko.po`
  - `./tools/check_po_printf_format.py`
  - If JSON changed, run `./build-scripts/lint-json.sh`.

## PR references

- #8729 split ambiguous yes/no labels in C++ with `pgettext` and `translate_marker_context`.
- #8741 split ambiguous animal/action/species/fish names in JSON with `ctxt` and updated PO entries.
