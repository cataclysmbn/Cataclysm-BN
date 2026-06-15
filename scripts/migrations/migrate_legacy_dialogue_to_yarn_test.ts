import { assert, assertEquals, assertRejects, assertStringIncludes } from "@std/assert"
import { exists } from "@std/fs"
import { join } from "@std/path"

import { convertEntriesToYarn, migratePaths } from "./migrate_legacy_dialogue_to_yarn.ts"

Deno.test("converts simple topics, dynamic lines, and terminals", () => {
  const result = convertEntriesToYarn([
    {
      type: "talk_topic",
      id: "TALK_TEST",
      dynamic_line: "Hello there.",
      responses: [
        { text: "Ask a question.", topic: "TALK_DETAIL" },
        { text: "Bye.", topic: "TALK_DONE" },
      ],
    },
  ])

  assertEquals(result.topicCount, 1)
  assertStringIncludes(result.yarn, "title: TALK_TEST")
  assertStringIncludes(result.yarn, "NPC: Hello there.")
  assertStringIncludes(result.yarn, "-> Ask a question.\n    <<jump TALK_DETAIL>>")
  assertStringIncludes(result.yarn, "-> Bye.\n    <<stop>>")
})

Deno.test("converts mixed dynamic line arrays to one line instead of duplicating every option", () => {
  const result = convertEntriesToYarn([
    {
      type: "talk_topic",
      id: "TALK_MIXED_LINE",
      dynamic_line: ["First.", "Second.", { u_male: ["Sir."], no: ["Ma'am."] }],
      responses: [],
    },
  ])

  assertStringIncludes(result.yarn, "NPC: First.")
  assert(!result.yarn.includes("NPC: Second."))
  assert(!result.yarn.includes("Sir."))
})

Deno.test("converts conditional dynamic lines", () => {
  const result = convertEntriesToYarn([
    {
      type: "talk_topic",
      id: "TALK_COND",
      dynamic_line: {
        npc_has_effect: "infection",
        yes: "Not until I get antibiotics.",
        no: "Why should I travel with you?",
      },
      responses: [],
    },
  ])

  assertStringIncludes(result.yarn, '<<if npc_has_effect("infection")>>')
  assertStringIncludes(result.yarn, "    NPC: Not until I get antibiotics.")
  assertStringIncludes(result.yarn, "<<else>>")
  assertStringIncludes(result.yarn, "    NPC: Why should I travel with you?")
})

Deno.test("converts trials, effects, opinion, and switch fallback guards", () => {
  const result = convertEntriesToYarn([
    {
      type: "talk_topic",
      id: "TALK_TRIAL",
      responses: [
        {
          switch: true,
          text: "Trade?",
          condition: { npc_has_trait: "HALLUCINATION" },
          topic: "TRADE_HALLU",
        },
        {
          switch: true,
          default: true,
          text: "Trade?",
          effect: "start_trade",
          topic: "TALK_DONE",
        },
        {
          text: "Drop it!",
          trial: { type: "INTIMIDATE", difficulty: 30 },
          success: {
            topic: "TALK_WEAPON_DROPPED",
            effect: "drop_weapon",
            opinion: { trust: 4, fear: -3 },
          },
          failure: { topic: "TALK_DONE", effect: "hostile" },
        },
      ],
    },
  ])

  assertStringIncludes(result.yarn, '-> Trade? <<if npc_has_trait("HALLUCINATION")>>')
  assertStringIncludes(result.yarn, '-> Trade? <<if (not npc_has_trait("HALLUCINATION"))>>')
  assertStringIncludes(result.yarn, "    <<start_trade>>\n    <<stop>>")
  assertStringIncludes(result.yarn, '<<if trial_roll("INTIMIDATE", 30)>>')
  assertStringIncludes(result.yarn, "        <<npc_add_trust 4>>")
  assertStringIncludes(result.yarn, "        <<npc_add_fear -3>>")
  assertStringIncludes(result.yarn, "        <<drop_weapon>>")
})

