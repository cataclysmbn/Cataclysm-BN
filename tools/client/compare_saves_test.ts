#!/usr/bin/env -S deno test --allow-read --allow-write --allow-run=sqlite3
/** @module Tests for lossless, complete native replay save comparison. */
import { assert, assertEquals, assertRejects, assertThrows } from "@std/assert"
import { walk } from "@std/fs"
import { join, relative } from "@std/path"
import {
  canonical,
  compare,
  decode_blob,
  differences,
  manifest,
  payload,
  records,
  strict_json,
  validate_table_info,
} from "./compare_saves.ts"

Deno.test("JSON objects ignore member order but arrays retain order and membership", () => {
  assertEquals(canonical(strict_json('{"b":2,"a":1}')), canonical(strict_json('{ "a":1,"b":2 }')))
  assertEquals(differences(strict_json("[1,2]"), strict_json("[2,1]")).count, 2)
  assertEquals(differences(strict_json("[1,2]"), strict_json("[1]")).count, 1)
  assertEquals(differences(strict_json('{"a":null}'), strict_json("{}")).count, 1)
})

Deno.test("duplicate keys including escaped equivalents are errors at any depth", () => {
  for (const text of ['{"a":1,"a":2}', '{"a":1,"\\u0061":2}', '[{"x":{"a":1,"a":2}}]']) {
    assertThrows(() => strict_json(text), Error, "Duplicate JSON key")
  }
})

Deno.test("numbers retain spelling, large integer precision, and scalar type", () => {
  for (
    const [left, right] of [["1", "1.0"], ["1", "true"], ["0", "-0"], [
      "9007199254740992",
      "9007199254740993",
    ], ["1e2", "100"]]
  ) {
    assertEquals(differences(strict_json(left), strict_json(right)).count, 1)
  }
})

Deno.test("malformed JSON and trailing data cannot silently become opaque text", () => {
  for (
    const text of [
      "[1,]",
      '{"a":}',
      "01",
      "true false",
      '"unterminated',
      '"bad\nstring"',
      "NaN",
      "",
    ]
  ) {
    assertThrows(() => strict_json(text))
  }
  assertThrows(
    () => payload(new TextEncoder().encode('{"a":1,"a":2}'), "lua_state.json"),
    Error,
    "Duplicate JSON key",
  )
})

Deno.test("save version headers are retained and unknown files compare byte-for-byte", () => {
  const encode = (text: string) => new TextEncoder().encode(text)
  const a = payload(encode('# version 1\n{"a":1}'), "avatar.sav")
  const b = payload(encode('# version 2\n{"a":1}'), "avatar.sav")
  assertEquals(differences(a, b).count, 1)
  assertEquals(
    differences(payload(encode("a b"), "unknown.bin"), payload(encode("ab"), "unknown.bin")).count,
    1,
  )
})

Deno.test("difference samples are bounded without bounding the comparison", () => {
  const result = differences(
    strict_json(JSON.stringify(Array(150).fill(1))),
    strict_json(JSON.stringify(Array(150).fill(2))),
  )
  assertEquals(result.count, 150)
  assertEquals(result.samples.length, 100)
  assert(result.samples_truncated)
})

Deno.test("unknown SQLite columns and compression fail closed", async () => {
  assertThrows(() => validate_table_info([]), Error, "Unknown SQLite files schema")
  await assertRejects(() => decode_blob("78", "gzip"), Error, "Unknown compression")
  await assertRejects(() => decode_blob("x", null), Error, "Malformed SQLite blob hex")
})

Deno.test("zlib decoding compares the complete payload", async () => {
  const bytes = new TextEncoder().encode('{"array":[2,1]}')
  const stream = new Blob([bytes]).stream().pipeThrough(new CompressionStream("deflate"))
  const compressed = new Uint8Array(await new Response(stream).arrayBuffer())
  const hex = [...compressed].map((byte) => byte.toString(16).padStart(2, "0")).join("")
  assertEquals(await decode_blob(hex, "zlib"), bytes)
  assertEquals(await decode_blob("0001ff", null), new Uint8Array([0, 1, 255]))
})

async function fixture(run: (root: string) => Promise<void>) {
  const root = await Deno.makeTempDir({ prefix: "bn-compare-saves-test-" })
  try {
    await run(root)
  } finally {
    await Deno.remove(root, { recursive: true })
  }
}

async function sqlite(path: string, sql: string) {
  const result = await new Deno.Command("sqlite3", {
    args: [path, sql],
    clearEnv: true,
    stdout: "piped",
    stderr: "piped",
  }).output()
  assert(result.success, new TextDecoder().decode(result.stderr))
}

const create = `CREATE TABLE files (
            path           TEXT PRIMARY KEY NOT NULL,
            parent         TEXT NOT NULL,
            compression    TEXT DEFAULT NULL,
            data           BLOB NOT NULL
        );`

