/**
 * @module
 * A client never changes the game. The same inputs under different client conditions (a client
 * reading everything, a slow reader with credits, a paced `max_hz`, no client at all) leave the same
 * per-turn state hashes. The engine writes one hash per part of the state after every turn
 * (`--state-hash-log`); the first turn and part where two logs differ is the report, as in
 * Factorio's desync report.
 *
 * BN_BINARY=out/build/linux-full/cataclysm-bn-tiles deno test --allow-read --allow-write \
 *   --allow-run --allow-env tests/protocol/determinism_test.ts
 */
import { assert, assertEquals } from "@std/assert"
import { atMainMenu, itemRows } from "./mirror.ts"
import { Session } from "./session.ts"
import { Client, schemaHash } from "./stdio_client.ts"

// deno-lint-ignore no-explicit-any
type Value = any
const schemaFile = new URL("../../docs/schema/engine-client/0.2.schema.json", import.meta.url)

/** One record of a state log: the hashes after one `game::do_turn`. */
type StateRecord = { step: number; turn: number; parts: Record<string, string> }

/** The state records of a log, in order; the header is checked and dropped. */
const parseStateLog = (text: string): StateRecord[] => {
  const [header, ...records] = text.split("\n").filter((line) => line).map((line) =>
    JSON.parse(line)
  )
  assertEquals(header, { kind: "header", format: "cataclysm-bn-state-hash", version: 1 })
  return records.map((record, index) => {
    assertEquals(record.kind, "state")
    assertEquals(record.step, index + 1, "steps are consecutive")
    return { step: record.step, turn: record.turn, parts: record.parts }
  })
}

/** Where two logs first differ: the step, the turn and the parts, or the length; undefined when equal. */
const firstDivergence = (a: StateRecord[], b: StateRecord[]): string | undefined => {
  for (let i = 0; i < Math.min(a.length, b.length); i++) {
    const parts = [...new Set([...Object.keys(a[i].parts), ...Object.keys(b[i].parts)])].filter(
      (part) => a[i].parts[part] !== b[i].parts[part],
    )
    if (a[i].turn !== b[i].turn) parts.unshift("turn")
    if (parts.length) {
      return `step ${a[i].step}, turn ${a[i].turn} against ${b[i].turn}: ${parts.join(", ")} differ`
    }
  }
  if (a.length !== b.length) {
    return `one log stops after step ${Math.min(a.length, b.length)}, the other has ${
      Math.max(a.length, b.length)
    }`
  }
}

const record = (step: number, turn: number, parts: Record<string, string>): StateRecord => ({
  step,
  turn,
  parts,
})

Deno.test("determinism: the first diverging turn and part are named", () => {
  const same = [record(1, 10, { rng: "a", avatar: "b" }), record(2, 11, { rng: "c", avatar: "d" })]
  assertEquals(firstDivergence(same, structuredClone(same)), undefined)
  const focus = structuredClone(same)
  focus[1].parts.avatar = "e"
  assertEquals(firstDivergence(same, focus), "step 2, turn 11 against 11: avatar differ")
  const both = structuredClone(focus)
  both[0].parts.rng = "z"
  both[0].turn = 12
  assertEquals(
    firstDivergence(same, both),
    "step 1, turn 10 against 12: turn, rng differ",
  )
  assertEquals(
    firstDivergence(same, same.slice(0, 1)),
    "one log stops after step 1, the other has 2",
  )
})

Deno.test("determinism: a log of the engine parses and rejects a foreign header", () => {
  const text = [
    '{"kind":"header","format":"cataclysm-bn-state-hash","version":1}',
    '{"kind":"state","step":1,"turn":5,"parts":{"rng":"0123456789abcdef"}}',
    "",
  ].join("\n")
  assertEquals(parseStateLog(text), [record(1, 5, { rng: "0123456789abcdef" })])
  let thrown = false
  try {
    parseStateLog('{"kind":"header","format":"other","version":1}\n')
  } catch {
    thrown = true
  }
  assert(thrown, "a foreign header is refused")
})

