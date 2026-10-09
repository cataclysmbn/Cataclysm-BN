#!/usr/bin/env -S deno run --allow-read --allow-write --allow-run=sqlite3
/**
 * @module
 * Strict whole-save comparison for quiescent native replay profiles.
 *
 * Compares every file and every logical SQLite files record, ignoring only JSON
 * object member order/whitespace and SQLite physical layout. Arrays and numeric
 * spellings remain exact. Duplicate JSON keys and unknown database schemas are
 * errors, not equality. Originals are opened immutable and checked before/after.
 */
import { Command } from "@cliffy/command"
import { walk } from "@std/fs"
import {
  basename,
  dirname,
  isAbsolute,
  join,
  relative,
  resolve,
  SEPARATOR,
  toFileUrl,
} from "@std/path"
import * as v from "@valibot/valibot"

type JsonNode =
  | { kind: "scalar"; value: string }
  | { kind: "array"; values: JsonNode[] }
  | { kind: "object"; values: Map<string, JsonNode> }

/** Parse without losing duplicate keys, large integers, or numeric spellings. */
export function strict_json(text: string): JsonNode {
  let cursor = 0
  const fail = (message: string): never => {
    throw new Error(`${message} at offset ${cursor}`)
  }
  const whitespace = () => {
    while (/[\t\n\r ]/.test(text[cursor] ?? "!") && cursor < text.length) cursor++
  }
  const string = (): string => {
    const start = cursor
    if (text[cursor++] !== '"') fail("Expected string")
    while (cursor < text.length) {
      const char = text[cursor++]
      if (char === "\\") cursor++
      else if (char === '"') return JSON.parse(text.slice(start, cursor))
    }
    return fail("Unterminated string")
  }
  const value = (): JsonNode => {
    whitespace()
    const char = text[cursor]
    if (char === '"') return { kind: "scalar", value: JSON.stringify(string()) }
    if (char === "{" || char === "[") {
      cursor++
      whitespace()
      const end = char === "{" ? "}" : "]"
      const members = new Map<string, JsonNode>()
      const elements: JsonNode[] = []
      if (text[cursor] !== end) {
        while (true) {
          whitespace()
          if (char === "{") {
            const key = string()
            if (members.has(key)) fail(`Duplicate JSON key ${JSON.stringify(key)}`)
            whitespace()
            if (text[cursor++] !== ":") fail("Expected colon")
            members.set(key, value())
          } else elements.push(value())
          whitespace()
          if (text[cursor] === end) break
          if (text[cursor++] !== ",") fail("Expected comma")
        }
      }
      cursor++
      return char === "{"
        ? { kind: "object", values: members }
        : { kind: "array", values: elements }
    }
    const token = /^(?:true|false|null|-?(?:0|[1-9]\d*)(?:\.\d+)?(?:[eE][+-]?\d+)?)/.exec(
      text.slice(cursor),
    )?.[0]
    if (!token) return fail("Expected JSON value")
    cursor += token.length
    return { kind: "scalar", value: token }
  }
  const result = value()
  whitespace()
  if (cursor !== text.length) fail("Trailing JSON data")
  return result
}

export function canonical(node: JsonNode): string {
  switch (node.kind) {
    case "scalar":
      return node.value
    case "array":
      return `[${node.values.map(canonical).join(",")}]`
    case "object":
      return `{${
        [...node.values.keys()].sort().map((key) =>
          `${JSON.stringify(key)}:${canonical(node.values.get(key)!)}`
        ).join(",")
      }}`
  }
}

type Difference = { path: string; left?: string; right?: string }

