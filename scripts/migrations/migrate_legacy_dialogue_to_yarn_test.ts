import { assert, assertEquals, assertStringIncludes } from "@std/assert"

import { convertEntriesToYarn } from "./migrate_legacy_dialogue_to_yarn.ts"

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
  assertStringIncludes(result.yarn, "-> Ask a question.\n    <<detour TALK_DETAIL>>")
  assertStringIncludes(result.yarn, "-> Bye.\n    <<stop>>")
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
