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
- Follow the style of `docs/en/game/changelog/11.stable-0.11.1.md`.
- For localized docs, look up every user-visible game term first with `rg -C2 -i '<term>' lang/po/{lang}.po | head`.
- If `msgstr` exists, use it for game terms and user-visible labels in headings/body. Do not leave English in localized docs.
- Translate every user-visible heading, summary label, table label, and caption in localized docs. `Featured`, `Credits`, `screenshots`, `before`, and `after` must be localized.
- Keep section order, media blocks, PR grouping, and tables synced to English, but never copy English labels back into localized docs.
- Preserve videos as `<video controls src="URL" title="CAPTION"></video>` immediately below the feature heading and outside image tables or `<details>` blocks; never render video/MP4 attachments with image Markdown and never delete valid videos solely because they are videos. Use a localized `media` summary label only for folded image blocks that accompany videos.
- Do not use subagents for glossary lookup. Korean title is `안정판`, not `Stable`.
- Korean glossary trap: Cataclysm is `카타클리즘`, never `대격변`.
- After writing every changelog file and before committing/opening/updating a PR, MUST run the `reviewer` subagent against the generated docs.
  - Primary agent must first gather glossary evidence directly with `rg -C2 -i '<term>' lang/po/{lang}.po | head`; pass that evidence to the reviewer instead of asking the reviewer to perform glossary lookup.
  - Reviewer prompt must ask for issues only, not rewrites, and must cover: meme/joke/irrelevant images, Tracy/perf graphs, buggy screenshots, valid videos deleted instead of converted to `<video>`, video/MP4 attachments rendered with image Markdown, too many inline images that should be under `<details>`, spoilers, non-player-facing items in `## Featured`, wrong JSON/Lua/Bugfix section placement, significant player-facing PRs buried in catch-all sections, misplaced questlines/locations/labs/finales/mod mainlines/mutation rebalances, unsynced language sections/media/labels, missing coauthors, bot/AI authors, and localized glossary mistakes.
  - Treat reviewer output as a gate: fix every valid issue, then rerun reviewer or explicitly document why remaining findings are not applied before finalizing.
- Group related PRs into one section and link every PR + author in `<details><summary>Credits</summary>`.
- Crawl PR bodies, comments, reviews, and linked source repositories with `gh api`; link named external mods/projects in player-facing summaries.
- Crawl every screenshot so the user can curate later.
- If a featured PR has multiple screenshots, include all.
- In `## Featured`, keep media visible unless a feature group has 9 or more images; if hiding 9+ images, use one media `<details>` block for the whole feature group instead of one `<details>` block per PR.
- Format 2+ images as a markdown table. Remove images that are too wide to read at table-column width.
- If a PR body has before/after shots, preserve them as-is in a markdown table: `| {NAME} before | {NAME} after |`.
- Refer to `docs/en/game/changelog/11.stable-0.11.1.md` for example.
- Keep it terse and player-facing. No corpo phrasing, hype, filler, or adverbs (NEVER `it's not just A but B`)
- Modder-only content belongs in `## JSON Modding` and `## Lua Modding`, not `## Featured`.
- Put every bugfix in `## Bugfixes`, and all JSON/Lua modding changes in `## JSON Modding` and `## Lua Modding`.
- Do not bury significant player-facing work in catch-all sections. Create/reuse focused Featured groups for questlines, locations, lab/finale changes, in-repo mod mainlines, vehicle families, mutation rebalances, companions/mounts, and other release-scale themes.
- Keep questlines separate from generic location/lab sections unless the PR is only a map room; keep lab rooms/finales/ice lab changes together; keep military airfields/blacksites in locations; keep mutation path rebalances in mutation sections.
- If the skill still feels wrong, stop and ask to improve the skill before writing docs.

## Example command

`/skill changelog-gen v0.10.0 v0.11.1 --limit=20`

## Before/after example

```md
| speedway before      | speedway after      |
| -------------------- | ------------------- |
| <img src="BEFORE" /> | <img src="AFTER" /> |
```
