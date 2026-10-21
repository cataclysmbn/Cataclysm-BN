/**
 * @module
 * The same input recording leaves the same complete save whichever client plays it back: twice
 * headless (`--client=mcp`) and once in the native Tiles client in a muted headless gamescope. Each
 * scenario is recorded once over stdio and ends with a save and quit, so every playback writes a
 * save; `tools/client/compare_saves.ts` compares every file and SQLite record of two saves.
 *
 * BN_BINARY=out/build/linux-full/src/cataclysm-bn-tiles deno test --allow-read --allow-write \
 *   --allow-run --allow-env tests/protocol/save_matrix_test.ts
 */
import { assert, assertEquals } from "@std/assert"
import { compare } from "../../tools/client/compare_saves.ts"
import { atMainMenu, itemRows, Mirror, stackPick } from "./mirror.ts"
import { Client, schemaHash } from "./stdio_client.ts"

// deno-lint-ignore no-explicit-any
type Value = any
class Session {
  mirror!: Mirror
  stages = new Map<string, string[]>()
  /** The error of the last command that ended in one. */
  refused = ""
  resyncs = 0
  /** The event types received. */
  told = new Set<string>()

  constructor(readonly client: Client) {}

  async start() {
    const header = await this.client.result("bn.subscribe", {})
    const parts: Value[] = []
    while (parts.length < header.parts) {
      const note = await this.client.note()
      assertEquals(note.method, "bn.snapshot.part")
      parts.push(note.params)
    }
    this.mirror = Mirror.fromSnapshot(header, parts)
  }

  handle(note: Value) {
    const { method, params } = note
    if (method === "bn.events") {
      this.mirror.applyEvents(params.epoch, params.events)
      for (const event of params.events) this.told.add(event.type)
    } else if (method === "bn.command") {
      if (params.error) this.refused = params.error
      const stages = this.stages.get(params.command_id) ?? ["received"]
      stages.push(params.stage)
      this.stages.set(params.command_id, stages)
    } else if (method === "bn.resync") {
      this.resyncs++
    } else if (!["bn.loading", "bn.progress"].includes(method)) {
      throw new Error(`unexpected notification ${method}`)
    }
  }

  /** Submit against what the mirror believes, follow pushes until the command is terminal and expect it to end as `ends`: loading or leaving a world ends its command `interrupted`. */
  async run(operation: Value, scope?: "screen" | "boundary", ends = "completed") {
    this.refused = ""
    const mirror = this.mirror
    const top = mirror.interaction.stack.at(-1)
    const offered = operation.kind === "invoke"
      ? top?.affordances.find((entry: Value) => entry.id === operation.affordance_id)
      : undefined
    const boundary = { revision: mirror.at.revision, boundary_id: mirror.interaction.boundary_id }
    const { command_id: id } = await this.client.result("bn.command.submit", {
      epoch: mirror.at.epoch,
      // An invoke names what its affordance's scope asks for; every other operation, the boundary.
      expect: operation.kind === "invoke"
        ? (scope ?? offered?.expect) === "boundary" ? boundary : { screen_id: top.screen_id }
        : { ...boundary, schema_id: mirror.interaction.interaction?.schema_id ?? null },
      operation,
    })
    this.stages.set(id, ["received"])
    while (!["completed", "rejected", "interrupted"].includes(this.stages.get(id)!.at(-1)!)) {
      this.handle(await this.client.note())
    }
    assertEquals(
      this.stages.get(id)!.at(-1),
      ends,
      `${JSON.stringify(operation)} ${this.refused}`,
    )
  }

  /** Choose the entry by label, or pick the tab or row of a screen of components by id or label. */
  async choose(label: string, ends = "completed") {
    const choice = this.mirror.interaction.interaction?.choices?.find((entry: Value) =>
      entry.enabled && entry.label === label
    )
    if (choice) return await this.run({ kind: "choose", choice_id: choice.id }, undefined, ends)
    const pick = stackPick(this.mirror, label)
    assert(pick, `no choice ${label}`)
    await this.run({ kind: "invoke", ...pick }, undefined, ends)
  }

