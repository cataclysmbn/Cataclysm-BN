#!/usr/bin/env -S deno run --allow-read --allow-run=git --allow-env=PATH

/**
 * @module
 *
 * Lists pull request file changes using the local git checkout.
 */

import $ from "./dax.ts"

export type PullFileStatus = "added" | "copied" | "modified" | "removed" | "renamed" | "changed"

export type PullFile = {
  filename: string
  previous_filename?: string
  status: PullFileStatus
}

export type ChangedFilesOptions = {
  base?: string
  head?: string
}

const gitStatusMap = {
  A: "added",
  C: "copied",
  D: "removed",
  M: "modified",
  R: "renamed",
} as const satisfies Record<string, PullFileStatus>

const parseGitStatus = (status: string): PullFileStatus =>
  gitStatusMap[status[0] as keyof typeof gitStatusMap] ?? "changed"

export const parseGitNameStatus = (output: string): PullFile[] =>
  output.trim().split("\n")
    .filter(Boolean)
    .map((line) => {
      const [status, firstPath, secondPath] = line.split("\t")
      if ((status.startsWith("R") || status.startsWith("C")) && secondPath !== undefined) {
        return {
          filename: secondPath,
          previous_filename: firstPath,
          status: parseGitStatus(status),
        }
      }
      return { filename: firstPath, status: parseGitStatus(status) }
    })

export const changedFilesFromGit = async (
  { base = "origin/main", head = "HEAD" }: ChangedFilesOptions = {},
): Promise<PullFile[]> => {
  const output = await $`git diff --name-status --find-renames --find-copies ${`${base}...${head}`}`
    .text()
  return parseGitNameStatus(output)
}