async function makeProfile(): Promise<string> {
  const profile = await Deno.makeTempDir({ prefix: "bn-determinism-" })
  await Deno.mkdir(`${profile}/config`, { recursive: true })
  await Deno.writeTextFile(
    `${profile}/config/options.json`,
    JSON.stringify(
      Object.entries({
        USE_LANG: "en_US",
        ANIMATIONS: "false",
        AUTOSAVE: "false",
        COMPUTE_ACCELERATION: "gpu_software",
      }).map(([name, value]) => ({ name, value })),
    ),
  )
  await Deno.writeTextFile(
    `${profile}/config/preload.json`,
    JSON.stringify({ compute_acceleration: "gpu_software" }),
  )
  return profile
}

const seed = "protocol-determinism-1"
const profileArgs = (profile: string) => [
  "--userdir",
  `${profile}/`,
  "--configdir",
  `${profile}/config/`,
]

/** Spawns one `name` through the debug menu's item wish list, which pages 200 entries at a time. */
async function wishItem(session: Session, name: string) {
  let found: Value
  for (let offset = 0; !found; offset += 200) {
    const page = await session.client.result("bn.interaction.choices", {
      epoch: session.mirror.at.epoch,
      boundary_id: session.mirror.interaction.boundary_id,
      offset,
      limit: 200,
    })
    assert(page.choices.length > 0, `${name} is listed`)
    found = page.choices.find((entry: Value) => entry.label === name)
  }
  await session.run({ kind: "choose", choice_id: found.id })
  await session.run({ kind: "fill", field_id: "field:value", value: "1", submit: true })
}