  /** Acknowledge single-choice prompts until `done` holds. */
  async acknowledge(done: () => boolean) {
    for (let step = 0; !done() && step < 20; step++) {
      const choices = this.mirror.interaction.interaction.choices.filter((entry: Value) =>
        entry.enabled
      )
      assertEquals(choices.length, 1, "unexpected prompt")
      await this.run({ kind: "choose", choice_id: choices[0].id })
    }
    assert(done())
  }

  clear() {
    return this.acknowledge(() => this.mirror.interaction.interaction === null)
  }

  top(): Value {
    return this.mirror.interaction.stack.at(-1)
  }

  /** Follows pushes until the top screen is `context`. */
  async until(context: string) {
    for (let n = 0; n < 200 && this.top().context !== context; n++) {
      this.handle(await this.client.note())
    }
    assertEquals(this.top().context, context)
  }

  /** Follows pushes until the engine resyncs, then subscribes to the new state. */
  async afterResync(): Promise<Session> {
    for (let n = 0; n < 2000 && this.resyncs === 0; n++) this.handle(await this.client.note())
    assert(this.resyncs > 0, "a resync")
    const next = new Session(this.client)
    await next.start()
    return next
  }
}

const findComponent = (component: Value, id: string): Value | undefined =>
  component.id === id
    ? component
    : (component.children ?? (component.child ? [component.child] : []))
      .map((child: Value) => findComponent(child, id)).find(Boolean)

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

/** Spawns `names` through the debug menu and leaves it. */
async function spawnItems(session: Session, names: string[]) {
  await session.run({ kind: "action", action_id: "debug" })
  await session.choose("Spawning…")
  await session.choose("Spawn an item")
  for (const name of names) await wishItem(session, name)
  while (session.mirror.interaction.interaction) await session.run({ kind: "cancel" })
}

/** Chooses the inventory row whose label starts with `prefix`. */
async function chooseItem(session: Session, prefix: string) {
  const row = itemRows(session.top()).find((entry) => entry.label.startsWith(prefix))
  assert(row, `no item ${prefix}`)
  await session.run({ kind: "invoke", affordance_id: row.activate, args: { row: row.id } })
}

/** Sells a wished flashlight to the debug shopkeeper through its dialogue. */
async function trade(session: Session) {
  await spawnItems(session, ["flashlight (off)"])
  // The debug shopkeeper stands beside the avatar; its own trade is closed so the dialogue opens one.
  await session.run({ kind: "action", action_id: "debug" })
  await session.choose("Spawning…")
  await session.choose("Trade with a spawned shopkeeper")
  await session.until("NPC_TRADE")
  await session.run({ kind: "invoke", affordance_id: "trade:cancel" }, "screen")
  await session.run({ kind: "action", action_id: "chat" })
  const talks = itemRows(session.top()).find((row) => row.label.startsWith("Talk to"))
  assert(talks, "the chat menu offers a talk")
  await session.run(
    { kind: "invoke", affordance_id: talks.activate, args: { row: talks.id } },
    "boundary",
  )
  // The scenario's companion is nearby too, so the game asks whom to talk to.
  const whom = session.mirror.interaction.interaction?.choices
    ?.find((entry: Value) => entry.label.endsWith("Merchant"))
  if (whom) await session.run({ kind: "choose", choice_id: whom.id })
  await session.until("DIALOGUE_CHOOSE_RESPONSE")
  const respond = async (text: string) => {
    const row = findComponent(session.top().root, "talk:responses").rows
      .find((entry: Value) => entry.label.includes(text))
    assert(row, `a response ${text}`)
    await session.run(
      { kind: "invoke", affordance_id: "talk:respond", args: { row: row.id } },
      "boundary",
    )
  }
  await respond("Let's trade")
  await session.until("NPC_TRADE")
  const offered = findComponent(session.top().root, "list:you").rows
    .find((row: Value) => row.label.includes("flashlight"))
  assert(offered, "the avatar offers a flashlight")
  await session.run(
    { kind: "invoke", affordance_id: "trade:set_count", args: { row: offered.id, count: 1 } },
    "boundary",
  )
  await session.run({ kind: "invoke", affordance_id: "trade:confirm" }, "screen")
  await session.choose("Yes")
  await session.until("DIALOGUE_CHOOSE_RESPONSE")
  await respond("Bye")
  await session.clear()
}

