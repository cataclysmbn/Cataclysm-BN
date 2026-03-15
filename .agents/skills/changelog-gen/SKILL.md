---
name: changelog-gen
description: Rewrite every `docs/*/game/changelog/{order}.stable-{semver}.md` from a ref range. Default `--limit=20`.
compatibility: Requires git, gh, internet access, and write access to this repo.
---

# Changelog Gen

- One ref => `start..HEAD`; two refs => `start..end`.
- `--limit=N` defaults to `20` and only caps `## Featured`.
- Use cheap subagents for crawling and choice; rank groups by reactions first, then player impact.
- Update every language (en, ko, ja...) copy with file name `{order}.stable-{semver}.md`.
- Follow the style of `docs/en/game/changelog/11.stable-0.11.0.md`.
- For localized docs, look up every user-visible game term first with `rg -C2 -i '<term>' lang/po/{lang}.po | head`.
- If `msgstr` exists, use it for game terms in headings and body text. Do not leave the English term in localized docs.
- Keep markdown structure synced to `docs/en/game/changelog/11.stable-0.11.0.md`; keep structural labels like `## Featured`, `## Bugfixes`, `Credits`, and `screenshots` identical unless the user asks otherwise.
- Do not use subagents for glossary lookup. Korean title uses `안정판`, not `Stable`.
- Group related PRs into one section and link every PR + author in `<details><summary>Credits</summary>`.
- Crawl PR bodies, comments, and reviews with `gh api`.
- Crawl every screenshot so the user can curate later.
- If a featured PR has multiple screenshots, include all.
- If a PR body has before/after shots, preserve them as-is in a markdown table: `| {NAME} before | {NAME} after |`.
- Refer to `docs/en/game/changelog/11.stable-0.11.0.md` for example.
- Keep it terse and player-facing. No corpo phrasing, hype, filler, or adverbs (NEVER `it's not just A but B`)
- Modder-only content belongs in `## JSON Modding` and `## Lua Modding`, not `## Featured`.
- Put every bugfix in `## Bugfixes`, and all JSON/Lua modding changes in `## JSON Modding` and `## Lua Modding`.
- If the skill still feels wrong, stop and ask to improve the skill before writing docs.

## Example command

`/skill changelog-gen v0.10.0 v0.11.0 --limit=20`

## Before/after example

```md
| speedway before      | speedway after      |
| -------------------- | ------------------- |
| <img src="BEFORE" /> | <img src="AFTER" /> |
```
