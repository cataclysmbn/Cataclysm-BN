/**
 * @module
 * The first lines in which two texts differ, for a log that has to show a drift.
 */

/** The first lines that differ, numbered, on disk (`-`) and fresh (`+`), so a CI log shows the drift. */
export function firstDifferences(disk: string, fresh: string, limit = 10): string {
  const [old, now] = [disk.split("\n"), fresh.split("\n")]
  return Array.from({ length: Math.max(old.length, now.length) }, (_, line) => line)
    .filter((line) => old[line] !== now[line]).slice(0, limit)
    .map((line) => `  ${line + 1}: - ${old[line] ?? ""}\n  ${line + 1}: + ${now[line] ?? ""}`)
    .join("\n")
}
