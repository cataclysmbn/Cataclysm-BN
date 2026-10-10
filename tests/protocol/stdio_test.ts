/**
 * @module
 * Drives a real game process over stdio: hello, subscribe, a semantic choose and a movement
 * action, with the pushed stages and events reconstructing the same state as a fresh subscribe.
 *
 * BN_BINARY=out/build/linux-full/cataclysm-bn-tiles deno test --allow-read --allow-write \
 *   --allow-run --allow-env tests/protocol/stdio_test.ts
 */
import { assert, assertEquals, assertNotEquals } from "@std/assert"
import { Ajv2020 } from "npm:ajv@8.17.1/dist/2020.js"
import { Mirror } from "./mirror.ts"

// Every message the engine really sends is checked against the published schema.
const schema = JSON.parse(
  await Deno.readTextFile(
    new URL("../../docs/schema/engine-client/1.0.schema.json", import.meta.url),
  ),
)
const ajv = new Ajv2020({ strict: false })
ajv.addSchema(schema, "bn")
const validate = (def: string, value: unknown, what: string) => {
  const check = ajv.getSchema(`bn#/$defs/${def}`)
  assert(check, def)
  assert(
    check(value),
    `${what} violates ${def}: ${JSON.stringify(check.errors)}\n${JSON.stringify(value)}`,
  )
}

// deno-lint-ignore no-explicit-any
type Value = any
/** The engine is killed after this long without output and without CPU use; either one restarts the wait. */
const stall = 120_000

class Client {
  #process: Deno.ChildProcess
  #writer: WritableStreamDefaultWriter<Uint8Array>
  #reader: ReadableStreamDefaultReader<string>
  #buffer = ""
  #id = 0
  /** Notifications that arrived while waiting for something else, in order. */
  notes: Value[] = []

  constructor(binary: string, profile: string) {
    this.#process = new Deno.Command(binary, {
      args: [
        "--client=mcp",
        "--seed",
        "protocol-stdio-1",
        "--userdir",
        `${profile}/`,
        "--configdir",
        `${profile}/config/`,
      ],
      stdin: "piped",
      stdout: "piped",
      stderr: "null",
    }).spawn()
    this.#writer = this.#process.stdin.getWriter()
    this.#reader = this.#process.stdout.pipeThrough(new TextDecoderStream()).getReader()
  }

  /** `utime + stime` of the engine in clock ticks; a loading engine is silent on stdout but busy. */
  async #cpuTicks(): Promise<number> {
    try {
      const stat = await Deno.readTextFile(`/proc/${this.#process.pid}/stat`)
      const fields = stat.slice(stat.lastIndexOf(")") + 1).trim().split(/\s+/)
      return Number(fields[11]) + Number(fields[12])
    } catch {
      return 0
    }
  }

  async #line(): Promise<Value> {
    let last = performance.now()
    let ticks = await this.#cpuTicks()
    // Kill the engine only after `stall` without output and without CPU use.
    const watch = setInterval(async () => {
      const now = await this.#cpuTicks()
      if (now !== ticks) [ticks, last] = [now, performance.now()]
      if (performance.now() - last > stall) this.#process.kill("SIGKILL")
    }, 1000)
    try {
      while (!this.#buffer.includes("\n")) {
        const part = await this.#reader.read()
        assert(!part.done, "engine closed stdout")
        this.#buffer += part.value
        last = performance.now()
      }
    } finally {
      clearInterval(watch)
    }
    const end = this.#buffer.indexOf("\n")
    const line = this.#buffer.slice(0, end)
    this.#buffer = this.#buffer.slice(end + 1)
    return { ...JSON.parse(line), receivedAt: performance.now() }
  }

  async request(method: string, params: unknown): Promise<Value> {
    const id = ++this.#id
    await this.#writer.write(
      new TextEncoder().encode(JSON.stringify({ jsonrpc: "2.0", id, method, params }) + "\n"),
    )
    while (true) {
      const message = await this.#line()
      if (message.id === id) {
        if (message.error) validate("application_error", message.error.data, `${method} error`)
        else validate(schema["x-methods"][method].result, message.result, `${method} result`)
        return message
      }
      assert(message.id === undefined, `unexpected response ${JSON.stringify(message)}`)
      validate(schema["x-notifications"][message.method], message.params, message.method)
      this.notes.push(message)
    }
  }

  async result(method: string, params: unknown): Promise<Value> {
    const response = await this.request(method, params)
    assert(response.error === undefined, JSON.stringify(response.error))
    return response.result
  }

  /** Next notification, from the backlog first. */
  async note(): Promise<Value> {
    const queued = this.notes.shift()
    if (queued) return queued
    const message = await this.#line()
    validate(schema["x-notifications"][message.method], message.params, message.method)
    return message
  }

  async close() {
    await this.#writer.close()
    const status = await this.#process.status
    assert(status.success, `engine exit ${JSON.stringify(status)}`)
  }

  kill() {
    try {
      this.#process.kill("SIGKILL")
    } catch { /* already gone */ }
  }
}

class Session {
  mirror!: Mirror
  /** Stages seen per command id. */
  stages = new Map<string, string[]>()
  eventTypes: string[] = []
  resyncs: Value[] = []
  /** `bn.loading` notifications with their arrival time, in arrival order. */
  loading: Value[] = []
  /** `bn.progress` heartbeats seen. */
  progress = 0

  constructor(readonly client: Client) {}

  async subscribe(): Promise<Mirror> {
    const header = await this.client.result("bn.subscribe", {})
    const parts: Value[] = []
    while (parts.length < header.parts) {
      const note = await this.client.note()
      assertEquals(note.method, "bn.snapshot.part")
      parts.push(note.params)
    }
    return Mirror.fromSnapshot(header, parts)
  }

  async start() {
    this.mirror = await this.subscribe()
  }