Deno.test("complete comparison includes files, empty databases and logical record membership; originals unchanged", async () => {
  await fixture(async (root) => {
    const left = join(root, "left")
    const right = join(root, "right")
    for (const dir of [left, right]) {
      await Deno.mkdir(dir)
      await Deno.writeTextFile(join(dir, "avatar.sav"), '# version 1\n{"player":{"all":[1,2]}}')
      // '#' must be URI-escaped during immutable reads.
      await sqlite(
        join(dir, "#avatar.sqlite3"),
        create + "INSERT INTO files VALUES('memory.mmr','',NULL,CAST('[1,2]' AS BLOB));",
      )
      await sqlite(join(dir, "empty.sqlite3"), create)
    }
    const before = await manifest(left)
    const result = await compare(left, right, join(root, "equal"))
    assert(result.all_logical_records_equal)
    assertEquals(result.logical_records_compared, 4)
    assertEquals(await manifest(left), before)
    await sqlite(
      join(right, "#avatar.sqlite3"),
      "INSERT INTO files VALUES('extra.map','',NULL,CAST('[]' AS BLOB));",
    )
    await Deno.writeTextFile(join(right, "extra.log"), "log")
    const changed = await compare(left, right, join(root, "changed"))
    assert(!changed.all_logical_records_equal)
    assertEquals(changed.changes.length, 2)
    assert(changed.immutable_inspection_preserved_sources)
    await assertRejects(
      () => compare(left, right, join(root, "changed")),
      Deno.errors.AlreadyExists,
    )
  })
})

Deno.test("SQLite page layout is provenance, not authoritative equality", async () => {
  await fixture(async (root) => {
    const left = join(root, "left")
    const right = join(root, "right")
    for (const directory of [left, right]) {
      await Deno.mkdir(directory)
      await sqlite(
        join(directory, "map.sqlite3"),
        create + "INSERT INTO files VALUES('x.map','maps',NULL,CAST('[1,2]' AS BLOB));",
      )
    }
    await sqlite(join(right, "map.sqlite3"), "PRAGMA page_size=8192; VACUUM;")
    const result = await compare(left, right, join(root, "report"))
    assert(result.all_logical_records_equal)
    assertEquals(result.logical_records_compared, 2)
    assertEquals(result.physical_files_different, ["map.sqlite3"])
    assert(result.immutable_inspection_preserved_sources)
  })
})

Deno.test("overmap visibility payloads reject duplicate keys instead of opaque comparison", () => {
  assertThrows(
    () =>
      payload(new TextEncoder().encode('# version 1\n{"visible":[],"visible":[]}'), ".seen.0.0"),
    Error,
    "Duplicate JSON key",
  )
})

Deno.test("invalid logical SQLite JSON and compression produce error evidence without changing originals", async () => {
  await fixture(async (root) => {
    for (
      const [compression, text, message] of [
        ["NULL", '{"a":1,"a":2}', "Duplicate JSON key"],
        ["'gzip'", "[]", "Unknown compression"],
      ]
    ) {
      const dir = await Deno.makeTempDir({ dir: root })
      const database = join(dir, "map.sqlite3")
      await sqlite(
        database,
        create + `INSERT INTO files VALUES('x.map','',${compression},CAST('${text}' AS BLOB));`,
      )
      const before = await manifest(dir)
      const output = await Deno.makeTempDir({ dir: root })
      const report = join(output, "report")
      await assertRejects(() => compare(dir, dir, report), Error, message)
      assertEquals(await manifest(dir), before)
      const error = JSON.parse(await Deno.readTextFile(join(report, "error.json")))
      assert(error.error.includes(message))
      assert(error.immutable_inspection_preserved_sources)
    }
  })
})

Deno.test("nonempty WAL prevents immutable inspection", async () => {
  await fixture(async (root) => {
    await Deno.writeTextFile(join(root, "map.sqlite3-wal"), "pending writer")
    await assertRejects(() => manifest(root), Error, "Nonempty WAL")
  })
})

Deno.test("extra SQLite tables, indexes, or triggers are unsupported schemas", async () => {
  await fixture(async (root) => {
    for (
      const sql of [
        "CREATE TABLE extra(x);",
        "CREATE INDEX extra ON files(parent);",
        "CREATE TRIGGER extra AFTER INSERT ON files BEGIN SELECT 1; END;",
      ]
    ) {
      const dir = await Deno.makeTempDir({ dir: root })
      await sqlite(join(dir, "map.sqlite3"), create + sql)
      // Read a new manifest for each database, not a stale listing.
      const files = await manifest(dir)
      await assertRejects(() => records(dir, files), Error, "Unknown SQLite schema")
    }
  })
})

type InputEntry = {
  path: string
  kind: "directory" | "file" | "symlink"
  bytes?: number[]
  target?: string
}

// Independent of the comparator's file-only manifest: include empty directories
// and every original byte, so creating an empty report directory also fails.
async function input_snapshot(root: string): Promise<InputEntry[]> {
  const entries: InputEntry[] = []
  for await (const entry of walk(root, { includeDirs: true, followSymlinks: false })) {
    const path = relative(root, entry.path) || "."
    if (entry.isSymlink) {
      entries.push({ path, kind: "symlink", target: await Deno.readLink(entry.path) })
    } else if (entry.isDirectory) {
      entries.push({ path, kind: "directory" })
    } else if (entry.isFile) {
      entries.push({ path, kind: "file", bytes: [...await Deno.readFile(entry.path)] })
    } else {
      throw new Error(`Unsupported fixture entry: ${entry.path}`)
    }
  }
  return entries.sort((left, right) => left.path.localeCompare(right.path))
}