/** Count every differing leaf/membership entry; samples alone may be bounded. */
export function differences(left: JsonNode, right: JsonNode) {
  let count = 0
  const samples: Difference[] = []
  const add = (path: string, a?: JsonNode, b?: JsonNode) => {
    count++
    if (samples.length < 100) {
      samples.push({
        path,
        left: a ? canonical(a).slice(0, 500) : undefined,
        right: b ? canonical(b).slice(0, 500) : undefined,
      })
    }
  }
  const visit = (a: JsonNode, b: JsonNode, path: string) => {
    if (a.kind === "object" && b.kind === "object") {
      for (const key of [...new Set([...a.values.keys(), ...b.values.keys()])].sort()) {
        const child = `${path}/${key.replaceAll("~", "~0").replaceAll("/", "~1")}`
        const av = a.values.get(key)
        const bv = b.values.get(key)
        if (!av || !bv) add(child, av, bv)
        else visit(av, bv, child)
      }
    } else if (a.kind === "array" && b.kind === "array") {
      for (let index = 0; index < Math.max(a.values.length, b.values.length); index++) {
        const av = a.values[index]
        const bv = b.values[index]
        if (!av || !bv) add(`${path}/${index}`, av, bv)
        else visit(av, bv, `${path}/${index}`)
      }
    } else if (canonical(a) !== canonical(b)) add(path, a, b)
  }
  visit(left, right, "")
  return { count, samples, samples_truncated: count > samples.length }
}

const sha256 = async (bytes: Uint8Array): Promise<string> =>
  [...new Uint8Array(await crypto.subtle.digest("SHA-256", new Uint8Array(bytes)))]
    .map((byte) => byte.toString(16).padStart(2, "0")).join("")

type FileEvidence = { path: string; size: number; sha256: string; mtime: number | null }

export async function manifest(root: string): Promise<FileEvidence[]> {
  const result: FileEvidence[] = []
  for await (const entry of walk(root, { includeDirs: false, followSymlinks: false })) {
    if (!entry.isFile || entry.isSymlink) throw new Error(`Unsupported save entry: ${entry.path}`)
    const stat = await Deno.stat(entry.path)
    if (entry.name.endsWith("-wal") && stat.size !== 0) {
      throw new Error(`Nonempty WAL: ${entry.path}; stop writers and take a consistent snapshot`)
    }
    result.push({
      path: relative(root, entry.path),
      size: stat.size,
      sha256: await sha256(await Deno.readFile(entry.path)),
      mtime: stat.mtime?.getTime() ?? null,
    })
  }
  return result.sort((a, b) => a.path.localeCompare(b.path))
}

const table_info_schema = v.array(v.strictObject({
  cid: v.number(),
  name: v.string(),
  type: v.string(),
  notnull: v.number(),
  dflt_value: v.nullable(v.string()),
  pk: v.number(),
}))
const row_schema = v.array(v.strictObject({
  path: v.string(),
  parent: v.string(),
  compression: v.nullable(v.string()),
  data_type: v.literal("blob"),
  data_hex: v.string(),
}))

export function validate_table_info(input: unknown) {
  const actual = v.parse(table_info_schema, input)
  const expected = [
    { cid: 0, name: "path", type: "TEXT", notnull: 1, dflt_value: null, pk: 1 },
    { cid: 1, name: "parent", type: "TEXT", notnull: 1, dflt_value: null, pk: 0 },
    { cid: 2, name: "compression", type: "TEXT", notnull: 0, dflt_value: "NULL", pk: 0 },
    { cid: 3, name: "data", type: "BLOB", notnull: 1, dflt_value: null, pk: 0 },
  ]
  if (
    canonical(strict_json(JSON.stringify(actual))) !==
      canonical(strict_json(JSON.stringify(expected)))
  ) {
    throw new Error(`Unknown SQLite files schema: ${JSON.stringify(actual)}`)
  }
  return actual
}

async function query(database: string, sql: string): Promise<unknown> {
  const url = toFileUrl(resolve(database))
  url.search = "?mode=ro&immutable=1"
  const result = await new Deno.Command("sqlite3", {
    args: ["-readonly", "-json", url.href, sql],
    clearEnv: true,
    stdout: "piped",
    stderr: "piped",
  }).output()
  if (!result.success) throw new Error(new TextDecoder().decode(result.stderr))
  const text = new TextDecoder().decode(result.stdout).trim()
  // sqlite3 itself owns JSON generation; still reject duplicate output keys.
  return text ? JSON.parse(canonical(strict_json(text))) : []
}

