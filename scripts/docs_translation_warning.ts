#!/usr/bin/env -S deno run --allow-read=docs --allow-env=GITHUB_TOKEN,GITHUB_REPOSITORY --allow-net=api.github.com

/**
 * @module
 *
 * Warns on PRs that change localized docs without updating every language variant.
 */

import { Command } from "@cliffy/command"
import { Octokit, type RestEndpointMethodTypes } from "@octokit/rest"

const marker = "<!-- docs-translation-sync-warning -->"
const docExt = /\.(?:md|mdx)$/
const changedStatuses = new Set(["added", "modified", "removed", "renamed", "changed"])

type PullFile = Pick<
  RestEndpointMethodTypes["pulls"]["listFiles"]["response"]["data"][number],
  "filename" | "previous_filename" | "status"
>

type DocPath = {
  lang: string
  path: string
}

export type TranslationWarning = {
  path: string
  changed: string[]
  stale: string[]
  missing: string[]
}

const repoSchema = /^(?<owner>[^/]+)\/(?<repo>[^/]+)$/

const isFile = async (path: string): Promise<boolean> => {
  try {
    return (await Deno.stat(path)).isFile
  } catch (error) {
    if (error instanceof Deno.errors.NotFound) return false
    throw error
  }
}

export const getDocLanguages = async (): Promise<string[]> => {
  const result: string[] = []
  for await (const entry of Deno.readDir("docs")) {
    if (!entry.isDirectory) continue
    if (await isFile(`docs/${entry.name}/_data.yml`)) result.push(entry.name)
  }
  return result.toSorted()
}

export const parseLocalizedDocPath = (path: string): DocPath | undefined => {
  if (!docExt.test(path)) return undefined
  const parts = path.split("/")
  if (parts[0] !== "docs" || parts.length < 3) return undefined
  return { lang: parts[1], path: parts.slice(2).join("/") }
}

const changedDocPaths = ({ filename, previous_filename, status }: PullFile): DocPath[] => {
  if (!changedStatuses.has(status)) return []
  return [filename, previous_filename]
    .filter((path): path is string => path !== undefined)
    .map(parseLocalizedDocPath)
    .filter((path): path is DocPath => path !== undefined)
}

export const findTranslationWarnings = async (
  files: PullFile[],
  languages: string[],
  existsInBase: (lang: string, path: string) => Promise<boolean>,
): Promise<TranslationWarning[]> => {
  const knownLanguages = new Set(languages)
  const changed = new Map<string, Set<string>>()

  for (const path of files.flatMap(changedDocPaths)) {
    if (!knownLanguages.has(path.lang)) continue
    changed.set(path.path, (changed.get(path.path) ?? new Set()).add(path.lang))
  }

  const warnings = await Promise.all(
    Array.from(changed.entries()).map(async ([path, changedLangs]) => {
      const untouched = languages.filter((lang) => !changedLangs.has(lang))
      const stale: string[] = []
      const missing: string[] = []
      for (const lang of untouched) {
        if (await existsInBase(lang, path)) stale.push(lang)
        else missing.push(lang)
      }
      return { path, changed: Array.from(changedLangs).toSorted(), stale, missing }
    }),
  )

  return warnings
    .filter(({ stale, missing }) => stale.length > 0 || missing.length > 0)
    .toSorted((a, b) => a.path.localeCompare(b.path))
}

const ticks = (xs: string[]) => xs.map((x) => `\`${x}\``).join(", ")

export const renderWarningComment = (warnings: TranslationWarning[]): string | undefined => {
  if (warnings.length === 0) return undefined
  const shown = warnings.slice(0, 20)
  const lines = shown.map(({ path, changed, stale, missing }) => {
    const parts = [`changed ${ticks(changed)}`]
    if (stale.length > 0) parts.push(`stale ${ticks(stale)}`)
    if (missing.length > 0) parts.push(`missing ${ticks(missing)}`)
    return `- \`${path}\`: ${parts.join("; ")}`
  })
  if (shown.length < warnings.length) lines.push(`- … ${warnings.length - shown.length} more`)
  return `${marker}\n⚠️ Following langs are not updated:\n${lines.join("\n")}`
}

const getPullFiles = async (octokit: Octokit, owner: string, repo: string, pr: number) =>
  await octokit.paginate(octokit.rest.pulls.listFiles, {
    owner,
    repo,
    pull_number: pr,
    per_page: 100,
  })

const upsertComment = async (
  octokit: Octokit,
  owner: string,
  repo: string,
  pr: number,
  body: string | undefined,
) => {
  const comments = await octokit.paginate(octokit.rest.issues.listComments, {
    owner,
    repo,
    issue_number: pr,
    per_page: 100,
  })
  const existing = comments.find((comment) => comment.body?.includes(marker))

  if (body === undefined) {
    if (existing) await octokit.rest.issues.deleteComment({ owner, repo, comment_id: existing.id })
    return
  }

  if (existing) {
    await octokit.rest.issues.updateComment({ owner, repo, comment_id: existing.id, body })
  } else {
    await octokit.rest.issues.createComment({ owner, repo, issue_number: pr, body })
  }
}

const main = new Command()
  .name("docs-translation-warning")
  .description("Warn when localized docs are not updated across all languages.")
  .arguments("<pr:number>")
  .action(async (_options, pr) => {
    const token = Deno.env.get("GITHUB_TOKEN")
    const repository = Deno.env.get("GITHUB_REPOSITORY")
    if (!token) throw new Error("GITHUB_TOKEN is required")
    const match = repository?.match(repoSchema)
    if (!match?.groups) throw new Error("GITHUB_REPOSITORY must be owner/repo")

    const { owner, repo } = match.groups
    const octokit = new Octokit({ auth: token })
    const [files, languages] = await Promise.all([
      getPullFiles(octokit, owner, repo, pr),
      getDocLanguages(),
    ])
    const warnings = await findTranslationWarnings(
      files,
      languages,
      (lang, path) => isFile(`docs/${lang}/${path}`),
    )
    await upsertComment(octokit, owner, repo, pr, renderWarningComment(warnings))
  })

if (import.meta.main) {
  await main.parse(Deno.args)
}