async function json_inputs(root: string) {
  for (const name of ["left", "right"]) {
    const directory = join(root, name)
    await Deno.mkdir(directory)
    await Deno.mkdir(join(directory, "empty"))
    await Deno.mkdir(join(directory, "nested"))
    await Deno.writeTextFile(
      join(directory, "avatar.sav"),
      '{"player":{"inventory":[1,2],"name":"fixture"}}\n',
    )
    await Deno.writeTextFile(join(directory, "nested/notes.json"), '{"ordered":[3,2,1]}\n')
  }
  return { left: join(root, "left"), right: join(root, "right") }
}

for (const side of ["left", "right"] as const) {
  for (const location of ["equal", "nested", "symlink-parent"] as const) {
    Deno.test(`output overlap: ${location} in ${side} rejects before any input creation`, async () => {
      await fixture(async (root) => {
        const inputs = await json_inputs(root)
        let output = inputs[side]
        if (location === "nested") output = join(inputs[side], "report")
        if (location === "symlink-parent") {
          const alias = join(root, "output-parent-alias")
          await Deno.symlink(join(inputs[side], "nested"), alias, { type: "dir" })
          output = join(alias, "report")
        }
        const before = {
          left: await input_snapshot(inputs.left),
          right: await input_snapshot(inputs.right),
        }
        if (location !== "equal") {
          await assertRejects(() => Deno.lstat(output), Deno.errors.NotFound)
        }
        const rejection = await assertRejects(
          () => compare(inputs.left, inputs.right, output),
          Error,
        )
        const after = {
          left: await input_snapshot(inputs.left),
          right: await input_snapshot(inputs.right),
        }
        // Preserve complete before/after evidence in test output before cleanup.
        console.log(JSON.stringify({ side, location, rejection: rejection.message, before, after }))
        assertEquals(
          after,
          before,
          "Overlapping output must be rejected before creating directories or files in either input",
        )
        if (location !== "equal") {
          await assertRejects(() => Deno.lstat(output), Deno.errors.NotFound)
        }
      })
    })
  }
}

Deno.test("output overlap: a separate prefix-sharing sibling output remains valid", async () => {
  await fixture(async (root) => {
    const inputs = await json_inputs(root)
    const before = {
      left: await input_snapshot(inputs.left),
      right: await input_snapshot(inputs.right),
    }
    const output = join(root, "left-report")
    const result = await compare(inputs.left, inputs.right, output)
    assert(result.all_logical_records_equal)
    assertEquals(result.logical_records_compared, 2)
    assertEquals(JSON.parse(await Deno.readTextFile(join(output, "summary.json"))), result)
    assertEquals({
      left: await input_snapshot(inputs.left),
      right: await input_snapshot(inputs.right),
    }, before)
  })
})

Deno.test("canonical ancestry: report writes stay in the checked output parent", async () => {
  await fixture(async (root) => {
    const inputs = await json_inputs(root)
    const external = join(root, "external")
    await Deno.mkdir(join(external, "deep"), { recursive: true })
    await Deno.mkdir(join(external, "left"))
    const alias = join(root, "ancestor-alias")
    await Deno.symlink(join(external, "deep"), alias, { type: "dir" })
    const output = `${alias}/../left/report`
    for (const input of [inputs.left, inputs.right]) {
      await Deno.mkdir(join(input, "report"))
      await Deno.writeTextFile(join(input, "report/original.json"), '{"keep":[2,1]}\n')
    }
    // Deno permission checks may normalize traversal before filesystem lookup.
    // Whatever parent the API checks, every report write must stay there.
    const checked_parent = await Deno.realPath(`${alias}/../left`)
    const before = {
      left: await input_snapshot(inputs.left),
      right: await input_snapshot(inputs.right),
    }
    let result: Awaited<ReturnType<typeof compare>> | undefined
    let error: string | undefined
    try {
      result = await compare(inputs.left, inputs.right, output)
    } catch (failure) {
      error = failure instanceof Error ? failure.message : String(failure)
    }
    const after = {
      left: await input_snapshot(inputs.left),
      right: await input_snapshot(inputs.right),
    }
    console.log(JSON.stringify({ checked_parent, error, before, after }))
    assertEquals(after, before, "Original input trees must survive every report write unchanged")
    if (checked_parent === inputs.left) {
      assertEquals(error, `Comparison output must be outside both input directories: ${output}`)
      assertEquals(result, undefined)
    } else {
      assertEquals(checked_parent, join(external, "left"))
      assertEquals(error, undefined, "Physically separate output must remain valid")
      assert(result)
      assert(result.all_logical_records_equal)
      assertEquals(result.logical_records_compared, 3)
      assertEquals(result.left, inputs.left)
      assertEquals(result.right, inputs.right)
      assertEquals(JSON.parse(await Deno.readTextFile(`${output}/summary.json`)), result)
    }
  })
})