Deno.test("converts repeat responses", () => {
  const result = convertEntriesToYarn([
    {
      type: "talk_topic",
      id: "TALK_REPEAT",
      repeat_responses: {
        for_category: "meds",
        include_containers: true,
        response: {
          text: "Sell <topic_item>.",
          condition: { u_has_ecash: 10 },
          effect: { u_sell_item: "aspirin", cost: 10, count: 1 },
          topic: "TALK_NONE",
        },
      },
    },
  ])

  assertStringIncludes(
    result.yarn,
    '<<repeat_for_category "meds" #include_containers if u_get_ecash() >= 10>>',
  )
  assertStringIncludes(result.yarn, "-> Sell <topic_item>.")
  assertStringIncludes(result.yarn, '    <<u_sell_item "aspirin" 10 1>>')
  assertStringIncludes(result.yarn, "    <<return>>")
  assert(result.warnings.length === 0)
})

Deno.test("requires output directory unless replacing in place", async () => {
  await assertRejects(
    () => migratePaths(["data/dialogue"], { quiet: true }),
    Error,
    "--output-dir is required unless --stdout or --replace is used",
  )
})

Deno.test("preserves source JSON when writing to an output directory", async () => {
  const tempRoot = await Deno.makeTempDir({ dir: Deno.cwd(), prefix: ".dialogue_yarn_migration_" })
  try {
    const nestedDir = join(tempRoot, "nested")
    const outputDir = join(tempRoot, "out")
    const sourcePath = join(nestedDir, "talk.json")
    const untouchedPath = join(tempRoot, "items.json")
    await Deno.mkdir(nestedDir)
    await Deno.writeTextFile(
      sourcePath,
      JSON.stringify([{ type: "talk_topic", id: "TALK_OUTPUT", dynamic_line: "Found." }]),
    )
    await Deno.writeTextFile(untouchedPath, JSON.stringify([{ type: "item", id: "rock" }]))

    const summary = await migratePaths([tempRoot], { outputDir, quiet: true })

    assertEquals(summary.convertedCount, 1)
    assertEquals(summary.deletedPaths, [])
    assertEquals(await exists(sourcePath), true)
    assertEquals(await exists(untouchedPath), true)
    assertEquals(summary.writtenPaths.length, 1)
    assertStringIncludes(summary.writtenPaths[0], "talk.yarn")
    assertStringIncludes(await Deno.readTextFile(summary.writtenPaths[0]), "title: TALK_OUTPUT")
  } finally {
    await Deno.remove(tempRoot, { recursive: true })
  }
})

Deno.test("replace mode keeps mixed JSON data while migrating talk topics", async () => {
  const tempRoot = await Deno.makeTempDir({ prefix: "dialogue_yarn_mixed_" })
  try {
    const sourcePath = join(tempRoot, "mixed.json")
    const outputPath = join(tempRoot, "mixed.yarn")
    await Deno.writeTextFile(
      sourcePath,
      JSON.stringify([
        { type: "npc", id: "tester", chat: "TALK_MIXED" },
        { type: "item_group", id: "NC_ROBOFAC_INTERCOM_trade", entries: [{ item: "rock" }] },
        { type: "talk_topic", id: "TALK_MIXED", dynamic_line: "Migrated." },
      ]),
    )

    const summary = await migratePaths([sourcePath], { replace: true, quiet: true })

    assertEquals(summary.convertedCount, 1)
    assertEquals(summary.deletedPaths, [])
    assertEquals(summary.rewrittenJsonPaths, [sourcePath])
    assertEquals(await exists(sourcePath), true)
    assertEquals(await exists(outputPath), true)
    const yarn = await Deno.readTextFile(outputPath)
    assertStringIncludes(yarn, "title: Start")
    assertStringIncludes(yarn, "<<jump TALK_MIXED>>")
    assertStringIncludes(yarn, "title: TALK_MIXED")
    const remaining = JSON.parse(await Deno.readTextFile(sourcePath)) as unknown[]
    assertEquals(remaining, [
      { type: "npc", id: "tester", chat: "TALK_MIXED", yarn_story: "mixed" },
      { type: "item_group", id: "NC_ROBOFAC_INTERCOM_trade", entries: [{ item: "rock" }] },
    ])
  } finally {
    await Deno.remove(tempRoot, { recursive: true })
  }
})