  #handle(note: Value) {
    const { method, params } = note
    if (method === "bn.events") {
      this.mirror.applyEvents(params.epoch, params.events)
      for (const event of params.events) this.eventTypes.push(event.type)
    } else if (method === "bn.command") {
      const stages = this.stages.get(params.command_id) ?? ["received"]
      stages.push(params.stage)
      this.stages.set(params.command_id, stages)
    } else if (method === "bn.loading") {
      this.loading.push({ ...params, receivedAt: note.receivedAt })
    } else if (method === "bn.resync") {
      this.resyncs.push(params)
    } else if (method === "bn.progress") {
      this.progress++
    } else {
      throw new Error(`unexpected notification ${method}`)
    }
  }

  /** Submit against what the mirror believes, then follow pushes until the command is terminal. */
  async submit(operation: Value, patch: Value = {}, during?: () => Promise<void>) {
    const mirror = this.mirror
    const response = await this.client.request("bn.command.submit", {
      epoch: mirror.at.epoch,
      expect: {
        revision: mirror.at.revision,
        boundary_id: mirror.interaction.boundary_id,
        schema_id: mirror.interaction.interaction?.schema_id ?? null,
        ...patch,
      },
      operation,
    })
    if (response.error) return { error: response.error, stages: [] as string[] }
    const id = response.result.command_id
    this.stages.set(id, ["received"])
    await during?.()
    while (true) {
      const stages = this.stages.get(id)!
      const last = stages.at(-1)
      if (["completed", "rejected", "interrupted"].includes(last!)) {
        return { id, stages, error: undefined }
      }
      this.#handle(await this.client.note())
    }
  }

  /** Resize the terrain window and follow the pushes until the engine reports a different view. */
  async viewport(cols: number, rows: number) {
    const before = JSON.stringify(this.mirror.view)
    await this.client.result("bn.viewport", { cols, rows })
    while (JSON.stringify(this.mirror.view) === before) this.#handle(await this.client.note())
  }

  async choose(label: string) {
    const choice = this.mirror.interaction.interaction.choices.find((entry: Value) =>
      entry.enabled && entry.label === label
    )
    assert(choice, `no choice ${label}`)
    return await this.submit({ kind: "choose", choice_id: choice.id })
  }

  /** Acknowledge single-choice prompts until `done` holds. */
  async acknowledge(done: () => boolean) {
    for (let step = 0; !done() && step < 20; step++) {
      const choices = this.mirror.interaction.interaction.choices.filter((entry: Value) =>
        entry.enabled
      )
      assertEquals(choices.length, 1, "unexpected prompt")
      assertEquals(
        (await this.submit({ kind: "choose", choice_id: choices[0].id })).stages.at(-1),
        "completed",
      )
    }
    assert(done())
  }
}

/** Loading progress is ordered, ends with `done`, and names images the client can find. */
const checkLoading = async (loading: Value[], chosenAt: number) => {
  assert(loading.length > 1, "loading notifications before the world snapshot")
  const done = loading.at(-1)
  assertEquals(done.done, true)
  const progress = loading.slice(0, -1)
  assert(progress.every((note) => note.done === undefined), "done only once, last")
  const contexts: { title: string; steps: number; entries: number; start: number }[] = []
  let previous: Value
  for (const note of progress) {
    assert(note.index >= 0 && note.index < note.entries.length, "index selects an entry")
    // A context restarts its list, so the same title can come again with the index back at 0.
    if (previous?.title !== note.title || note.index < previous.index) {
      contexts.push({ title: note.title, steps: 0, entries: 0, start: note.receivedAt })
    }
    const context = contexts.at(-1)!
    context.steps++
    context.entries = note.entries.length
    if (note.image) {
      assert(!note.image.path.startsWith("/"), "image path is relative to the base path")
      await Deno.stat(new URL(`../../${note.image.path}`, import.meta.url))
    }
    previous = note
  }
  assert(progress.some((note) => note.image), "a loading image is chosen")
  // Time spent in each loading context: from its first notification to the next context's.
  console.log(`loading total ${((done.receivedAt - chosenAt) / 1000).toFixed(1)} s`)
  const slowest = progress.slice(0, -1).map((note, i) => ({
    step: `${note.title}: ${note.entries[note.index]}`,
    seconds: (loading[i + 1].receivedAt - note.receivedAt) / 1000,
  })).sort((a, b) => b.seconds - a.seconds).slice(0, 5)
  for (const { step, seconds } of slowest) {
    console.log(`loading slowest ${step}: ${seconds.toFixed(2)} s`)
  }
  for (const [i, context] of contexts.entries()) {
    const end = contexts[i + 1]?.start ?? done.receivedAt
    console.log(
      `loading ${context.title}: ${context.steps} steps, ${context.entries} entries, ` +
        `${((end - context.start) / 1000).toFixed(2)} s`,
    )
  }
}

async function makeProfile(): Promise<string> {
  const profile = await Deno.makeTempDir({ prefix: "bn-protocol-" })
  await Deno.mkdir(`${profile}/config`, { recursive: true })
  await Deno.writeTextFile(
    `${profile}/config/options.json`,
    JSON.stringify([
      { name: "USE_LANG", value: "en_US" },
      { name: "ANIMATIONS", value: "false" },
      { name: "AUTOSAVE", value: "false" },
      { name: "COMPUTE_ACCELERATION", value: "gpu_software" },
    ]),
  )
  await Deno.writeTextFile(
    `${profile}/config/preload.json`,
    JSON.stringify({ compute_acceleration: "gpu_software" }),
  )
  return profile
}

/** Hello, then New Game and Tutorial; returns the session on the loaded tutorial world. */
async function enterTutorial(client: Client): Promise<Session> {
  await client.result("bn.hello", {
    versions: ["1.0"],
    client: { name: "stdio-test", version: "1" },
  })
  let session = new Session(client)
  await session.start()
  await session.acknowledge(() => session.mirror.interaction.interaction?.context === "MAIN_MENU")
  await session.choose("New Game")
  await session.choose("Tutorial")
  assertEquals((await client.note()).method, "bn.resync")
  session = new Session(client)
  await session.start()
  await session.acknowledge(() => session.mirror.interaction.interaction === null)
  return session
}

