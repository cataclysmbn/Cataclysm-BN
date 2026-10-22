#!/usr/bin/env -S deno run --allow-read --allow-write
/**
 * @module
 *
 * Measures per-file-tag test durations from Catch2 XML reports (`-r xml -d yes`, as written by
 * `run-linux-test-shards.ts`) and writes the weights `run-linux-test-shards.ts` balances shards by.
 *
 * Usage: `build-scripts/test-shard-weights.ts <report.log>... > build-scripts/test-shard-weights.json`
 */

import { assertEquals } from "@std/assert"

export type ShardWeights = { nonSlow: Record<string, number>; slow: Record<string, number> }

const testCasePattern =
  /<TestCase name="[^"]*"(?: description="[^"]*")? tags="([^"]*)"[\s\S]*?<OverallResult success="\w+" durationInSeconds="([^"]*)"\/>\s*<\/TestCase>/g

/** Sums whole-second durations of every test case by its `[#..._test]` file tag. */
export const measureWeights = (reports: string[]): ShardWeights => {
  const weights: ShardWeights = { nonSlow: {}, slow: {} }
  for (const report of reports) {
    for (const [, tags, duration] of report.matchAll(testCasePattern)) {
      const fileTag = tags.match(/\[#[^\]]*_test\]/)?.[0]
      if (fileTag === undefined || tags.includes("[.]")) continue
      const bucket = tags.includes("[slow]") ? weights.slow : weights.nonSlow
      bucket[fileTag] = (bucket[fileTag] ?? 0) + Number(duration)
    }
  }
  const rounded = (bucket: Record<string, number>) =>
    Object.fromEntries(
      Object.entries(bucket).toSorted(([a], [b]) => a.localeCompare(b))
        .map(([tag, seconds]) => [tag, Math.max(1, Math.round(seconds))]),
    )
  return { nonSlow: rounded(weights.nonSlow), slow: rounded(weights.slow) }
}

Deno.test("measureWeights sums cases per file tag and splits slow cases", () => {
  const case_ = (tags: string, seconds: string) =>
    `<TestCase name="x" description=" " tags="${tags}" filename="f" line="1">
      <Section name="s"><OverallResults successes="1" failures="0" durationInSeconds="9"/></Section>
      <OverallResult success="true" durationInSeconds="${seconds}"/>
    </TestCase>`
  const report = [
    case_("[#a_test][x]", "1.4"),
    case_("[#a_test][slow]", "20"),
    case_("[#a_test][x]", "1.4"),
    case_("[#b_test][.]", "50"),
    case_("[untagged]", "5"),
  ].join("\n")
  assertEquals(measureWeights([report]), { nonSlow: { "[#a_test]": 3 }, slow: { "[#a_test]": 20 } })
})

if (import.meta.main) {
  const reports = await Promise.all(Deno.args.map((path) => Deno.readTextFile(path)))
  console.log(JSON.stringify(measureWeights(reports), null, 2))
}
