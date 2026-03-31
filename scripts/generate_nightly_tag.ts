#!/usr/bin/env -S deno run --allow-run=git --allow-env=TAG_DATE,GITHUB_OUTPUT --allow-write=GITHUB_OUTPUT

import * as semver from "jsr:@std/semver"

const tagDate = Deno.env.get("TAG_DATE")
const outputPath = Deno.env.get("GITHUB_OUTPUT")

if (!tagDate || !outputPath) {
  throw new Error("Missing TAG_DATE or GITHUB_OUTPUT")
}

const { stdout } = await new Deno.Command("git", {
  args: ["tag", "--list"],
  stdout: "piped",
}).output()

const latestStable = ((new TextDecoder().decode(stdout).split(/\r?\n/).filter(Boolean))
  .flatMap((tag) => semver.tryParse(tag) ?? []))
  .toSorted(semver.compare)
  .at(-1)

if (!latestStable) {
  throw new Error("No stable semver tags found.")
}

const nightlyTag = `${semver.increment(latestStable, "patch")}-dev.${tagDate}`
const releaseName = `Nightly ${nightlyTag}`

await Deno.writeTextFile(
  outputPath,
  `tag_name=${nightlyTag}\nrelease_name=${releaseName}\n`,
  { append: true },
)