Deno.test("replace mode repairs already migrated NPC JSON when yarn exists", async () => {
  const tempRoot = await Deno.makeTempDir({ prefix: "dialogue_yarn_repair_" })
  try {
    const sourcePath = join(tempRoot, "already.json")
    const outputPath = join(tempRoot, "already.yarn")
    await Deno.writeTextFile(
      sourcePath,
      JSON.stringify([{ type: "npc", id: "tester", chat: "TALK_ALREADY" }]),
    )
    await Deno.writeTextFile(outputPath, "title: Start\n---\n<<jump TALK_ALREADY>>\n===\n")

    const summary = await migratePaths([sourcePath], { replace: true, quiet: true })

    assertEquals(summary.convertedCount, 0)
    assertEquals(summary.repairedJsonPaths, [sourcePath])
    const remaining = JSON.parse(await Deno.readTextFile(sourcePath)) as unknown[]
    assertEquals(remaining, [{
      type: "npc",
      id: "tester",
      chat: "TALK_ALREADY",
      yarn_story: "already",
    }])
  } finally {
    await Deno.remove(tempRoot, { recursive: true })
  }
})

Deno.test("replace mode writes in place and removes pure dialogue JSON", async () => {
  const tempRoot = await Deno.makeTempDir({ prefix: "dialogue_yarn_migration_" })
  try {
    const nestedDir = join(tempRoot, "nested")
    const sourcePath = join(nestedDir, "talk.json")
    const outputPath = join(nestedDir, "talk.yarn")
    const untouchedPath = join(tempRoot, "items.json")
    await Deno.mkdir(nestedDir)
    await Deno.writeTextFile(
      sourcePath,
      JSON.stringify([{ type: "talk_topic", id: "TALK_RECURSIVE", dynamic_line: "Found." }]),
    )
    await Deno.writeTextFile(untouchedPath, JSON.stringify([{ type: "item", id: "rock" }]))

    const summary = await migratePaths([tempRoot], { replace: true, quiet: true })

    assertEquals(summary.convertedCount, 1)
    assertEquals(summary.writtenPaths, [outputPath])
    assertEquals(summary.deletedPaths, [sourcePath])
    assertEquals(await exists(sourcePath), false)
    assertEquals(await exists(outputPath), true)
    assertEquals(await exists(untouchedPath), true)
    assertStringIncludes(await Deno.readTextFile(outputPath), "title: TALK_RECURSIVE")
  } finally {
    await Deno.remove(tempRoot, { recursive: true })
  }
})

Deno.test("replace mode can write to an output directory while removing source JSON", async () => {
  const tempRoot = await Deno.makeTempDir({ dir: Deno.cwd(), prefix: ".dialogue_yarn_replace_" })
  try {
    const nestedDir = join(tempRoot, "nested")
    const outputDir = join(tempRoot, "out")
    const sourcePath = join(nestedDir, "talk.json")
    await Deno.mkdir(nestedDir)
    await Deno.writeTextFile(
      sourcePath,
      JSON.stringify([{ type: "talk_topic", id: "TALK_REPLACE_OUTPUT", dynamic_line: "Moved." }]),
    )

    const summary = await migratePaths([sourcePath], { outputDir, replace: true, quiet: true })

    assertEquals(summary.convertedCount, 1)
    assertEquals(summary.deletedPaths, [sourcePath])
    assertEquals(await exists(sourcePath), false)
    assertEquals(summary.writtenPaths.length, 1)
    assertStringIncludes(summary.writtenPaths[0], "talk.yarn")
    assertStringIncludes(
      await Deno.readTextFile(summary.writtenPaths[0]),
      "title: TALK_REPLACE_OUTPUT",
    )
  } finally {
    await Deno.remove(tempRoot, { recursive: true })
  }
})
