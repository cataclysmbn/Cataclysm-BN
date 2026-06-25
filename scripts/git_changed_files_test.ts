import { assertEquals } from "@std/assert"
import { parseGitNameStatus } from "./git_changed_files.ts"

Deno.test("parseGitNameStatus() maps native git status output to PR file shape", () => {
  assertEquals(
    parseGitNameStatus([
      "A\tdocs/en/new.md",
      "M\tsrc/foo.cpp",
      "D\tdocs/ko/old.md",
      "R100\tdocs/en/before.md\tdocs/en/after.md",
      "C075\tsrc/original.cpp\tsrc/copied.cpp",
      "T\tdata/file.json",
    ].join("\n")),
    [
      { filename: "docs/en/new.md", status: "added" },
      { filename: "src/foo.cpp", status: "modified" },
      { filename: "docs/ko/old.md", status: "removed" },
      { filename: "docs/en/after.md", previous_filename: "docs/en/before.md", status: "renamed" },
      { filename: "src/copied.cpp", previous_filename: "src/original.cpp", status: "copied" },
      { filename: "data/file.json", status: "changed" },
    ],
  )
})