Deno.test({
  name: "stdio: hello, subscribe, choose and move; stream equals a fresh subscribe",
  ignore: !Deno.env.get("BN_BINARY"),
  async fn() {
    const profile = await makeProfile()
    const client = new Client(Deno.env.get("BN_BINARY")!, profile)
    try {
      // Nothing works before hello.
      const early = await client.request("bn.subscribe", {})
      assertEquals(early.error.data.kind, "negotiation_failed")
      const hello = await client.result("bn.hello", {
        versions: ["0.0", "1.0"],
        client: { name: "stdio-test", version: "1" },
        viewport: { cols: 40, rows: 25 },
      })
      assertEquals(hello.version, "1.0")
      assertEquals(hello.limits.frame_bytes, 1048576)

      const session = new Session(client)
      await session.start()
      const epoch = session.mirror.at.epoch
      assertEquals(epoch, hello.epoch)

      // Startup prompts, then a semantic choose into a loaded world.
      await session.acknowledge(() =>
        session.mirror.interaction.interaction?.context === "MAIN_MENU"
      )
      // The main menu is tabbed: tab rows have no pane_id, entries belong to the selected tab only.
      const menu = () => {
        const choices = session.mirror.interaction.interaction.choices as Value[]
        const tabs = choices.filter((entry) => entry.pane_id === undefined)
        return {
          tabs: tabs.map((entry) => entry.id),
          selected: tabs.filter((entry) => entry.selected).map((entry) => entry.id),
          entries: choices.filter((entry) => entry.pane_id !== undefined),
        }
      }
      const allTabs = [
        "tab:motd",
        "tab:new_game",
        "tab:load",
        "tab:world",
        "tab:settings",
        "tab:help",
        "tab:credits",
        "tab:quit",
      ]
      assertEquals(menu().tabs, allTabs)
      // The text tabs publish their text as the interaction message.
      for (const [label, tab] of [["MOTD", "tab:motd"], ["Credits", "tab:credits"]]) {
        assertEquals((await session.choose(label)).stages.at(-1), "completed")
        assertEquals(menu().selected, [tab])
        assert(session.mirror.interaction.interaction.message.length > 20, label)
        assert(!session.mirror.interaction.interaction.message.includes("<color"), label)
      }
      const settings = await session.choose("Settings")
      assertEquals(settings.stages, ["received", "validated", "executing", "completed"])
      assertEquals(menu().tabs, allTabs)
      assertEquals(menu().selected, ["tab:settings"])
      assertEquals(menu().entries.map((entry) => entry.id), [
        "settings:options",
        "settings:keybindings",
        "settings:autopickup",
        "settings:safemode",
        "settings:distractions",
        "settings:colors",
      ])
      const newGame = await session.choose("New Game")
      assertEquals(newGame.stages, ["received", "validated", "executing", "completed"])
      assert(session.eventTypes.includes("interaction.changed"))
      assertEquals(menu().tabs, allTabs)
      assertEquals(menu().selected, ["tab:new_game"])
      assertEquals(
        menu().entries.map((entry) => [entry.id, entry.pane_id]),
        [
          "custom",
          "preset",
          "random",
          "play_now_default",
          "play_now",
          "tutorial",
          "defence",
        ].map((id) => [`new_game:${id}`, "new_game"]),
      )

      // Entering the world replaces the authority: the command is interrupted, the stream resyncs.
      const chosenAt = performance.now()
      const tutorial = await session.choose("Tutorial")
      assertEquals(tutorial.stages.at(-1), "interrupted")
      await checkLoading(session.loading, chosenAt)
      // An interrupted command says why: the world it ran in is gone.
      const interrupted = await client.result("bn.command.result", {
        epoch,
        command_id: tutorial.id,
      })
      assertEquals([interrupted.stage, interrupted.error], ["interrupted", "stale_epoch"])
      const resync = await client.note()
      assertEquals(resync.method, "bn.resync")
      session.resyncs.push(resync.params)
      assertEquals(session.resyncs[0].reason, "world_replaced")
      assertEquals(session.resyncs[0].epoch, epoch)
      await session.start()
      assertNotEquals(session.mirror.at.epoch, epoch)
      assert(session.mirror.interaction.readiness.game_ready, "world loaded")

      await session.acknowledge(() => session.mirror.interaction.interaction === null)
      const moves = ["RIGHT", "LEFT", "UP", "DOWN"]
      const move = moves.find((id) =>
        session.mirror.interaction.actions.some((a: Value) => a.id === id)
      )
      assert(move, `no movement action in ${JSON.stringify(session.mirror.interaction.actions)}`)

      // The loaded world is known: cells and the avatar.
      assert(session.mirror.cells.size > 0, "known cells")
      // The avatar carries the native sidebar: every limb, the needs and the time of day.
      const sidebar = session.mirror.avatar.sidebar
      assert(sidebar.limbs.length >= 6, "limbs")
      assertEquals(typeof sidebar.limbs[0].hp, "number")
      assertEquals(typeof sidebar.hunger.text, "string")
      assert(["walk", "run", "crouch", "prone"].includes(sidebar.movement.mode))
      assert(sidebar.location.text.length > 0, "location name")
      // A tiled client gets the engine's own tile selection for what it sees.
      const visible = [...session.mirror.cells.values()].filter((entry) =>
        entry.known === "visible"
      )
      assert(visible.length > 0 && visible.every((entry) => entry.terrain?.subtile !== undefined))
      assert(visible.every((entry) => [0, 1, 2, 3].includes(entry.terrain.rotation)))
      assert(visible.every((entry) => Number.isInteger(entry.light) && entry.light >= 0))
      assert(visible.some((entry) => entry.terrain.subtile !== "unconnected"), "connected terrain")
      assert(["player_male", "player_female"].includes(session.mirror.avatar.look?.tile))
      assert(["spring", "summer", "autumn", "winter"].includes(session.mirror.environment?.season))
      const avatarBefore = session.mirror.avatar.at
      const typesBefore = session.eventTypes.length
      const before = BigInt(session.mirror.at.sequence)
      const step = await session.submit({ kind: "action", action_id: move })
      assertEquals(step.stages, ["received", "validated", "executing", "completed"])
      assert(BigInt(session.mirror.at.sequence) > before, "the boundary published an event")
      const completed = session.stages.get(step.id!)!
      assertEquals(completed.at(-1), "completed")
      // Movement publishes what the avatar now knows, and the avatar moved by one square.
      const worldTypes = session.eventTypes.slice(typesBefore).filter((type) =>
        type === "cells.seen" || type === "coverage.moved"
      )
      assert(worldTypes.length > 0, `world change event after moving: ${session.eventTypes}`)
      const avatarAfter = session.mirror.avatar.at
      assertEquals(
        Math.abs(avatarAfter.x - avatarBefore.x) + Math.abs(avatarAfter.y - avatarBefore.y),
        1,
      )

      // Lost-notification recovery.
      const recovered = await client.result("bn.command.result", {
        epoch: session.mirror.at.epoch,
        command_id: step.id,
      })
      assertEquals(recovered.stage, "completed")
      assert(recovered.at)

      // Stale submit: rejected with the reason, nothing executes, the clock stays.
      const clock = { ...session.mirror.at }
      const stale = await session.submit(
        { kind: "action", action_id: move },
        { revision: (BigInt(clock.revision) - 1n).toString() },
      )
      assertEquals(stale.stages.at(-1), "rejected")
      assertEquals(session.mirror.at, clock)
      const rejected = await client.result("bn.command.result", {
        epoch: clock.epoch,
        command_id: stale.id,
      })
      assertEquals([rejected.stage, rejected.error], ["rejected", "stale_revision"])

      // Waypoint travel: the first click publishes the engine's own route, the second walks it.
      await session.acknowledge(() => session.mirror.interaction.interaction === null)
      const here = session.mirror.avatar.at
      const reach = (pos: Value) => Math.max(Math.abs(pos.x - here.x), Math.abs(pos.y - here.y))
      const open = (entry: Value) =>
        entry.known === "visible" && !entry.furniture && !entry.vehicle &&
        /floor|dirt|grass|pavement|sidewalk/.test(entry.terrain?.id ?? "")
      const candidates = [...session.mirror.cells.values()]
        .filter((entry) => open(entry) && reach(entry.at) >= 2 && reach(entry.at) <= 8)
        .map((entry) => entry.at)
        .sort((a, b) => reach(b) - reach(a))
      assert(candidates.length > 0, `no open square in view: ${JSON.stringify(here)}`)
      // Ask the engine until it finds a route; a wall or an unreachable square plans none.
      let aim: Value
      let plan: Value
      for (const candidate of candidates.slice(0, 40)) {
        plan = await session.submit({ kind: "travel", pos: candidate })
        if (plan.stages.at(-1) === "rejected") {
          const why = await client.result("bn.command.result", {
            epoch: session.mirror.at.epoch,
            command_id: plan.id,
          })
          throw new Error(
            `travel rejected: ${why.error} ${
              JSON.stringify(session.mirror.interaction).slice(0, 600)
            }`,
          )
        }
        assertEquals(plan.stages, ["received", "validated", "executing", "completed"])
        if (session.mirror.route.length >= 2) {
          aim = candidate
          break
        }
      }
      assert(
        aim,
        `the engine planned a route: ${
          JSON.stringify([here, candidates.slice(0, 3), session.mirror.route])
        }`,
      )
      assertEquals(session.mirror.route.at(-1), aim, "the route ends at the clicked square")
      assertEquals(session.mirror.avatar.at, here, "the first click does not move")
      const planned = session.mirror.route.length

      const eventsBefore = session.eventTypes.length
      const walk = await session.submit({ kind: "travel", pos: aim })
      assertEquals(walk.stages.at(-1), "completed")
      assertEquals(session.mirror.route, [], "confirming clears the plan")
      const walkEvents = session.eventTypes.length - eventsBefore
      assert(walkEvents >= planned, `events per step: ${walkEvents} for ${planned} squares`)
      // Arrived (or stopped where the engine stopped, never past the destination).
      const arrived = session.mirror.avatar.at
      assertEquals(arrived, aim, "avatar stands on the destination")

      // A click outside the view is rejected without effect.
      const clock2 = { ...session.mirror.at }
      const far = await session.submit({ kind: "travel", pos: { ...aim, x: aim.x + 5000 } })
      assertEquals(far.stages.at(-1), "rejected")
      assertEquals(session.mirror.at, clock2)

      // The declared 40x25 viewport is the clickable view, which follows the avatar. Its far east
      // edge lies outside the 36x24 window of the default terminal, yet a click there is accepted.
      const size = (view: Value) => [view.max.x - view.min.x + 1, view.max.y - view.min.y + 1]
      assertEquals(size(session.mirror.view!), [40, 25])
      const edge = { ...session.mirror.avatar.at, x: session.mirror.view!.max.x }
      const wide = await session.submit({ kind: "travel", pos: edge })
      assertEquals(wide.stages.at(-1), "completed", "a click inside the declared view is accepted")

      // Shrinking is a native window resize, bounded by the native minimum terminal: the same click
      // is outside the new view and rejected without effect.
      await session.viewport(30, 10)
      const [columns] = size(session.mirror.view!)
      assert(columns < 40, `the window shrank: ${JSON.stringify(session.mirror.view)}`)
      const clock3 = { ...session.mirror.at }
      const narrow = await session.submit({
        kind: "travel",
        pos: { ...edge, x: session.mirror.avatar.at.x + 19 },
      })
      assertEquals(narrow.stages.at(-1), "rejected")
      assertEquals(session.mirror.at, clock3)

      // The reconstructed state equals a fresh subscribe.
      const fresh = await session.subscribe()
      assertEquals(fresh.state(), session.mirror.state())
      await client.close()
    } finally {
      client.kill()
      await Deno.remove(profile, { recursive: true })
    }
  },
})

