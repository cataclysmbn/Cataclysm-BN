/** @module The scenarios whose recorded wire becomes the contract examples. */
import type { Scenario } from "./driver.ts"
import { handshake } from "./handshake.ts"
import { tutorial } from "./tutorial.ts"

export const scenarios: Scenario[] = [handshake, tutorial]
