#!/usr/bin/env -S deno run --allow-read --allow-write
/** Tools for merging and validating gettext POT files. */

import PO from "pofile"

const decoder = new TextDecoder("utf-8", { fatal: true })

type PoItem = {
  msgid: string
  msgctxt?: string | null
  msgid_plural?: string | null
  msgstr?: string[]
  references?: string[]
  extractedComments?: string[]
  flags?: Record<string, boolean>
}

type PoFile = {
  headers: Record<string, string>
  headerOrder?: string[]
  comments?: string[]
  items: PoItem[]
}

const usage = () => {
  console.error("usage: pot_tools.ts <concat|dedup|unicode-check> ...")
  console.error("  concat INPUT1 INPUT2 OUTPUT")
  console.error("  dedup FILE")
  console.error("  unicode-check FILE")
  Deno.exit(1)
}

const readText = async (path: string): Promise<string> => decoder.decode(await Deno.readFile(path))

const headerOrder = (text: string): string[] => {
  const order: string[] = []
  const headerMatch = text.match(/msgid ""\nmsgstr ""\n(?<headers>(?:"(?:\\.|[^"\\])*"\n)+)/)
  if (!headerMatch?.groups?.headers) return order
  for (const match of headerMatch.groups.headers.matchAll(/^"(.*)\\n"$/gm)) {
    const header = match[1].replaceAll('\\"', '"').replaceAll("\\\\", "\\")
    const colon = header.indexOf(":")
    if (colon > 0) order.push(header.slice(0, colon))
  }
  return order
}

const readPo = async (path: string): Promise<PoFile> => {
  const text = await readText(path)
  const po = PO.parse(text) as PoFile
  po.headerOrder = headerOrder(text)
  return po
}

const poEscape = (text: string): string =>
  text
    .replaceAll("\\", "\\\\")
    .replaceAll("\t", "\\t")
    .replaceAll("\r", "\\r")
    .replaceAll("\n", "\\n")
    .replaceAll('"', '\\"')

const wrapText = (text: string, width: number): string[] => {
  if (text.length <= width) return [text]
  const chunks = text.match(/ +|[^ ]+/g) ?? []
  const lines: string[] = []
  let current = ""
  let pendingSpace = ""
  for (const chunk of chunks) {
    if (chunk.trim() === "") {
      pendingSpace += chunk
      continue
    }
    const separator = current || pendingSpace ? pendingSpace : ""
    const candidate = `${current}${separator}${chunk}`
    if (candidate.length <= width) {
      current = candidate
      pendingSpace = ""
      continue
    }
    const remainingWidth = width - current.length - separator.length
    const hyphenIndex = chunk.lastIndexOf("-", remainingWidth)
    if (current && hyphenIndex > 0) {
      lines.push(`${current}${separator}${chunk.slice(0, hyphenIndex + 1)}`)
      current = chunk.slice(hyphenIndex + 1)
      pendingSpace = ""
      continue
    }
    if (current) lines.push(`${current}${pendingSpace}`)
    current = chunk
    pendingSpace = ""
  }
  if (current || pendingSpace || lines.length === 0) lines.push(`${current}${pendingSpace}`)
  return lines
}

const poField = (key: string, value: string, pluralIndex = ""): string[] => {
  const escaped = poEscape(value)
  const prefix = `${key}${pluralIndex}`
  const firstLineWidth = 78 - prefix.length - 3
  if (!escaped.includes("\\n") && escaped.length <= firstLineWidth) {
    return [`${prefix} "${escaped}"`]
  }
  return [`${prefix} ""`, ...wrapText(escaped, 76).map((line) => `"${line}"`)]
}

const wrapPoComment = (prefix: string, comment: string): string[] => {
  if (comment.length + prefix.length <= 78) return [`${prefix}${comment}`]
  return wrapText(comment.replaceAll(/\s/g, " "), 78 - prefix.length).map((line) =>
    `${prefix}${line.trimEnd()}`
  )
}

const normalizeReference = (reference: string): string => reference.replace(/:\d+$/, "")

const normalizedReferences = (
  references: string[] | undefined,
): string[] => [
  ...new Set(
    (references ?? []).flatMap((reference) => reference.split(/\s+/)).filter(Boolean).map(
      normalizeReference,
    ),
  ),
]

const firstOccurrence = (entry: PoItem): string => entry.references?.[0] ?? "unknown_file"

const itemFlags = (entry: PoItem): string[] =>
  Object.entries(entry.flags ?? {}).filter(([, enabled]) => enabled).map(([flag]) => flag)

const itemComment = (entry: PoItem): string => (entry.extractedComments ?? []).join("\n")

const cloneItem = (entry: PoItem): PoItem => ({
  msgid: entry.msgid,
  msgctxt: entry.msgctxt ?? null,
  msgid_plural: entry.msgid_plural ?? null,
  msgstr: entry.msgid_plural ? ["", ""] : [""],
  references: normalizedReferences(entry.references),
  extractedComments: itemComment(entry) ? [itemComment(entry)] : [],
  flags: Object.fromEntries(
    itemFlags(entry).filter((flag) => flag === "c-format").map((flag) => [flag, true]),
  ),
})

const mergeItem = (target: PoItem, source: PoItem) => {
  const sourceComment = itemComment(source)
  if (sourceComment && !(target.extractedComments ?? []).includes(sourceComment)) {
    target.extractedComments = [...(target.extractedComments ?? []), sourceComment]
  }

  target.flags = target.flags ?? {}
  for (const flag of itemFlags(source).filter((flag) => flag === "c-format")) {
    target.flags[flag] = true
  }

  const refs = target.references ?? []
  for (const normalized of normalizedReferences(source.references)) {
    if (!refs.includes(normalized)) refs.push(normalized)
  }
  target.references = refs

  if (!target.msgid_plural && source.msgid_plural) target.msgid_plural = source.msgid_plural
  else if (
    target.msgid_plural && source.msgid_plural && target.msgid_plural !== source.msgid_plural
  ) {
    console.log(
      `WARNING: plural form mismatch for msgid='${target.msgid}' msgctxt='${target.msgctxt}' in ${
        firstOccurrence(target)
      } and ${firstOccurrence(source)}: '${target.msgid_plural}' vs '${source.msgid_plural}'`,
    )
  }
  target.msgstr = target.msgid_plural ? ["", ""] : [""]
}

const dedupItems = (items: PoItem[]): PoItem[] => {
  const deduped: PoItem[] = []
  const indexes = new Map<string, number>()
  for (const entry of items) {
    const normalized = cloneItem(entry)
    const flags = itemFlags(normalized)
    if (flags.length > 0 && !(flags.length === 1 && flags[0] === "c-format")) {
      console.log(
        `WARNING: unexpected flags ${JSON.stringify(flags)} for msgid='${normalized.msgid}' in ${
          firstOccurrence(normalized)
        }`,
      )
    }
    const key = `${normalized.msgctxt ?? ""}\u0000${normalized.msgid}`
    const existing = indexes.get(key)
    if (existing === undefined) {
      indexes.set(key, deduped.length)
      deduped.push(normalized)
    } else mergeItem(deduped[existing], normalized)
  }
  return deduped
}

const formatPo = (po: PoFile, items: PoItem[]): string => {
  const lines = ["#", 'msgid ""', 'msgstr ""']
  const headerOrder = po.headerOrder?.length ? po.headerOrder : Object.keys(po.headers)
  for (const key of headerOrder) {
    if (po.headers[key] !== undefined) lines.push(`"${poEscape(`${key}: ${po.headers[key]}\n`)}"`)
  }
  lines.push("")

  for (const item of items) {
    for (const comment of item.extractedComments ?? []) {
      for (const line of comment.split("\n")) lines.push(...wrapPoComment("#. ", line))
    }
    const references = item.references ?? []
    if (references.length > 0) lines.push(...wrapPoComment("#: ", references.join(" ")))
    const flags = itemFlags(item)
    if (flags.length > 0) lines.push(`#, ${flags.join(", ")}`)
    if (item.msgctxt) lines.push(...poField("msgctxt", item.msgctxt))
    lines.push(...poField("msgid", item.msgid))
    if (item.msgid_plural) {
      lines.push(...poField("msgid_plural", item.msgid_plural))
      lines.push('msgstr[0] ""')
      lines.push('msgstr[1] ""')
    } else lines.push('msgstr ""')
    lines.push("")
  }
  return `${lines.join("\n")}`
}

const concat = async ([input1, input2, output]: string[]) => {
  if (!input1 || !input2 || !output) usage()
  console.log(`==> Merging '${input1}' and '${input2}' into '${output}`)
  const po1 = await readPo(input1)
  const po2 = await readPo(input2)
  await Deno.writeTextFile(output, formatPo(po1, [...po1.items, ...po2.items]))
}

const dedup = async ([file]: string[]) => {
  if (!file) usage()
  const po = await readPo(file)
  await Deno.writeTextFile(file, formatPo(po, dedupItems(po.items)))
}

const unicodeCheck = async ([file]: string[]) => {
  if (!file) usage()
  await readText(file)
}

const [command, ...args] = Deno.args
if (command === "concat") await concat(args)
else if (command === "dedup") await dedup(args)
else if (command === "unicode-check") await unicodeCheck(args)
else usage()