Deno.test({
  name: "stdio: walking the tutorial to the stairs and back keeps every command completing",
  ignore: !Deno.env.get("BN_BINARY"),
  async fn() {
    const profile = await makeProfile()
    const client = new Client(Deno.env.get("BN_BINARY")!, profile)
    try {
      const session = await enterTutorial(client)
      const stairs = { x: 1210, y: 1214 }
      const log: string[] = []
      const run = async (operation: Value) => {
        const result = await session.submit(operation)
        log.push(`${JSON.stringify(operation)} ${result.stages.at(-1)}`)
        assertEquals(result.stages.at(-1), "completed", log.slice(-6).join("\n"))
      }
      const acknowledgePopups = async () => {
        while (session.mirror.interaction.interaction) {
          const prompt = session.mirror.interaction.interaction
          assertEquals(prompt.context, "POPUP_WAIT")
          await run({ kind: "choose", choice_id: prompt.choices[0].id })
        }
      }
      // Lessons pop up on the way; each one is a prompt the client can answer.
      for (let step = 0; step < 60; step++) {
        await acknowledgePopups()
        const at = session.mirror.avatar.at
        if (at.x === stairs.x && at.y === stairs.y) break
        const dir = at.x === 1202 && at.y < 1214 ? "DOWN" : at.x < stairs.x ? "RIGHT" : "LEFT"
        await run({ kind: "action", action_id: dir })
      }
      await acknowledgePopups()
      assertEquals(session.mirror.avatar.at, { dim: "", ...stairs, z: 0 })
      await run({ kind: "action", action_id: "LEVEL_DOWN" })
      await acknowledgePopups()
      assertEquals(session.mirror.avatar.at.z, -1)
      await run({ kind: "action", action_id: "LEFT" })
      await run({ kind: "action", action_id: "RIGHT" })
      await run({ kind: "action", action_id: "LEVEL_UP" })
      assertEquals(session.mirror.avatar.at.z, 0)

      // Look mode publishes its cursor as a target interaction and its own actions; it can be left.
      await run({ kind: "action", action_id: "look" })
      const ids = session.mirror.interaction.actions.map((a: Value) => a.id)
      const look = session.mirror.interaction.interaction
      assertEquals([look.kind, look.context], ["target", "LOOK"])
      assertEquals(look.target.current, session.mirror.avatar.at)
      assert(ids.includes("QUIT") && !ids.includes("look"), ids.join())
      await run({ kind: "action", action_id: "QUIT" })
      assert(session.mirror.interaction.actions.some((a: Value) => a.id === "look"))
      await client.close()
    } finally {
      client.kill()
      await Deno.remove(profile, { recursive: true })
    }
  },
})

