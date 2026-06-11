import { typedRegEx } from "https://deno.land/x/typed_regex@0.1.0/mod.ts"

const re = typedRegEx("(?<title>.*?)\\(#(?<pr>\\d+)\\)$")

/**
 * Format changelog into a markdown link
 */
export const fmtLink = (
  { subject, author, coauthors = [] }: { subject: string; author: string; coauthors?: string[] },
) => {
  const groups = re.match(subject).groups
  if (!groups) return []

  const { title, pr } = groups
  const contributors = [author, ...coauthors].join(", ")
  const entry =
    `- [#${pr}](https://github.com/cataclysmbn/Cataclysm-BN/pull/${pr}) **${title.trim()}** by ${contributors}.`

  return [{ pr: parseInt(pr, 10), entry }]
}
