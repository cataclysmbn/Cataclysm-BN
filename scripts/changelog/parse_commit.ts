import { typedRegEx } from "https://deno.land/x/typed_regex@0.1.0/mod.ts"

export type CommitInfo = {
  type: string
  desc: string
  scopes: string[]
  pr: number
  breaking: boolean
}

/**
 * Parse a squash-merged commit in conventional commit format.
 * Returns null if the commit doesn't match the expected format.
 */
export const parseCommit = (x: string): CommitInfo | null => {
  const res = re.captures(x)
  if (!res) return null

  const { type, desc, scopes, pr, breaking } = res
  return { type, desc, scopes: scopes?.split(",") ?? [], pr: +pr, breaking: !!breaking }
}

const re = typedRegEx(
  "^(?<type>\\w+)(\\((?<scopes>.*)\\))?(?<breaking>!)?:\\s*(?<desc>.*?)\\s*\\(#(?<pr>\\d+)\\)$",
)