Deno.test({
  name: "stdio: the tutorial crafting menu lists recipes, searches and closes",
  ignore: !Deno.env.get("BN_BINARY"),
  async fn() {
    const profile = await makeProfile()
    const client = new Client(Deno.env.get("BN_BINARY")!, profile)
    try {
      const session = await enterTutorial(client)
      await session.acknowledge(() => session.mirror.interaction.interaction === null)
      const interaction = () => session.mirror.interaction.interaction
      const opened = await session.submit({ kind: "action", action_id: "craft" })
      assertEquals(opened.stages.at(-1), "completed")
      assertEquals(interaction().context, "CRAFTING")
      assertEquals(interaction().kind, "choices")
      assertEquals(interaction().field.id, "filter")
      // Tab rows have no pane; the sub-tab and recipe rows belong to the selected tab.
      const rows = () => interaction().choices as Value[]
      const tabs = () => rows().filter((entry) => entry.id.startsWith("tab:"))
      const recipes = () => rows().filter((entry) => entry.id.startsWith("recipe:"))
      assert(tabs().length > 1 && tabs().every((entry) => entry.pane_id === undefined))
      assertEquals(tabs().filter((entry) => entry.selected).length, 1)
      const selected = tabs().find((entry) => entry.selected)
      assert(rows().some((entry) => entry.id.startsWith("subtab:") && entry.pane_id !== undefined))

      // Each tab lists its own recipes, with the native detail text on the row.
      let tab: Value
      for (const candidate of tabs()) {
        if (candidate.id === selected.id) continue
        assertEquals(
          (await session.submit({ kind: "choose", choice_id: candidate.id })).stages.at(-1),
          "completed",
        )
        assertEquals(interaction().context, "CRAFTING")
        if (recipes().length > 0) {
          tab = candidate
          break
        }
      }
      assert(tab, "some tab lists recipes")
      assertEquals(tabs().filter((entry) => entry.selected).map((entry) => entry.id), [tab.id])
      assert(recipes().every((entry) => entry.pane_id === tab.id.slice(4)))
      const first = recipes()[0]
      assert(first.description.includes("Time to complete"), first.description)
      assert(!first.description.includes("<color"), first.description)

      // Searching replaces the list; clearing restores it.
      const all = recipes().length
      const word = first.label.split(" ")[0]
      const search = (value: string) =>
        session.submit({ kind: "fill", field_id: "filter", value, submit: true })
      assertEquals((await search(word)).stages.at(-1), "completed")
      assertEquals(interaction().field.value, word)
      assert(recipes().length > 0 && recipes().length <= all)
      assert(recipes().some((entry) => entry.label.includes(word)))
      assertEquals((await search("")).stages.at(-1), "completed")
      assertEquals(recipes().length, all)

      // Closing returns to the map.
      assertEquals((await session.submit({ kind: "cancel" })).stages.at(-1), "completed")
      assertEquals(interaction(), null)
      assert(session.mirror.interaction.actions.some((a: Value) => a.id === "craft"))
      await client.close()
    } finally {
      client.kill()
      await Deno.remove(profile, { recursive: true })
    }
  },
})

Deno.test({
  name: "stdio: waiting five minutes passes five minutes and completes when input is awaited again",
  ignore: !Deno.env.get("BN_BINARY"),
  async fn() {
    const profile = await makeProfile()
    const client = new Client(Deno.env.get("BN_BINARY")!, profile)
    try {
      const session = await enterTutorial(client)
      const turn = () => BigInt(session.mirror.environment.turn)
      // Tutorial lessons pop up as turns pass; they are prompts of their own.
      const lessons = () =>
        session.acknowledge(() => session.mirror.interaction.interaction === null)
      await lessons()
      const wait = async (label: string) => {
        const menu = await session.submit({ kind: "action", action_id: "wait" })
        assertEquals(menu.stages.at(-1), "completed")
        assertEquals(session.mirror.interaction.interaction.kind, "choices")
        const before = turn()
        const chosen = await session.choose(label)
        assertEquals(chosen.stages.at(-1), "completed")
        await lessons()
        return turn() - before
      }
      // The menu is offered every time, and the activity is not cut short at a boundary.
      assertEquals(await wait("5 minutes"), 300n)
      assertEquals(await wait("5 minutes"), 300n)
      await client.close()
    } finally {
      client.kill()
      await Deno.remove(profile, { recursive: true })
    }
  },
})

Deno.test({
  name: "stdio: a long wait is interrupted mid-activity like the native interrupt key",
  ignore: !Deno.env.get("BN_BINARY"),
  async fn() {
    const profile = await makeProfile()
    const client = new Client(Deno.env.get("BN_BINARY")!, profile)
    try {
      const session = await enterTutorial(client)
      await session.acknowledge(() => session.mirror.interaction.interaction === null)
      // Tutorial lessons pop up while the first turns pass; let them out of the way first.
      await session.submit({ kind: "action", action_id: "wait" })
      await session.choose("5 minutes")
      await session.acknowledge(() => session.mirror.interaction.interaction === null)
      // Nothing runs, so there is nothing to interrupt.
      const idle = await client.request("bn.interrupt", {})
      assertEquals(idle.error.data.kind, "not_ready")

      assertEquals(
        (await session.submit({ kind: "action", action_id: "wait" })).stages.at(-1),
        "completed",
      )
      const before = BigInt(session.mirror.environment.turn)
      const hours = 6n * 3600n
      const choice = session.mirror.interaction.interaction.choices.find((entry: Value) =>
        entry.label.startsWith("6 hours")
      )
      assert(choice, "6 hours entry")
      // The request is answered while the activity runs; the activity asks whether to stop.
      const waiting = await session.submit(
        { kind: "choose", choice_id: choice.id },
        {},
        async () => {
          assertEquals(await client.result("bn.interrupt", {}), {})
        },
      )
      assertEquals(waiting.stages.at(-1), "completed")
      const query = session.mirror.interaction.interaction
      assert(query, "the activity asks before stopping")
      assert(BigInt(session.mirror.environment.turn) - before < hours, "stopped early")
      const asked = BigInt(session.mirror.environment.turn)
      await session.choose("Yes")
      assertEquals(session.mirror.interaction.interaction, null)
      const stopped = BigInt(session.mirror.environment.turn)
      assert(stopped - before < hours, `the wait ended early: ${stopped - before}`)
      assert(stopped - asked < 60n, "the confirmed stop ends the activity at once")
      await client.close()
    } finally {
      client.kill()
      await Deno.remove(profile, { recursive: true })
    }
  },
})

