import type { MergeExclusive } from "npm:type-fest"

type DateLike = string | Date

type RangeOption =
  & MergeExclusive<{ after?: DateLike }, { since?: DateLike }>
  & MergeExclusive<{ before?: DateLike }, { until?: DateLike }>

type Coauthor = {
  name: string
  email: string
}

const coauthorRe = /^Co-authored-by:\s*(.*?)\s*<([^>]+)>$/gimu

const botRe =
  /(\[bot\]|bot@|autofix-ci|copilot|dependabot|github-actions|pre-commit-ci|renovate|claude|anthropic|gpt|codex)/iu

const isBot = ({ name, email }: Coauthor): boolean => botRe.test(name) || botRe.test(email)

const parseCoauthors = (body: string): string[] => {
  const seen = new Set<string>()
  return [...body.matchAll(coauthorRe)]
    .map(([, name, email]) => ({ name: name.trim(), email: email.trim() }))
    .filter((coauthor) => !isBot(coauthor))
    .filter(({ name, email }) => {
      const key = `${name}\0${email}`.toLocaleLowerCase()
      if (seen.has(key)) {
        return false
      }
      seen.add(key)
      return true
    })
    .map(({ name }) => name)
}

const rangeArgs = (option: RangeOption): string[] =>
  Object.entries(option)
    .map(([k, v]) => `--${k}=${typeof v === "string" ? v : v.toISOString()}`)

const gitLog = async (option: RangeOption): Promise<string> => {
  const command = new Deno.Command("git", {
    args: [
      "log",
      ...rangeArgs(option),
      "--pretty=format:%x1f%s%x1f%an%x1f%aI%x1f%B%x1e",
    ],
    stdout: "piped",
  })
  const { stdout, success } = await command.output()
  if (!success) {
    throw new Error("git log failed")
  }
  return new TextDecoder().decode(stdout)
}

export type Commit = {
  subject: string
  author: string
  coauthors: string[]
  date: Date
}

export const readCommits = async (option: RangeOption): Promise<Commit[]> => {
  const log = await gitLog(option)

  return log
    .split("\x1e")
    .filter((record) => record.trim().length > 0)
    .map((record) => {
      const [, subject, author, date, ...body] = record.split("\x1f")
      return { subject, author, coauthors: parseCoauthors(body.join("\x1f")), date: new Date(date) }
    })
}

if (import.meta.main) {
  console.log(await readCommits({ since: "1 week" }))
}