/** Every kind of boundary operation, once or more, in a session whose start is the tutorial. */
async function playEveryOperation(session: Session) {
  const run = (operation: Value) => session.run(operation)
  await session.clear()
  await run({ kind: "action", action_id: "wait" })
  await session.choose("5 minutes")
  await session.clear()

  // Waypoint travel: the first click plans the route, the second walks it.
  const here = session.mirror.avatar.at
  const reach = (pos: Value) => Math.max(Math.abs(pos.x - here.x), Math.abs(pos.y - here.y))
  const open = (entry: Value) =>
    entry.known === "visible" && !entry.furniture && !entry.vehicle &&
    /floor|dirt|grass|pavement|sidewalk/.test(entry.terrain?.id ?? "")
  const candidates = [...session.mirror.cells.values()]
    .filter((entry) => open(entry) && reach(entry.at) >= 2 && reach(entry.at) <= 8)
    .map((entry) => entry.at)
    .sort((a, b) => reach(b) - reach(a))
  let aim: Value
  for (const candidate of candidates.slice(0, 40)) {
    if (
      await session.submit({ kind: "travel", pos: candidate }) === "completed" &&
      session.mirror.route.length >= 2
    ) {
      aim = candidate
      break
    }
  }
  assert(aim, "the engine planned a route")
  await run({ kind: "travel", pos: aim })

  // The right click on the avatar's square and on a far one.
  const standing = session.mirror.avatar.at
  await run({ kind: "context", pos: { ...standing, x: standing.x + 4 } })
  await run({ kind: "context", pos: standing })
  await session.clear()
  // An action the square lists: the look view, always offered, left by its own cancel.
  await run({ kind: "tile_action", action_id: "look", pos: standing })
  await run({ kind: "invoke", affordance_id: "QUIT" })
  await session.clear()

  // A grenade through the debug menu: choose, fill and cancel; then drop it and pick it up.
  await run({ kind: "action", action_id: "debug" })
  await session.choose("Spawning…")
  await session.choose("Spawn an item")
  await wishItem(session, "grenade")
  while (session.mirror.interaction.interaction) await run({ kind: "cancel" })
  await session.clear()
  // Both lists are absent when empty; the wish may put the grenade on the ground or in the pack.
  const grenade = (list: "inventory" | "ground") =>
    (session.mirror.avatar[list] ?? []).find((row: Value) => /grenade/.test(row.name))
  if (!grenade("inventory")) {
    const lying = grenade("ground")
    assert(lying, `the grenade is nowhere: ${JSON.stringify(Object.keys(session.mirror.avatar))}`)
    await run({ kind: "pickup", item_id: lying.id, count: 1 })
    await session.clear()
  }
  const carried = grenade("inventory")
  assert(carried, "the grenade is carried")
  await run({ kind: "drop", item_id: carried.id, count: 1 })
  await session.clear()
  const lying = grenade("ground")
  assert(lying, "the grenade lies at the avatar's feet")
  await run({ kind: "pickup", item_id: lying.id, count: 1 })
  await session.clear()

  // The drop selector counts a quantity; the throw picker leads to the target UI.
  await run({ kind: "action", action_id: "drop" })
  const selector = () =>
    itemRows(session.mirror.interaction.stack.at(-1)).find((row) => /grenade/.test(row.label))!
  await run({
    kind: "invoke",
    affordance_id: selector().activate.replace(/:choose$/, ":count"),
    args: { row: selector().id, count: 1 },
  })
  await run({ kind: "invoke", affordance_id: "QUIT" })
  await run({ kind: "action", action_id: "throw" })
  await run({
    kind: "invoke",
    affordance_id: selector().activate,
    args: { row: selector().id },
  })
  const source = session.mirror.avatar.at
  await run({
    kind: "invoke",
    affordance_id: "target:move",
    args: { pos: { ...source, x: source.x + 1 } },
  })
  // The 0.1 target move belongs to screens with a structured interaction; a screen of components moves its cursor by `target:move`.
  assertEquals(
    await session.submit({ kind: "set_target", pos: { ...source, x: source.x + 1 } }),
    "rejected",
  )
  // The targeting screen is one of components: it is left by its own cancel affordance, as the 0.1 cancel belongs to screens with a structured interaction.
  await run({ kind: "invoke", affordance_id: "QUIT" })
  await session.clear()
  // The 0.1 count belongs to the screens that have not moved to components; the map has none to give it to.
  assertEquals(
    await session.submit({ kind: "set_count", choice_id: "entry:0", count: 1 }),
    "rejected",
  )

  // A long wait is an activity: the client conditions act on its frames.
  await run({ kind: "action", action_id: "wait" })
  await session.choose("1 hour")
  await session.clear()
}

/** A wait alone, to look for what differs between two runs of the same inputs. */
async function playWait(session: Session) {
  await session.clear()
  await session.run({ kind: "action", action_id: "wait" })
  await session.choose("5 minutes")
  await session.clear()
  await session.run({ kind: "action", action_id: "wait" })
  await session.choose("1 hour")
  await session.clear()
}

type Condition = {
  name: string
  delivery?: { credits: number; max_hz?: number }
  readDelayMs?: number
}

type Run = { log: StateRecord[]; profile: string; kinds: Set<string> }

/** One session of `play` with a client under `condition`; recorded when `recorded` is set. */
async function drive(
  binary: string,
  condition: Condition,
  play: (session: Session) => Promise<void>,
  recorded: boolean,
): Promise<Run> {
  const profile = await makeProfile()
  const log = `${profile}/state.jsonl`
  const replay = `${profile}/input.jsonl`
  const client = new Client(binary, [
    "--seed",
    seed,
    ...profileArgs(profile),
    "--state-hash-log",
    log,
    ...(recorded ? ["--replay-record", replay] : []),
  ])
  client.readDelayMs = condition.readDelayMs ?? 0
  try {
    await client.result("bn.hello", {
      versions: ["0.2"],
      schema: schemaHash,
      client: { name: "determinism-test", version: "1" },
      ...(condition.delivery ? { delivery: condition.delivery } : {}),
    })
    let session = new Session(client)
    session.window = condition.delivery?.credits ?? 0
    await session.start()
    await session.acknowledge(() => atMainMenu(session.mirror))
    await session.choose("New Game")
    await session.pick("Tutorial")
    assertEquals((await client.note()).method, "bn.resync")
    const window = session.window
    session = new Session(client)
    session.window = window
    await session.start()
    await play(session)
    const kinds = session.kinds
    await client.close()
    return { log: parseStateLog(await Deno.readTextFile(log)), profile, kinds }
  } catch (error) {
    client.kill()
    throw error
  }
}

