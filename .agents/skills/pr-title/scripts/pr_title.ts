#!/usr/bin/env -S deno run --allow-read --allow-run=git,deno

import { Command } from "@cliffy/command"

export const allowed_types = new Set([
  "feat",
  "fix",
  "docs",
  "style",
  "refactor",
  "perf",
  "test",
  "build",
  "ci",
  "chore",
  "revert",
])

export type ChangeEntry = {
  status: string
  path: string
}

const docs_threshold = 0.8

export const parse_conventional_title = (
  title: string,
): { type: string; scope?: string } | undefined => {
  const trimmed = title.trim()
  const match = /^(?<type>[a-z]+)(?:\((?<scope>[^)]+)\))?:\s+.+$/.exec(trimmed)
  if (!match?.groups) {
    return undefined
  }
  const type = match.groups.type
  const scope = match.groups.scope
  return scope === undefined ? { type } : { type, scope }
}

export const parse_name_status = (text: string): ChangeEntry[] =>
  text
    .split("\n")
    .map((line) => line.trim())
    .filter((line) => line.length > 0)
    .flatMap((line): ChangeEntry[] => {
      const columns = line.split("\t")
      if (columns.length < 2) {
        return []
      }
      const status = columns[0]
      const path = status.startsWith("R") || status.startsWith("C") ? columns[2] : columns[1]
      return path === undefined ? [] : [{ status, path }]
    })

export const is_docs_path = (path: string): boolean => path.startsWith("docs/")

export const find_new_mod_ids = (entries: ChangeEntry[]): string[] =>
  entries
    .flatMap((entry) => {
      if (!entry.status.startsWith("A")) {
        return []
      }
      const match = /^data\/mods\/([^/]+)\/modinfo\.json$/.exec(entry.path)
      return match ? [match[1]] : []
    })
    .toSorted()

export type ValidationResult = {
  ok: boolean
  reason?: string
}

export const validate_title_type = (
  parsed_title: { type: string; scope?: string } | undefined,
  entries: ChangeEntry[],
): ValidationResult => {
  if (parsed_title === undefined) {
    return {
      ok: false,
      reason: "PR title must follow Conventional Commits: <type>(<scope>): <subject>",
    }
  }

  if (!allowed_types.has(parsed_title.type)) {
    return {
      ok: false,
      reason: `Unsupported type \`${parsed_title.type}\`. Use one of: ${
        Array.from(allowed_types).join(", ")
      }`,
    }
  }

  if (entries.length === 0) {
    return { ok: true }
  }

  const docs_count = entries.filter((entry) => is_docs_path(entry.path)).length
  const docs_ratio = docs_count / entries.length

  if (docs_ratio > docs_threshold && parsed_title.type !== "docs") {
    return {
      ok: false,
      reason:
        "`docs/` changes exceed 80% of touched files, so PR type must be `docs` per project policy.",
    }
  }

  return { ok: true }
}

const read_changed_files = async (base_ref: string): Promise<ChangeEntry[]> => {
  const refs = [base_ref, "upstream/main", "origin/main", "main"]

  for (const ref of refs) {
    const command = new Deno.Command("git", {
      args: ["diff", "--name-status", `${ref}...HEAD`],
      stdout: "piped",
      stderr: "null",
    })
    const output = await command.output()
    if (output.code === 0) {
      return parse_name_status(new TextDecoder().decode(output.stdout))
    }
  }

  throw new Error("Unable to read git diff against any main reference")
}

const run_semantic_task = async (): Promise<void> => {
  const command = new Deno.Command("deno", {
    args: ["task", "semantic"],
    stdout: "inherit",
    stderr: "inherit",
  })
  const output = await command.output()
  if (output.code !== 0) {
    throw new Error("`deno task semantic` failed")
  }
}

if (import.meta.main) {
  await new Command()
    .name("pr-title")
    .description("Validate PR title type/scope against changed files")
    .option("--title <title:string>", "PR title to validate", { required: true })
    .option("--base <ref:string>", "base ref used for git diff", { default: "upstream/main" })
    .action(async ({ title, base }) => {
      const entries = await read_changed_files(base)
      const parsed_title = parse_conventional_title(title)
      const validation = validate_title_type(parsed_title, entries)

      if (!validation.ok) {
        console.error(`PR title invalid: ${validation.reason}`)
        Deno.exit(1)
      }

      const new_mod_ids = find_new_mod_ids(entries)
      if (new_mod_ids.length > 0) {
        console.log(`New mods detected: ${new_mod_ids.join(", ")}`)
        await run_semantic_task()
      }

      console.log("PR title validation passed")
    })
    .parse(Deno.args)
}
