<!-- for small documentation fixes, it's okay to ignore the template -->

## Purpose of change (The Why)

<!-- e.g resolves #1234 / monster A is too OP despite being an early-game mob -->

## Describe the solution (The How)

<!-- e.g nerfs monster A -->

## Describe alternatives you've considered

## Testing

<!-- Describe what steps you took to test that this PR resolved the bug or added the feature, and what tests you performed to make sure it didn't cause any regressions.  Also include testing suggestions for reviewers and maintainers. -->

## Additional context

<!-- Add any other context (such as mock-ups, proof of concepts or screenshots) about the feature or bugfix here. -->

## Checklist

<!--
NOTE: Please grant permission for repository maintainers to edit your PR.  It is EXTREMELY common for PRs to be held up due to trivial changes being requested and the author being unavailable to make them.  In web UI, you can do it by clicking the "Allow edits and access to secrets by maintainers" checkbox next to "Create Pull Request" button at the bottom of the editor, or by clicking the same checkbox in the sidebar after PR has been created.

NOTE: Please read your emails. Anyone mentioned on Github with an @ will receive an email, any activity on your work will also send emails. This is more reliable than being notified on our Discord, you will always get an email.
--->

### Mandatory

- [ ] I wrote the PR title in [conventional commit format](https://docs.cataclysmbn.org/en/contribute/changelog_guidelines/).
- [ ] I ran the [code formatter](https://docs.cataclysmbn.org/en/contribute/contributing/#code-style).
- [ ] I linked any relevant issues using [github keyword syntax](https://docs.cataclysmbn.org/en/contribute/contributing/#pull-request-notes) like `closes #1234` in [Summary of the PR](#purpose-of-change-the-why) so it can be closed automatically.
- [ ] I've [committed my changes to new branch that isn't `main`](https://docs.cataclysmbn.org/en/contribute/contributing/#make-your-changes) so it won't cause conflict when updating `main` branch later.

<!--
please remove sections irrelevant to this PR.

### Optional

- [ ] This PR ports commits from DDA or other cataclysm forks.
  - [ ] I have attributed original authors in the commit messages adding [`Co-Authored-By`](https://docs.github.com/pull-requests/committing-changes-to-your-project/creating-and-editing-commits/creating-a-commit-with-multiple-authors) in the commit message.
  - [ ] I have linked the URL of original PR(s) in the description.
- [ ] This is a C++ PR that modifies JSON loading or behavior.
  - [ ] I have documented the changes in the appropriate location in the `doc/` folder.
  - [ ] If documentation for this feature does not exist, please write it or at least note its lack in PR description.
  - [ ] New localizable fields need to be added to the `lang/bn_extract_json_strings.sh` script if it does not support them yet.
  - [ ] If applicable, add checks on game load that would validate the loaded data.
  - [ ] If it modifies format of save files, please add migration from the old format.
- [ ] This is a PR that modifies build process or code organization.
  - [ ] Please document the changes in the appropriate location in the `doc/` folder.
  - [ ] If documentation for this feature or process does not exist, please write it.
  - [ ] If the change alters versions of software required to build or work with the game, please document it.
- [ ] This is a PR that removes JSON entities.
  - [ ] The removed JSON entities have new entries in `data/json/obsoletion/` folder or use some other migration process for existing saves.
- [ ] This PR modifies BN's lua API.
  - [ ] I have committed the output of `deno task doc` so the Lua API documentation is updated.
-->
