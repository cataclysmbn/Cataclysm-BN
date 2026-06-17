#!/usr/bin/env -S deno run --allow-run=git

/**
 * @module
 *
 * Lists pull request file changes using the local git checkout.
 */

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

const decoder = new TextDecoder()

const runGit = async (args: string[]): Promise<string> => {
  const command = new Deno.Command("git", {
    args,
    clearEnv: true,
    stdout: "piped",
    stderr: "piped",
  })
  const { success, stdout, stderr } = await command.output()
  if (!success) {
    const message = decoder.decode(stderr).trim()
    throw new Error(`git ${args.join(" ")} failed${message ? `: ${message}` : ""}`)
  }
  return decoder.decode(stdout)
}

const parseGitStatus = (status: string): PullFileStatus => {
  switch (status[0]) {
    case "A":
      return "added"
    case "C":
      return "copied"
    case "D":
      return "removed"
    case "M":
      return "modified"
    case "R":
      return "renamed"
    default:
      return "changed"
  }
}

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
  const output = await runGit([
    "diff",
    "--name-status",
    "--find-renames",
    "--find-copies",
    `${base}...${head}`,
  ])
  return parseGitNameStatus(output)
}
