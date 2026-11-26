import { assertEquals } from "@std/assert"
import { groupRebaseMerges } from "./merged_prs.ts"
import type { Commit } from "./read_commits.ts"

const commit = (sha: string, subject: string, author = "scarf"): Commit => ({
  sha,
  subject,
  author,
  coauthors: [],
  date: new Date("2026-11-25T00:00:00Z"),
  committed: new Date("2026-11-25T00:00:00Z"),
})

// commits are newest first, like git log

Deno.test("groupRebaseMerges() makes one entry from the commits of a rebase-merged PR", () => {
  const commits = [
    commit("e", "docs: pin docs version"),
    commit("d", "fix: take the hotter heat (#11039)"),
    commit("c", "fix: run weather per turn", "bob"),
    commit("b", "test: compare body state"),
    commit("a", "feat: add solar unit (#11044)"),
  ]
  const prs = new Map([
    ["d", { number: 10997, title: "fix: match single-turn results" }],
    ["a", { number: 11044, title: "feat: add solar unit" }],
  ])
  const prNumbers = new Set([10997, 11044, 11039])

  assertEquals(groupRebaseMerges({ commits, prs, prNumbers }), [
    commit("e", "docs: pin docs version"),
    { ...commit("d", "fix: match single-turn results (#10997)"), coauthors: ["bob"] },
    commit("d", "fix: take the hotter heat (#11039)"),
    commit("a", "feat: add solar unit (#11044)"),
  ])
})

Deno.test("groupRebaseMerges() puts commits that end in an issue number into their PR entry", () => {
  const commits = [
    commit("c", "fix: skip save of failed overmap (#11005)"),
    commit("b", "fix: guard read statement"),
    commit("a", "fix: load negative bionic power (#11005)", "bob"),
  ]
  const prs = new Map([["c", { number: 11007, title: "fix: keep overmap data" }]])

  assertEquals(groupRebaseMerges({ commits, prs, prNumbers: new Set([11007]) }), [
    { ...commit("c", "fix: keep overmap data (#11007)", "bob"), coauthors: ["scarf"] },
  ])
})
