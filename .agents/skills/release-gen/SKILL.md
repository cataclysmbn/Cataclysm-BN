---
name: release-gen
description: Finish release follow-up after the changelog exists. Update `data/XDG/org.cataclysmbn.CataclysmBN.metainfo.xml` and send the matching Flathub PR with `gh pr create --web`.
compatibility: Requires git, gh, internet access, write access to this repo, and access to `flathub/org.cataclysmbn.CataclysmBN`.
---

# Release Gen

- Use after `changelog-gen` updates all `docs/*/game/changelog/{order}.stable-{semver}.md` files.
- Add the new `<release>` entry at the top of `data/XDG/org.cataclysmbn.CataclysmBN.metainfo.xml`.
- Use the exact version, real release date, tagged GitHub release URL, and 5-7 terse player-facing bullets.
- Then update Flathub `org.cataclysmbn.CataclysmBN.yml` with the matching commit, metainfo URL, and recomputed `sha256`.
- Push the Flathub branch and open the PR with `gh pr create --web`.
- If Flathub cannot be updated, leave an exact handoff note with repo, branch, file, URL, commit, and `sha256` work left.
- Verify XML validity and that version, date, URL, commit, and hash all match.

## Example release

- For `0.11.1`, set the metainfo version to `0.11.1`, use the tagged release URL `https://github.com/cataclysmbn/Cataclysm-BN/releases/tag/v0.11.1`, and update the Flathub manifest commit + metainfo source hash to the same release.
