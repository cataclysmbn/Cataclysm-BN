#!/usr/bin/env -S deno run -RW

import { partition } from "@std/collections/partition"

/**
 * Converts starlight-style [asides](https://starlight.astro.build/guides/authoring-content/#asides) to
 * [Github-flavored markdown alerts](https://docs.github.com/en/get-started/writing-on-github/getting-started-with-writing-and-formatting-on-github/basic-writing-and-formatting-syntax#alerts).
 */
export const asideToGfm = (aside: string): string =>
  aside.replace(
    regex,
    (_match, ...args) => {
      const { type = "", attrs, content = "" } = args.at(-1) as Record<string, string | undefined>
      const title = attrs?.match(titleRegex)?.groups?.title
      const [links, lines] = partition(content.trim().split("\n"), (line) => linkRegex.test(line))

      return `> [!${getType(type)}]\n>\n${title ? `> #### ${title}\n>\n` : ""}${
        formatLine(lines).concat(links).map((line) => line.trim()).join("\n")
      }`
    },
  )

const getType = (type: string): string => (types[type] ?? type).toUpperCase()
const types: Record<string, string> = {
  note: "NOTE",
  tip: "TIP",
  warning: "WARNING",
  caution: "CAUTION",
  danger: "CAUTION",
}

const formatLine = (lines: string[]) =>
  lines.map((line, i) => isLastLineEmpty(line, i, lines) ? "" : `> ${line}`)

const isLastLineEmpty = (line: string, i: number, lines: string[]) =>
  line === "" && i === lines.length - 1

const regex = /:::(?<type>\w+)(?:\{(?<attrs>[^\}]*)\})?\n+(?<content>(?:.|\n)+?)\n+:::/g
const titleRegex = /title\s*=\s*"(?<title>.*?)"/
const linkRegex = /\[(?<text>[^\]]+)\]:\s*(?<url>.+)/

if (import.meta.main) {
  const { Command } = await import("@cliffy/command")
  const { paragraph } = await import("../changelog/paragraph.ts")
  const { walk } = await import("@std/fs")

  const { args: [path] } = await new Command()
    .description(paragraph`
        Converts starlight-style asides (:::note) to github-flavored markdown alerts ([!NOTE]).
    `)
    .arguments("<path>")
    .parse(Deno.args)

  const files = await Array.fromAsync(walk(path, { exts: [".md"], includeDirs: false }))
  await Promise.allSettled(
    files.map(async ({ path }) => {
      const text = await Deno.readTextFile(path)
      await Deno.writeTextFile(path, asideToGfm(text))
    }),
  )
}