/** The recording played with no client: no stdin, no protocol. */
async function playBack(binary: string, replay: string): Promise<StateRecord[]> {
  const profile = await makeProfile()
  const log = `${profile}/state.jsonl`
  const status = await new Deno.Command(binary, {
    args: [
      "--client=mcp",
      ...profileArgs(profile),
      "--replay-play",
      replay,
      "--state-hash-log",
      log,
    ],
    stdin: "null",
    stdout: "null",
    stderr: "piped",
  }).output()
  const said = new TextDecoder().decode(status.stderr).split("\n").slice(-20).join("\n")
  assert(status.success, `playback exit ${status.code}:\n${said}`)
  const records = parseStateLog(await Deno.readTextFile(log))
  await Deno.remove(profile, { recursive: true })
  return records
}

const binary = Deno.env.get("BN_BINARY")

Deno.test({
  name:
    "determinism: every operation leaves the same per-turn hashes under every client condition and in headless playback",
  ignore: !binary,
  async fn() {
    const conditions: Condition[] = [
      { name: "a client reading everything" },
      { name: "a slow reader with credits", delivery: { credits: 4 }, readDelayMs: 25 },
      { name: "max_hz set", delivery: { credits: 64, max_hz: 5 } },
    ]
    const runs: Run[] = []
    for (const condition of conditions) {
      runs.push(await drive(binary!, condition, playEveryOperation, true))
    }
    const [baseline] = runs
    assert(baseline.log.length > 10, `turns hashed: ${baseline.log.length}`)

    // Every kind the schema lets a client submit as a boundary operation was submitted.
    const schema = JSON.parse(await Deno.readTextFile(schemaFile))
    const offered = schema["$defs"].operation.oneOf.flatMap((operation: Value) =>
      operation.properties ? operation.properties.kind.const ?? operation.properties.kind.enum : []
    ).sort()
    // An invoke is not a boundary operation: each affordance names the scope it expects.
    assertEquals([...baseline.kinds].filter((kind) => kind !== "invoke").sort(), offered)

    for (const [i, condition] of conditions.entries()) {
      assertEquals(
        firstDivergence(baseline.log, runs[i].log),
        undefined,
        `${condition.name} against ${conditions[0].name}`,
      )
    }
    for (const [i, condition] of conditions.entries()) {
      assertEquals(
        firstDivergence(baseline.log, await playBack(binary!, `${runs[i].profile}/input.jsonl`)),
        undefined,
        `headless playback of ${condition.name} against ${conditions[0].name}`,
      )
    }
    for (const run of runs) await Deno.remove(run.profile, { recursive: true })
  },
})

Deno.test({
  name: "determinism: the same seed and waits leave the same per-turn hashes without a recording",
  ignore: !binary,
  async fn() {
    for (const pair of [1, 2, 3]) {
      const a = await drive(binary!, { name: "first" }, playWait, false)
      const b = await drive(binary!, { name: "second" }, playWait, false)
      const found = firstDivergence(a.log, b.log)
      for (const run of [a, b]) await Deno.remove(run.profile, { recursive: true })
      assertEquals(found, undefined, `pair ${pair} of unrecorded runs`)
    }
  },
})