/** Throws an activated grenade at the farthest square in sight the target UI calls good, and waits out the fuse. */
async function explosion(session: Session) {
  await spawnItems(session, ["grenade"])
  await session.run({ kind: "action", action_id: "apply" })
  await chooseItem(session, "grenade")
  await session.clear()
  await session.run({ kind: "action", action_id: "throw" })
  await chooseItem(session, "active grenade")
  const avatar = session.mirror.avatar.at
  const reach = (pos: Value) => Math.max(Math.abs(pos.x - avatar.x), Math.abs(pos.y - avatar.y))
  const candidates = [...session.mirror.cells.values()]
    .filter((cell) => cell.known === "visible" && cell.at.z === avatar.z && reach(cell.at) <= 12)
    .map((cell) => cell.at)
    .sort((a, b) => reach(b) - reach(a) || a.x - b.x || a.y - b.y)
  let aimed = false
  for (const pos of candidates) {
    await session.run({ kind: "invoke", affordance_id: "target:move", args: { pos } })
    if (session.top().root.child.target.status === "good") {
      aimed = true
      break
    }
  }
  assert(aimed, "a square in sight to throw at")
  await session.run({ kind: "action", action_id: "FIRE" })
  // The fuse burns five turns.
  for (let turn = 0; turn < 7; turn++) {
    await session.clear()
    await session.run({ kind: "action", action_id: "pause" })
  }
  await session.clear()
  assert(session.told.has("explosion.ended"), "the grenade went off")
}

async function makeProfile(root: string, name: string): Promise<string> {
  const profile = `${root}/${name}`
  await Deno.mkdir(`${profile}/config`, { recursive: true })
  await Deno.writeTextFile(
    `${profile}/config/options.json`,
    JSON.stringify(
      Object.entries({ USE_LANG: "en_US", AUTOSAVE: "false", COMPUTE_ACCELERATION: "gpu_software" })
        .map(([name, value]) => ({ name, value })),
    ),
  )
  await Deno.writeTextFile(
    `${profile}/config/preload.json`,
    JSON.stringify({ compute_acceleration: "gpu_software" }),
  )
  return profile
}

const binary = Deno.env.get("BN_BINARY")

const profileArgs = (profile: string) => [
  "--userdir",
  `${profile}/`,
  "--configdir",
  `${profile}/config/`,
]

/** Plays `play` in a Play Now world over stdio, recorded, then saves and quits; returns the recording. */
async function record(root: string, play: (session: Session) => Promise<void>): Promise<string> {
  const profile = await makeProfile(root, "record")
  const replay = `${root}/input.jsonl`
  const client = new Client(binary!, [
    "--seed",
    "save-matrix-1",
    ...profileArgs(profile),
    "--replay-record",
    replay,
  ])
  try {
    await client.result("bn.hello", {
      versions: ["0.2"],
      schema: schemaHash,
      client: { name: "save-matrix-test", version: "1" },
    })
    let session = new Session(client)
    await session.start()
    await session.acknowledge(() => atMainMenu(session.mirror))
    await session.choose("new_game")
    await session.choose("new_game:play_now_default", "interrupted")
    session = await session.afterResync()
    await session.clear()
    await play(session)
    assertEquals(session.top().role, "map")
    await session.run({ kind: "action", action_id: "save" })
    await session.choose("Yes", "interrupted")
    session = await session.afterResync()
    await session.acknowledge(() => atMainMenu(session.mirror))
    await client.close()
    return replay
  } catch (error) {
    client.kill()
    throw error
  }
}

