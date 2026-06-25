#!/usr/bin/env -S deno run --allow-read --allow-run=git,gh --allow-env=GH_TOKEN,GITHUB_TOKEN,PATH

/**
 * @module
 *
 * Updates PR bodies when localized docs changed without every tracked language variant.
 */

import { Command } from "@cliffy/command"
import $ from "./dax.ts"
import { walk } from "@std/fs"
import { markdownTable } from "markdown-table"
import { changedFilesFromGit, type PullFile } from "./git_changed_files.ts"

const beginMarker = "<!-- BEGIN docs comment -->"
const endMarker = "<!-- END docs comment -->"
const markerPattern = new RegExp(`\n*${beginMarker}[\\s\\S]*?${endMarker}\\s*`, "g")
const changedStatuses = new Set(["added", "modified", "removed", "renamed", "changed"])
const trackedLanguages = ["en", "ja", "ko"]

export type { PullFile }
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

const ghEnv = (): Record<string, string> => {
  const token = Deno.env.get("GH_TOKEN") ?? Deno.env.get("GITHUB_TOKEN")
  return token === undefined ? {} : { GH_TOKEN: token }
}

const readPrBody = async (pr: number): Promise<string> =>
  (await $`gh pr view ${pr} --json body --jq .body`.env(ghEnv()).text()).trimEnd()

const updatePrBody = async (pr: number, body: string): Promise<void> => {
  await $`gh pr edit ${pr} --body-file -`.env(ghEnv()).stdinText(body)
}

const main = new Command()
  .description(
    "updates PR bodies when localized docs changed without every tracked language variant.",
  )
  .arguments("<pr:number>")
  .option("--base <rev:string>", "Base git revision", { default: "origin/main" })
  .option("--head <rev:string>", "Head git revision", { default: "HEAD" })
  .action(async ({ base, head }, pr) => {
    const files = await changedFilesFromGit({ base, head })
    if (files.flatMap(changedDocPaths).length === 0) return

    const block = renderTranslationBlock(
      findTranslationWarnings(files, trackedLanguages, await existingDocPaths()),
    )
    const oldBody = await readPrBody(pr)
    const body = withTranslationBlock(oldBody, block)
    if (body !== oldBody) await updatePrBody(pr, body)
  })

if (import.meta.main) {
  await main.parse(Deno.args)
}
