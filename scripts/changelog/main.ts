import { Command } from "@cliffy/command"
import { mapValues, maxBy, minBy } from "jsr:@std/collections"

import { titleCase } from "https://deno.land/x/case@2.1.1/mod.ts"

import { fmtLink } from "./fmt_commit.ts"
import { type CommitInfo, parseCommit } from "./parse_commit.ts"
import { type Commit, readCommits } from "./read_commits.ts"
import { paragraph } from "./paragraph.ts"

const asSections = (group: Record<string, string>): string =>
  Object.entries(group)
    .map(([type, message]) => `### ${titleCase(type)}\n\n${message}`)
    .join("\n\n")

const formatCommitList = (items: { commit: Commit; info: CommitInfo }[]): string =>
  items
    .flatMap((x) => fmtLink({ subject: x.commit.subject, author: x.commit.author }))
    .toSorted((a, b) => a.pr - b.pr)
    .map(({ entry }) => entry)
    .join("\n")

const commitsSection = (changelog: Commit[]) => {
  const parsed = changelog
    .map((commit) => ({ commit, info: parseCommit(commit.subject) }))
    .filter((x): x is { commit: Commit; info: CommitInfo } => x.info !== null)

  const breaking = parsed.filter((x) => x.info.breaking)
  const regular = parsed.filter((x) => !x.info.breaking)

  const byTypes = Object.groupBy(regular, (x) => x.info.type)
  const regularMessages = mapValues(byTypes, formatCommitList)

  const messages: Record<string, string> = {}

  if (breaking.length > 0) {
    messages["Breaking Changes"] = formatCommitList(breaking)
  }

  Object.assign(messages, regularMessages)

  return asSections(messages)
}

const authorsSection = (changelog: Commit[]) => {
  const byAuthors = mapValues(
    Object.groupBy(changelog, (x) => x.author),
    (xs) => xs!.length,
  )

  return Object.entries(byAuthors)
    .sort((a, b) => b[1] - a[1])
    .map(([k, v]) => `- **${k}** with ${v} contributions`)
    .join("\n")
}

const changelogImage =
  "https://preview.redd.it/kukz23r0cszb1.png?width=660&format=png&auto=webp&s=8039aa5748462b40570b8ba98600f5f7359f5320"

const isoDate = (date: Date | string) => new Date(date).toISOString().split("T")[0]

const redditTemplate = (commits: Commit[]) => {
  const begin = minBy(commits, (x) => x.date)!.date
  const end = maxBy(commits, (x) => x.date)!.date

  return /*md*/ `
# CBN Changelog: ${isoDate(end)}. {TITLE GOES HERE}

${changelogImage}

**Changelog for Cataclysm: Bright Nights.**

**Changes for:** ${isoDate(begin)}/${isoDate(end)}.

- **Bright Nights discord server link:** https://discord.gg/XW7XhXuZ89
- **Bright Nights launcher/updater (also works for DDA!) by qrrk:** https://github.com/qrrk/Catapult/releases
- **Bright Nights launcher/updater by 4nonch:** https://github.com/4nonch/BN---Primitive-Launcher/releases
- **TheAwesomeBoophis' UDP revival project:** https://discord.gg/mSATZeZmjz

{DESRIPTION GOES HERE}

## With thanks to

${authorsSection(commits)}

And to all others who contributed to making these updates possible!

## Changelog

${commitsSection(commits)}

## Links

- **Previous changelog:** https://www.reddit.com/r/cataclysmbn/comments/17t44xk/cbn_changelog_november_11_2023_item_identity/
- **Changes so far:** https://github.com/cataclysmbn/Cataclysm-BN/wiki/Changes-so-far
- **Download:** https://github.com/cataclysmbn/Cataclysm-BN/releases
- **Bugs and suggestions can be posted here:** https://github.com/cataclysmbn/Cataclysm-BN/issues

## How to help:

https://docs.cataclysmbn.org/contribute/contributing/

- **Translations!** https://www.transifex.com/bn-team/cataclysm-bright-nights/
- **Contributing via code changes.**
- **Contributing via JSON changes.** Yes, we need modders and content makers help.
- **Contributing via rebalancing content.**
- **Reporting bugs. Including ones inherited from DDA.**
- Identifying problems that aren't bugs. Misleading descriptions, values that are clearly off compared to similar cases, grammar mistakes, UI wonkiness that has an obvious solution.
- Making useless things useful or putting them on a blacklist. Adding deconstruction recipes for things that should have them but don't, replacing completely redundant items with their generic versions (say, "tiny marked bottle" with just "tiny bottle") in spawn lists.
- Tileset work. We're occasionally adding new objects, like the new electric grid elements, and they could use new tiles.
- Balance analysis. Those should be rather in depth or "obviously correct". Obviously correct would be things like: "weapon x has strictly better stats than y, but y requires rarer components and has otherwise identical requirements".
- Identifying performance bottlenecks with a profiler.
- Code quality help.
`
}

const main = new Command()
  .option(
    "-s, --since <ref>",
    "Same as git log --since. Accepts dates, SHAs, or tags (e.g., 2024-09-22, HEAD~10, v0.1)",
    { default: "last monday 1 week ago" },
  )
  .option(
    "-u, --until <ref>",
    "Same as git log --until. Accepts dates, SHAs, or tags (e.g., 2024-10-01, HEAD, v0.2)",
    { default: "today" },
  )
  .option("-o, --output <file>", "Output file to save changelog to")
  .option("-q, --quiet", "Do not print the changelog to stdout")
  .option("-f, --format <format>", "Output format (reddit or default)", {
    default: "default",
    value: (val: string) => {
      if (val !== "reddit" && val !== "default") {
        throw new Error(`Invalid format "${val}". Must be "reddit" or "default"`)
      }
      return val
    },
  })
  .description(paragraph`
      Generate a changelog from git commits.

      usage (at project root): deno task changelog --since 2024-09-22 --until 2024-09-30

      For reddit format, switch to Markdown Editor when pasting the output into Reddit.
    `)
  .action(async ({ since, until, output, quiet = false, format }) => {
    const log = quiet ? () => {} : console.log

    const commits = await readCommits({ since, until })
    log(`${commits.length} commits found`)

    const changelog = format === "reddit" ? redditTemplate(commits) : commitsSection(commits)

    log(changelog)
    if (output) await Deno.writeTextFile(output, changelog)
  })

if (import.meta.main) {
  await main.parse(Deno.args)
}
