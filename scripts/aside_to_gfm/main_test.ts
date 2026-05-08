import { assertEquals } from "@std/assert"

import { asideToGfm } from "./main.ts"

import given from "./given.md" with { type: "text" }
import expected from "./expected.md" with { type: "text" }

Deno.test("asideToGfm() handles cautions", () => assertEquals(asideToGfm(given), expected))