export async function decode_blob(hex: string, compression: string | null): Promise<Uint8Array> {
  if (!/^(?:[0-9a-fA-F]{2})*$/.test(hex)) throw new Error("Malformed SQLite blob hex")
  const bytes = Uint8Array.from(hex.match(/../g) ?? [], (byte) => parseInt(byte, 16))
  if (compression === null || compression === "") return bytes
  if (compression !== "zlib") throw new Error(`Unknown compression: ${compression}`)
  const stream = new Blob([bytes]).stream().pipeThrough(new DecompressionStream("deflate"))
  return new Uint8Array(await new Response(stream).arrayBuffer())
}

/** Known save JSON formats only; unknown files are compared byte-for-byte. */
export function payload(bytes: Uint8Array, name: string): JsonNode {
  const json = /\.(?:json|sav|gsav|map|mmr)$/.test(name) ||
    /(?:^|\/)(?:o|\.seen)\.-?\d+\.-?\d+$/.test(name)
  if (!json) {
    return { kind: "scalar", value: JSON.stringify([...bytes]) }
  }
  const text = new TextDecoder("utf-8", { fatal: true }).decode(bytes)
  const newline = text.indexOf("\n")
  const header = text.startsWith("#") && newline >= 0 ? text.slice(0, newline) : ""
  return {
    kind: "object",
    values: new Map([
      ["header", { kind: "scalar", value: JSON.stringify(header) }],
      ["json", strict_json(header ? text.slice(newline + 1) : text)],
    ]),
  }
}

export async function records(root: string, files: FileEvidence[]): Promise<Map<string, JsonNode>> {
  const result = new Map<string, JsonNode>()
  const insert = (key: string, node: JsonNode) => {
    if (result.has(key)) throw new Error(`Duplicate logical record: ${key}`)
    result.set(key, node)
  }
  for (const file of files) {
    const path = join(root, file.path)
    if (!file.path.endsWith(".sqlite3")) {
      insert(file.path, payload(await Deno.readFile(path), file.path))
      continue
    }
    const objects = await query(
      path,
      "SELECT type,name,tbl_name,sql FROM sqlite_master ORDER BY type,name;",
    )
    const expected_sql =
      "CREATE TABLE files ( path TEXT PRIMARY KEY NOT NULL, parent TEXT NOT NULL, compression TEXT DEFAULT NULL, data BLOB NOT NULL )"
    const objects_schema = v.array(v.strictObject({
      type: v.string(),
      name: v.string(),
      tbl_name: v.string(),
      sql: v.nullable(v.string()),
    }))
    const schema = v.parse(objects_schema, objects)
    if (
      schema.length !== 2 || !schema.some((entry) =>
        entry.type === "index" && entry.name === "sqlite_autoindex_files_1" &&
        entry.tbl_name === "files" && entry.sql === null
      ) || !schema.some((entry) =>
        entry.type === "table" && entry.name === "files" && entry.tbl_name === "files" &&
        entry.sql?.replace(/\s+/g, " ").trim() === expected_sql
      )
    ) {
      throw new Error(`Unknown SQLite schema in ${path}: ${JSON.stringify(schema)}`)
    }
    const table_info = validate_table_info(await query(path, "PRAGMA table_info(files);"))
    const integrity = await query(path, "PRAGMA integrity_check;")
    if (JSON.stringify(integrity) !== '[{"integrity_check":"ok"}]') {
      throw new Error(`SQLite integrity failure: ${path}`)
    }
    // Retain the empty database and its full schema as a logical record too.
    insert(
      file.path,
      strict_json(JSON.stringify({ schema, table_info })),
    )
    const rows = v.parse(
      row_schema,
      await query(
        path,
        "SELECT path,parent,compression,typeof(data) AS data_type,hex(data) AS data_hex FROM files ORDER BY path;",
      ),
    )
    for (const row of rows) {
      const bytes = await decode_blob(row.data_hex, row.compression)
      insert(`${file.path}::${row.path}`, {
        kind: "object",
        values: new Map([
          ["parent", strict_json(JSON.stringify(row.parent))],
          ["compression", strict_json(JSON.stringify(row.compression))],
          ["payload", payload(bytes, row.path)],
        ]),
      })
    }
  }
  return result
}