Deno.test({
  name: "stdio: the native message log reaches the client in order with repeat counts",
  ignore: !Deno.env.get("BN_BINARY"),
  async fn() {
    const profile = await makeProfile()
    const client = new Client(Deno.env.get("BN_BINARY")!, profile)
    try {
      const session = await enterTutorial(client)
      const before = session.mirror.messages.length
      // The same native message twice in a row is one line whose count rises.
      for (let step = 0; step < 2; step++) {
        const up = await session.submit({ kind: "action", action_id: "LEVEL_UP" })
        assertEquals(up.stages.at(-1), "completed")
      }
      const lines = session.mirror.messages
      assertEquals(lines.length, before + 1, "the repeat replaced its line")
      assert(lines.at(-1)!.text.startsWith("You can't"), lines.at(-1)!.text)
      assertEquals([lines.at(-1)!.count, lines.at(-1)!.kind], [2, "info"])
      const ids = lines.map((line) => BigInt(line.id))
      assertEquals(ids, [...ids].sort((a, b) => (a < b ? -1 : a > b ? 1 : 0)), "log order")
      assert(session.eventTypes.filter((type) => type === "message.logged").length >= 2)
      await client.close()
    } finally {
      client.kill()
      await Deno.remove(profile, { recursive: true })
    }
  },
})

/** Main menu to the Settings tab entry `id`, at the first menu boundary. */
async function openSetting(session: Session, id: string) {
  await session.acknowledge(() => session.mirror.interaction.interaction?.context === "MAIN_MENU")
  await session.submit({ kind: "choose", choice_id: "tab:settings" })
  assertEquals((await session.submit({ kind: "choose", choice_id: id })).stages.at(-1), "completed")
}

const shown = (session: Session): Value[] => session.mirror.interaction.interaction.choices
const rowOf = (session: Session, id: string) => shown(session).find((entry) => entry.id === id)
const press = (session: Session, id: string) => session.submit({ kind: "choose", choice_id: id })
const setTo = (session: Session, id: string, value: string) =>
  session.submit({ kind: "fill", field_id: id, value, submit: true })

/** Leave a settings screen the way Escape does and answer its save prompt. */
async function leaveAndSave(session: Session, done: () => boolean) {
  assertEquals((await session.submit({ kind: "cancel" })).stages.at(-1), "completed")
  assertEquals((await session.choose("Yes")).stages.at(-1), "completed")
  await session.acknowledge(done)
}

const inMainMenu = (session: Session) => () =>
  session.mirror.interaction.interaction?.context === "MAIN_MENU"

async function startSession(client: Client): Promise<Session> {
  await client.result("bn.hello", {
    versions: ["1.0"],
    client: { name: "settings-test", version: "1" },
  })
  const session = new Session(client)
  await session.start()
  return session
}

Deno.test({
  name: "stdio: options and keybindings are changed through the wire, persist and take effect",
  ignore: !Deno.env.get("BN_BINARY"),
  async fn() {
    const profile = await makeProfile()
    const binary = Deno.env.get("BN_BINARY")!
    let client = new Client(binary, profile)
    try {
      let session = await startSession(client)
      await openSetting(session, "settings:options")
      const options = session.mirror.interaction.interaction
      assertEquals(options.context, "OPTIONS")
      const pages = shown(session).filter((entry) => entry.pane_id === undefined)
      assert(pages.length >= 5 && pages.every((entry) => entry.id.startsWith("page:")))
      // Find the page that holds an option, as a client would when a tab is clicked.
      const find = async (id: string) => {
        for (const page of pages) {
          await press(session, page.id)
          if (rowOf(session, id)) return rowOf(session, id)
        }
        throw new Error(`no option ${id}`)
      }
      const safe = await find("option:SAFEMODE")
      assertEquals(safe.editor.type, "bool")
      assertEquals(safe.editor.value, "true")
      assertEquals((await setTo(session, "option:SAFEMODE", "false")).stages.at(-1), "completed")
      assertEquals(rowOf(session, "option:SAFEMODE").editor.value, "false")
      // A number outside the native range is refused and changes nothing.
      const near = await find("option:SAFEMODEPROXIMITY")
      assertEquals(near.editor.type, "integer")
      const clock = { ...session.mirror.at }
      assertEquals(
        (await setTo(session, near.id, String(near.editor.maximum + 1))).stages.at(-1),
        "rejected",
      )
      assertEquals(session.mirror.at, clock)
      assertEquals((await setTo(session, near.id, "7")).stages.at(-1), "completed")
      assertEquals(rowOf(session, near.id).editor.value, "7")
      // A select takes one of its own values; a foreign one is refused.
      const select = shown(session).concat(...[]).find((entry) =>
        entry.editor?.type === "select"
      ) ??
        await (async () => {
          for (const page of pages) {
            await press(session, page.id)
            const found = shown(session).find((entry) => entry.editor?.type === "select")
            if (found) return found
          }
        })()
      assert(select, "a select option exists")
      assertEquals((await setTo(session, select.id, "no such value")).stages.at(-1), "rejected")
      await leaveAndSave(session, inMainMenu(session))
      await client.close()

      // Relaunch on the same profile: the saved values are what the menu shows.
      client = new Client(binary, profile)
      session = await startSession(client)
      await openSetting(session, "settings:options")
      assertEquals((await find("option:SAFEMODE")).editor.value, "false")
      assertEquals((await find("option:SAFEMODEPROXIMITY")).editor.value, "7")
      assertEquals((await session.submit({ kind: "cancel" })).stages.at(-1), "completed")
      await session.acknowledge(() =>
        session.mirror.interaction.interaction?.context === "MAIN_MENU"
      )

      // In the game, the same menu applies a saved change at once: safe mode follows the option.
      await press(session, "tab:new_game")
      await press(session, "new_game:tutorial")
      assertEquals((await client.note()).method, "bn.resync")
      session = new Session(client)
      await session.start()
      await session.acknowledge(() => session.mirror.interaction.interaction === null)
      // Safe mode is off after the native toggle; the saved option turns the sidebar flag back on.
      assertEquals(
        (await session.submit({ kind: "action", action_id: "safemode" })).stages.at(-1),
        "completed",
      )
      await session.acknowledge(() => session.mirror.interaction.interaction === null)
      assertEquals(session.mirror.avatar.sidebar.safe_mode.enabled, false)
      // The in-game keybindings menu binds a key to Options, which then opens the same menu.
      assertEquals(
        (await session.submit({ kind: "action", action_id: "HELP_KEYBINDINGS" })).stages.at(-1),
        "completed",
      )
      await setTo(session, session.mirror.interaction.interaction.field.id, "Options")
      assert(rowOf(session, "action:open_options"), "Options is listed")
      await press(session, "mode:add_local")
      await press(session, "action:open_options")
      assertEquals((await setTo(session, "field:key", "=")).stages.at(-1), "completed")
      await answerPrompts(session)
      await leaveAndSave(session, () => session.mirror.interaction.interaction === null)
      const opened = await session.submit({ kind: "action", action_id: "open_options" })
      assertEquals(opened.stages.at(-1), "completed")
      assertEquals(session.mirror.interaction.interaction.context, "OPTIONS")
      await find("option:AUTOSAFEMODE")
      assertEquals((await setTo(session, "option:AUTOSAFEMODE", "true")).stages.at(-1), "completed")
      assertEquals((await session.submit({ kind: "cancel" })).stages.at(-1), "completed")
      assertEquals((await session.choose("Yes")).stages.at(-1), "completed")
      await session.acknowledge(() => session.mirror.interaction.interaction === null)
      assertEquals(session.mirror.avatar.sidebar.safe_mode.enabled, true)
      await client.close()
    } finally {
      client.kill()
      await Deno.remove(profile, { recursive: true })
    }
  },
})