/** Plays `replay` back in `client` without a protocol peer, in a new profile `name`; returns its save directory. */
async function playBack(
  { root, name, replay, client }: {
    root: string
    name: string
    replay: string
    client: "mcp" | "tiles"
  },
): Promise<string> {
  const profile = await makeProfile(root, name)
  const args = [`--client=${client}`, ...profileArgs(profile), "--replay-play", replay]
  if (client === "mcp") {
    const status = await run(binary!, args)
    assert(status.success, `${name} playback exit ${status.code}:\n${status.said}`)
    return `${profile}/save`
  }
  // The native client draws to a real SDL window in a headless gamescope and plays its sound into a
  // null sink of its own; gamescope exits 0 whatever the game did, so the game's status is written aside.
  const sink = `bn_save_matrix_${name}_${Date.now()}`
  const loaded = await run("pactl", ["load-module", "module-null-sink", `sink_name=${sink}`])
  assert(loaded.success, `no null sink: ${loaded.said}`)
  try {
    const exit = `${profile}/exit.json`
    const host = await run("gamescope", [
      ...["--backend", "headless", "-W", "1920", "-H", "1080", "-w", "1920", "-h", "1080", "--"],
      ...[Deno.execPath(), "run", "--allow-run", "--allow-write", exitStatus, exit, binary!],
      ...args,
    ], { PULSE_SINK: sink })
    const status: Deno.CommandStatus | undefined = await Deno.readTextFile(exit)
      .then(JSON.parse, () => undefined)
    assert(status?.success, `${name} playback ${JSON.stringify(status)}:\n${host.said}`)
  } finally {
    await run("pactl", ["unload-module", loaded.out.trim()])
  }
  return `${profile}/save`
}

const exitStatus = new URL("./exit_status.ts", import.meta.url).href

/** Runs `command` without input; its status, standard output, and the last lines of its standard error. */
async function run(command: string, args: string[], env: Record<string, string> = {}) {
  const output = await new Deno.Command(command, { args, env, stdin: "null" }).output()
  const text = (bytes: Uint8Array) => new TextDecoder().decode(bytes)
  return {
    success: output.success,
    code: output.code,
    out: text(output.stdout),
    said: text(output.stderr).split("\n").slice(-20).join("\n"),
  }
}

/** The records two saves differ in, as compare_saves names them; empty when equal. */
async function saveDifferences(root: string, left: string, right: string, label: string) {
  const summary = await compare(left, right, `${root}/compare-${label}`)
  return summary.changes.map((change: Value) =>
    change.membership
      ? `${change.record}: ${change.membership}`
      : `${change.record}: ${change.count} differ, as ${
        change.samples.slice(0, 4).map((sample: Value) =>
          `${sample.path} ${sample.left?.slice(0, 60)} -> ${sample.right?.slice(0, 60)}`
        ).join("; ")
      }`
  )
}

for (
  const [name, play] of [
    ["an ordinary trade", trade],
    ["a native explosion", explosion],
  ] as const
) {
  Deno.test({
    name:
      `save matrix: ${name} leaves the same complete save played back headless twice and in Tiles`,
    ignore: !binary,
    async fn() {
      const root = await Deno.makeTempDir({ prefix: "bn-save-matrix-" })
      try {
        const replay = await record(root, play)
        const mcp = await playBack({ root, name: "mcp-1", replay, client: "mcp" })
        const again = await playBack({ root, name: "mcp-2", replay, client: "mcp" })
        const tiles = await playBack({ root, name: "tiles", replay, client: "tiles" })
        const found = {
          "mcp against mcp": await saveDifferences(root, mcp, again, "mcp-mcp"),
          "mcp against tiles": await saveDifferences(root, mcp, tiles, "mcp-tiles"),
        }
        assertEquals(found, { "mcp against mcp": [], "mcp against tiles": [] })
      } finally {
        await Deno.remove(root, { recursive: true })
      }
    },
  })
}