export async function compare(left: string, right: string, output: string) {
  // The exclusive, nonrecursive mkdir requires a new leaf and an existing parent.
  // Resolve that parent through filesystem aliases before creating any evidence.
  const canonical_output = join(await Deno.realPath(dirname(output)), basename(output))
  for (const input of [left, right]) {
    // Match the input normalization performed by walk() and join().
    const subpath = relative(await Deno.realPath(resolve(input)), canonical_output)
    if (
      subpath === "" ||
      (!isAbsolute(subpath) && subpath !== ".." && !subpath.startsWith(`..${SEPARATOR}`))
    ) {
      throw new Error(`Comparison output must be outside both input directories: ${output}`)
    }
  }
  // Keep mkdir and all report writes on the same checked filesystem path.
  output = canonical_output
  await Deno.mkdir(output, { recursive: false }) // refuse overwriting previous evidence
  const before = { left: await manifest(left), right: await manifest(right) }
  await Deno.writeTextFile(
    join(output, "membership-before.json"),
    JSON.stringify(before, null, 2) + "\n",
  )
  try {
    const a = await records(left, before.left)
    const b = await records(right, before.right)
    const changes = []
    for (const key of [...new Set([...a.keys(), ...b.keys()])].sort()) {
      const av = a.get(key)
      const bv = b.get(key)
      if (!av || !bv) changes.push({ record: key, membership: av ? "left-only" : "right-only" })
      else {
        const difference = differences(av, bv)
        if (difference.count) changes.push({ record: key, ...difference })
      }
    }
    const after = { left: await manifest(left), right: await manifest(right) }
    const unchanged = JSON.stringify(before) === JSON.stringify(after)
    await Deno.writeTextFile(
      join(output, "membership-after.json"),
      JSON.stringify(after, null, 2) + "\n",
    )
    if (!unchanged) {
      throw new Error("Inspection changed source evidence or a writer is still active")
    }
    for (const [name, data] of [["left", a], ["right", b]] as const) {
      await Deno.writeTextFile(
        join(output, `${name}-records.jsonl`),
        [...data].map(([key, node]) =>
          `{"record":${JSON.stringify(key)},"value":${canonical(node)}}\n`
        ).join(""),
      )
    }
    const summary = {
      left,
      right,
      left_records: a.size,
      right_records: b.size,
      logical_records_compared: new Set([...a.keys(), ...b.keys()]).size,
      all_logical_records_equal: changes.length === 0,
      immutable_inspection_preserved_sources: unchanged,
      changes,
      physical_files_different: [
        ...new Set([...before.left, ...before.right].map((file) => file.path)),
      ]
        .filter((path) =>
          before.left.find((file) => file.path === path)?.sha256 !==
            before.right.find((file) => file.path === path)?.sha256
        ),
      rng_gate: "unverified: replay header records seed only, not full RNG state",
    }
    await Deno.writeTextFile(join(output, "summary.json"), JSON.stringify(summary, null, 2) + "\n")
    return summary
  } catch (error) {
    const after = { left: await manifest(left), right: await manifest(right) }
    await Deno.writeTextFile(
      join(output, "error.json"),
      JSON.stringify(
        {
          error: error instanceof Error ? error.message : String(error),
          immutable_inspection_preserved_sources: JSON.stringify(before) === JSON.stringify(after),
        },
        null,
        2,
      ) + "\n",
    )
    throw error
  }
}

if (import.meta.main) {
  await new Command()
    .name("compare-saves")
    .description(
      "Compare complete quiescent save directories; refuses unknown schemas and duplicate JSON keys.",
    )
    .arguments("<left:string> <right:string> <output:string>")
    .action(async (_options, left, right, output) => {
      try {
        const result = await compare(resolve(left), resolve(right), resolve(output))
        console.log(JSON.stringify(result, null, 2))
        if (!result.all_logical_records_equal) Deno.exitCode = 1
      } catch (error) {
        console.error(error instanceof Error ? error.message : String(error))
        Deno.exitCode = 2
      }
    })
    .parse(Deno.args)
}