/** The native yes/no prompts that may sit between two keybinding steps. */
async function answerPrompts(session: Session) {
  while (session.mirror.interaction.interaction?.context !== "HELP_KEYBINDINGS") {
    assertEquals((await session.choose("Yes")).stages.at(-1), "completed")
  }
}

const keysOf = (session: Session, id: string) => rowOf(session, id).columns[0].value

Deno.test({
  name: "stdio: a keybinding is removed and added through the wire, persists and moves the avatar",
  ignore: !Deno.env.get("BN_BINARY"),
  async fn() {
    const profile = await makeProfile()
    const binary = Deno.env.get("BN_BINARY")!
    let client = new Client(binary, profile)
    try {
      let session = await startSession(client)
      await openSetting(session, "settings:keybindings")
      const menu = session.mirror.interaction.interaction
      assertEquals([menu.title, menu.field.type], ["Keybindings", "text"])
      assertEquals(
        shown(session).filter((entry) => entry.pane_id === "modes").map((entry) => entry.id),
        ["mode:add_local", "mode:add_global", "mode:remove"],
      )
      // The filter is the native one: only the matching actions remain.
      await setTo(session, menu.field.id, "Move East")
      assertEquals(
        shown(session).filter((entry) => entry.pane_id === "actions").map((entry) => entry.id),
        ["action:RIGHT"],
      )
      assertEquals(keysOf(session, "action:RIGHT"), "l, RIGHT, 6 or NUMPAD_6")
      // Remove every key, then add one.
      await press(session, "mode:remove")
      assertEquals(rowOf(session, "mode:remove").selected, true)
      await press(session, "action:RIGHT")
      await answerPrompts(session)
      assertEquals(rowOf(session, "action:RIGHT").columns[1].value, "Unbound")
      await press(session, "mode:add_global")
      await press(session, "action:RIGHT")
      const prompt = session.mirror.interaction.interaction
      assertEquals(prompt.field.type, "key")
      // A key the game does not know is refused; a real one is taken.
      assertEquals((await setTo(session, prompt.field.id, "no such key")).stages.at(-1), "rejected")
      assertEquals((await setTo(session, prompt.field.id, "z")).stages.at(-1), "completed")
      await answerPrompts(session)
      assertEquals(keysOf(session, "action:RIGHT"), "z")
      await leaveAndSave(session, inMainMenu(session))
      await client.close()

      // Relaunch on the same profile: the menu shows the saved key, and it moves the avatar.
      client = new Client(binary, profile)
      session = await startSession(client)
      await openSetting(session, "settings:keybindings")
      await setTo(session, session.mirror.interaction.interaction.field.id, "Move East")
      assertEquals(keysOf(session, "action:RIGHT"), "z")
      assertEquals((await session.submit({ kind: "cancel" })).stages.at(-1), "completed")
      await session.acknowledge(() =>
        session.mirror.interaction.interaction?.context === "MAIN_MENU"
      )
      await press(session, "tab:new_game")
      await press(session, "new_game:tutorial")
      assertEquals((await client.note()).method, "bn.resync")
      session = new Session(client)
      await session.start()
      await session.acknowledge(() => session.mirror.interaction.interaction === null)
      const before = session.mirror.avatar.at
      assertEquals(
        (await session.submit({ kind: "action", action_id: "RIGHT" })).stages.at(-1),
        "completed",
      )
      assertEquals(session.mirror.avatar.at.x, before.x + 1)
      await client.close()
    } finally {
      client.kill()
      await Deno.remove(profile, { recursive: true })
    }
  },
})

Deno.test({
  name: "stdio: the other settings screens are structured and a distraction persists",
  ignore: !Deno.env.get("BN_BINARY"),
  async fn() {
    const profile = await makeProfile()
    const binary = Deno.env.get("BN_BINARY")!
    let client = new Client(binary, profile)
    try {
      let session = await startSession(client)
      const leave = async () => {
        assertEquals((await session.submit({ kind: "cancel" })).stages.at(-1), "completed")
        await session.acknowledge(inMainMenu(session))
      }
      // Each screen publishes its tabs and columns or rows; the rule screens start empty.
      for (
        const [entry, context, first] of [
          ["settings:autopickup", "AUTO_PICKUP", "column:1"],
          ["settings:safemode", "SAFEMODE", "page:0"],
          ["settings:colors", "COLORS", "column:1"],
        ]
      ) {
        await openSetting(session, entry)
        const screen = session.mirror.interaction.interaction
        assertEquals([screen.context, shown(session)[0].id], [context, first])
        await leave()
      }
      const colors = await (async () => {
        await openSetting(session, "settings:colors")
        return shown(session).filter((row) => row.id.startsWith("color:"))
      })()
      assert(colors.length > 10 && colors.every((row) => row.columns.length === 2))
      // Choosing a color row opens the native list of colors for the cursor's column.
      await press(session, colors[0].id)
      assert(session.mirror.interaction.interaction.choices.length > 10)
      assertEquals((await session.submit({ kind: "cancel" })).stages.at(-1), "completed")
      assertEquals(session.mirror.interaction.interaction.context, "COLORS")
      await leave()

      await openSetting(session, "settings:distractions")
      const rows = shown(session)
      assert(rows.length >= 8 && rows.every((row) => row.editor.type === "bool"))
      const target = rows[0]
      assertEquals(target.editor.value, "true")
      assertEquals((await setTo(session, target.id, "false")).stages.at(-1), "completed")
      assertEquals(rowOf(session, target.id).columns[0].value, "Disabled")
      await leave()
      await client.close()

      client = new Client(binary, profile)
      session = await startSession(client)
      await openSetting(session, "settings:distractions")
      assertEquals(rowOf(session, target.id).editor.value, "false")
      await client.close()
    } finally {
      client.kill()
      await Deno.remove(profile, { recursive: true })
    }
  },
})

