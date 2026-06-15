#!/usr/bin/env -S deno run --allow-read=docs --allow-env=GITHUB_TOKEN,GITHUB_REPOSITORY --allow-net=api.github.com

/**
 * @module
 *
 * Updates PR bodies when localized docs changed without every tracked language variant.
 */

import { walk } from "@std/fs"
import { Octokit, type RestEndpointMethodTypes } from "@octokit/rest"
import { markdownTable } from "markdown-table"

const beginMarker = "<!-- BEGIN docs comment -->"
const endMarker = "<!-- END docs comment -->"
const markerPattern = new RegExp(`\n*${beginMarker}[\\s\\S]*?${endMarker}\\s*`, "g")
const changedStatuses = new Set(["added", "modified", "removed", "renamed", "changed"])
const trackedLanguages = ["en", "ja", "ko"]

type PullFile = Pick<
  RestEndpointMethodTypes["pulls"]["listFiles"]["response"]["data"][number],
  "filename" | "previous_filename" | "status"
>
type DocPath = { lang: string; path: string }
export type TranslationWarning = {
  path: string
  changed: string[]
  stale: string[]
  missing: string[]
}

const docPaths = (paths: (string | undefined)[]): DocPath[] =>
  paths.flatMap((file) => {
    const [root, lang, ...rest] = file?.split("/") ?? []
    const path = rest.join("/")
    return root === "docs" && /\.(?:md|mdx)$/.test(path) ? [{ lang, path }] : []
  })

const changedDocPaths = ({ filename, previous_filename, status }: PullFile): DocPath[] =>
  changedStatuses.has(status) ? docPaths([filename, previous_filename]) : []

export const findTranslationWarnings = (
  files: PullFile[],
  languages: string[],
  existingDocs: DocPath[],
): TranslationWarning[] => {
  const changed = Map.groupBy(
    files.flatMap(changedDocPaths).filter(({ lang }) => languages.includes(lang)),
    ({ path }) => path,
  )
  if (changed.size <= 1) return []

  const existing = Map.groupBy(
    existingDocs.filter(({ lang }) => languages.includes(lang)),
    ({ path }) => path,
  )
  return [...changed].map(([path, docs]) => {
    const changed = docs.map(({ lang }) => lang).toSorted()
    const stale = (existing.get(path) ?? [])
      .map(({ lang }) => lang)
      .filter((lang) => !changed.includes(lang))
      .toSorted()
    const missing = languages.filter((lang) => !(changed.includes(lang) || stale.includes(lang)))
    return { path, changed, stale, missing }
  })
    .filter(({ stale, missing }) => stale.length > 0 || missing.length > 0)
    .toSorted((a, b) => a.path.localeCompare(b.path))
}

export const renderTranslationBlock = (ws: TranslationWarning[]): string | undefined => {
  if (ws.length === 0) return undefined
  const table = markdownTable([
    ["post", "changed", "stale", "missing"],
    ...ws.map((w) => [w.path, w.changed.join(", "), w.stale.join(", "), w.missing.join(", ")]),
  ])
  return [beginMarker, "", "## Translations", "", table, "", endMarker].join("\n")
}

const existingDocPaths = async () =>
  docPaths(
    await Array.fromAsync(walk("docs", { exts: [".md", ".mdx"], includeDirs: false }), ({ path }) =>
      path),
  )

const withTranslationBlock = (body: string, block: string | undefined): string => {
  const cleanBody = body.replace(markerPattern, "").replace(/[ \t]+\n/g, "\n").trimEnd()
  return block === undefined ? cleanBody : [cleanBody, block].filter(Boolean).join("\n\n")
}

if (import.meta.main) {
  const token = Deno.env.get("GITHUB_TOKEN")
  const [owner, repo] = Deno.env.get("GITHUB_REPOSITORY")?.split("/") ?? []
  const pr = Number(Deno.args[0])
  if (!token) throw new Error("GITHUB_TOKEN is required")
  if (!owner || !repo) throw new Error("GITHUB_REPOSITORY must be owner/repo")
  if (!Number.isInteger(pr)) throw new Error("PR number is required")

  const octokit = new Octokit({ auth: token })
  const files = await octokit.paginate(octokit.rest.pulls.listFiles, {
    owner,
    repo,
    pull_number: pr,
    per_page: 100,
  })
  const block = renderTranslationBlock(
    findTranslationWarnings(files, trackedLanguages, await existingDocPaths()),
  )
  const { data } = await octokit.rest.pulls.get({ owner, repo, pull_number: pr })
  const oldBody = data.body ?? ""
  const body = withTranslationBlock(oldBody, block)
  if (body !== oldBody) await octokit.rest.pulls.update({ owner, repo, pull_number: pr, body })
}
