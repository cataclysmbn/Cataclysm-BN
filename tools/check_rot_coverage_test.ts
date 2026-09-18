import { assertEquals } from "@std/assert"
import {
  checkCoverage,
  type Counts,
  type FunctionCoverage,
  parseReport,
  scope,
  sourcePath,
  wholeFiles,
} from "./check_rot_coverage.ts"

const complete: Counts = {
  regions: 2,
  missedRegions: 0,
  lines: 3,
  missedLines: 0,
  branches: 2,
  missedBranches: 0,
}
const covered = (): FunctionCoverage[] => [
  ...wholeFiles.map((file) => ({
    file,
    name: "rot::existing_function()",
    counts: { ...complete },
  })),
  ...Object.entries(scope).flatMap(([file, names]) =>
    names.map((stem) => ({ file, name: `${stem}()`, counts: { ...complete } }))
  ),
]

Deno.test("coverage requires every manifest function", () => {
  assertEquals(checkCoverage(covered()).passed, true)
  assertEquals(checkCoverage([]).passed, false)
  assertEquals(checkCoverage(covered().slice(1)).missing, ["whole file: rot.cpp"])
})

Deno.test("unrounded misses and absent summaries cannot pass", () => {
  for (
    const counts of [
      undefined,
      { ...complete, regions: 0 },
      { ...complete, lines: 0 },
      { ...complete, missedRegions: 1 },
      { ...complete, missedLines: 1 },
      { ...complete, branches: 1000000, missedBranches: 1 },
    ]
  ) {
    const functions = covered()
    functions[0].counts = counts
    assertEquals(checkCoverage(functions).passed, false)
    assertEquals(checkCoverage(functions).failed.length, 1)
  }
})

Deno.test("nested callbacks and whole-file functions join the denominator", () => {
  for (
    const extra of [
      { file: "item.cpp", name: "item::actualize_rot()::$_0::operator()()" },
      { file: "rot.cpp", name: "rot::temp::new_helper()" },
    ]
  ) {
    assertEquals(
      checkCoverage([...covered(), { ...extra, counts: { ...complete, missedLines: 1 } }]).passed,
      false,
    )
  }
  assertEquals(
    checkCoverage([...covered(), { file: "item.cpp", name: "item::unrelated()" }]).passed,
    true,
  )
})

Deno.test("whole-file presence is required even when no functions are selected", () => {
  const withoutWholeFile = covered().filter((function_coverage) =>
    function_coverage.file !== "rot.cpp"
  )
  assertEquals(checkCoverage(withoutWholeFile).passed, false)
  assertEquals(checkCoverage(withoutWholeFile).missing[0], "whole file: rot.cpp")
})

Deno.test("new item rot methods cannot escape the whole-file denominator", () => {
  const method = {
    file: "rot/item_rot.cpp",
    name: "item::new_rot_method()",
    counts: { ...complete, missedLines: 1 },
  }
  assertEquals(checkCoverage([...covered(), method]).passed, false)
  assertEquals(
    checkCoverage([...covered(), { ...method, counts: { ...complete } }]).passed,
    true,
  )
  assertEquals(
    checkCoverage(covered().filter((f) => f.file !== "rot/item_rot.cpp")).missing,
    ["whole file: rot/item_rot.cpp"],
  )
})

Deno.test("relocated rot files retain complete-file coverage without basename aliases", () => {
  for (const file of ["rot/item_rot.cpp", "rot/rot_calculation.cpp"]) {
    for (
      const filename of [
        `/src/${file}`,
        `/repo/src/${file}`,
        `C:\\repo\\src\\${file.replaceAll("/", "\\")}`,
      ]
    ) {
      const root = filename.startsWith("C:") ? "C:\\repo" : "/repo"
      assertEquals(sourcePath(filename, root), file)
      assertEquals(
        checkCoverage([...covered(), {
          file: sourcePath(filename, root),
          name: "rot::new_helper()",
          counts: { ...complete, missedBranches: 1 },
        }]).passed,
        false,
      )
    }
    const basenameOnly = covered().map((entry) =>
      entry.file === file ? { ...entry, file: file.slice("rot/".length) } : entry
    )
    assertEquals(checkCoverage(basenameOnly).missing, [`whole file: ${file}`])
  }
})

Deno.test("coverage recognizes map sources after they move into a subdirectory", () => {
  const paths = [
    { filename: "/src/map/mapbuffer.cpp", root: "/repo" },
    { filename: "/repo/src/map/mapbuffer.cpp", root: "/repo" },
    { filename: "C:\\repo\\src\\map\\mapbuffer.cpp", root: "C:\\repo" },
  ]
  for (const { filename, root } of paths) {
    const file = sourcePath(filename, root)
    assertEquals(file, "map/mapbuffer.cpp")
    const functions = covered().map((entry) =>
      entry.file === "map/mapbuffer.cpp" ? { ...entry, file } : entry
    )
    assertEquals(checkCoverage(functions).passed, true)
    const target = functions.find((entry) => entry.file === file)!
    target.counts = { ...complete, missedBranches: 1 }
    assertEquals(checkCoverage(functions).passed, false)
  }
})

Deno.test("an unrelated file with the same basename cannot satisfy map coverage", () => {
  const outside = sourcePath("/vendor/mapbuffer.cpp", "/repo")
  const functions = covered().map((entry) =>
    entry.file === "map/mapbuffer.cpp" ? { ...entry, file: outside } : entry
  )
  assertEquals(checkCoverage(functions).passed, false)
  assertEquals(checkCoverage(functions).missing.length, scope["map/mapbuffer.cpp"].length)
})

Deno.test("native LLVM reports retain counts rather than rounded percentages", () => {
  const report = parseReport(`File '/src/rot.cpp':
Name Regions Miss Cover Lines Miss Cover Branches Miss Cover
symbol 1000000 1 100.00% 1000000 1 100.00% 1000000 1 100.00%
no_branch 1 0 100.00% 3 0 100.00% 0 0 0.00%
`)
  assertEquals(report.get("symbol"), {
    regions: 1000000,
    missedRegions: 1,
    lines: 1000000,
    missedLines: 1,
    branches: 1000000,
    missedBranches: 1,
  })
  assertEquals(report.get("no_branch"), {
    regions: 1,
    missedRegions: 0,
    lines: 3,
    missedLines: 0,
    branches: 0,
    missedBranches: 0,
  })
  assertEquals(report.size, 2)
})