Deno.test({
  name: "stdio: an offered action without a key binding is executed by its id",
  ignore: !Deno.env.get("BN_BINARY"),
  async fn() {
    const profile = await makeProfile()
    const client = new Client(Deno.env.get("BN_BINARY")!, profile)
    try {
      const session = await enterTutorial(client)
      await session.acknowledge(() => session.mirror.interaction.interaction === null)
      assert(
        session.mirror.interaction.actions.some((a: Value) => a.id === "pickup_feet"),
        "pickup_feet is offered",
      )
      const picked = await session.submit({ kind: "action", action_id: "pickup_feet" })
      assertEquals(picked.stages.at(-1), "completed")
      await client.close()
    } finally {
      client.kill()
      await Deno.remove(profile, { recursive: true })
    }
  },
})

Deno.test({
  name: "stdio: a long activity sends progress heartbeats while it runs",
  ignore: !Deno.env.get("BN_BINARY"),
  async fn() {
    const profile = await makeProfile()
    const client = new Client(Deno.env.get("BN_BINARY")!, profile)
    try {
      const session = await enterTutorial(client)
      await session.acknowledge(() => session.mirror.interaction.interaction === null)
      // Tutorial lessons pop up on the first turns; let them out of the way.
      await session.submit({ kind: "action", action_id: "wait" })
      await session.choose("5 minutes")
      await session.acknowledge(() => session.mirror.interaction.interaction === null)

      await session.submit({ kind: "action", action_id: "wait" })
      const choice = session.mirror.interaction.interaction.choices.find((entry: Value) =>
        entry.label.startsWith("6 hours")
      )
      assert(choice, "6 hours entry")
      const before = session.progress
      const started = performance.now()
      // Interrupt after a few heartbeats' worth of time; the activity keeps no boundary until then.
      const waiting = await session.submit(
        { kind: "choose", choice_id: choice.id },
        {},
        async () => {
          await new Promise((resolve) => setTimeout(resolve, 7000))
          await client.result("bn.interrupt", {})
        },
      )
      assertEquals(waiting.stages.at(-1), "completed")
      const seconds = (performance.now() - started) / 1000
      assert(seconds > 6, `the wait lasted ${seconds} s`)
      assert(session.progress > before, "the engine sent heartbeats while the activity ran")
      await client.close()
    } finally {
      client.kill()
      await Deno.remove(profile, { recursive: true })
    }
  },
})

Deno.test({
  name: "stdio: the native mouse view describes a square and a context click is the right button",
  ignore: !Deno.env.get("BN_BINARY"),
  async fn() {
    const profile = await makeProfile()
    const client = new Client(Deno.env.get("BN_BINARY")!, profile)
    try {
      const session = await enterTutorial(client)
      const here = session.mirror.avatar.at
      const describe = (pos: Value, boundary = session.mirror.interaction.boundary_id) =>
        client.request("bn.world.describe", {
          epoch: session.mirror.at.epoch,
          boundary_id: boundary,
          pos,
        })
      // The avatar's own square: the native mouse view prints something, and nothing changes.
      const before = JSON.stringify(session.mirror.state())
      const described = await describe(here)
      assert(described.result.lines.length > 0, JSON.stringify(described))
      assert(described.result.lines.every((line: string) => line === line.trimEnd()))
      assertEquals(JSON.stringify(session.mirror.state()), before)
      // A square outside the terrain window and a stale boundary are refused with their kinds.
      const far = await describe({ ...here, x: here.x + 5000 })
      assertEquals(far.error.data.kind, "validation_failed")
      const stale = await describe(here, "boundary:old")
      assertEquals(stale.error.data.kind, "stale_boundary")

      // Right click on a far square: the native SEC_SELECT finds nothing relevant and the command
      // still completes; on the avatar's square it is the pickup action.
      const nothing = await session.submit({ kind: "context", pos: { ...here, x: here.x + 4 } })
      assertEquals(nothing.stages.at(-1), "completed")
      const self = await session.submit({ kind: "context", pos: here })
      assertEquals(self.stages.at(-1), "completed")
      await client.close()
    } finally {
      client.kill()
      await Deno.remove(profile, { recursive: true })
    }
  },
})

Deno.test({
  name: "stdio: the overmap opens as a structured view, moves its cursor and closes",
  ignore: !Deno.env.get("BN_BINARY"),
  async fn() {
    const profile = await makeProfile()
    const client = new Client(Deno.env.get("BN_BINARY")!, profile)
    try {
      const session = await enterTutorial(client)
      await session.acknowledge(() => session.mirror.interaction.interaction === null)
      const opened = await session.submit({ kind: "action", action_id: "map" })
      assertEquals(opened.stages.at(-1), "completed")
      const view = () => session.mirror.interaction.interaction
      assertEquals(view().context, "OVERMAP")
      assertEquals(view().kind, "target")
      const first = view().overmap
      assert(first.cols > 20 && first.rows > 10, `${first.cols}x${first.rows}`)
      assertEquals(first.cells.length, first.rows)
      assertEquals(first.cells[0].length, first.cols)
      assert(first.legend.some((line: string) => line.trim().length > 0), "sidebar text")
      assertEquals(view().target.unit, "omt")
      assertEquals(view().target.current, first.player)
      // The window is centered on the cursor.
      assertEquals(first.origin.x, first.player.x - Math.floor(first.cols / 2))

      const to = { ...first.player, x: first.player.x + 3, y: first.player.y - 2 }
      const moved = await session.submit({ kind: "set_target", pos: to })
      assertEquals(moved.stages.at(-1), "completed")
      assertEquals(view().target.current, to)
      assertEquals(view().overmap.origin.x, first.origin.x + 3)
      assertEquals(view().overmap.player, first.player)

      // A square outside the safe bounds is refused.
      const far = await session.submit({
        kind: "set_target",
        pos: { ...to, x: 1 << 30 },
      })
      assertEquals(far.stages.at(-1), "rejected")

      assertEquals((await session.submit({ kind: "cancel" })).stages.at(-1), "completed")
      assertEquals(view(), null)
      assert(session.mirror.interaction.actions.some((a: Value) => a.id === "map"))
      await client.close()
    } finally {
      client.kill()
      await Deno.remove(profile, { recursive: true })
    }
  },
})
