/**
 * @module
 * A client session over the engine's stream: the mirror it builds, the commands it submits and the
 * stages it follows to their end.
 */
import { assert, assertEquals, assertNotEquals } from "@std/assert"
import { Mirror, stackPick } from "./mirror.ts"
import type { Client } from "./stdio_client.ts"

// deno-lint-ignore no-explicit-any
type Value = any

export class Session {
  mirror!: Mirror
  /** The operation kinds submitted. */
  kinds = new Set<string>()
  stages = new Map<string, string[]>()
  /** Pushed frames the client may be sent before it pays; 0 for a client that asked for no credits. */
  window = 0
  #unpaid = 0

  constructor(readonly client: Client) {}

  /** Subscribes again after a `bn.resync`: the snapshot replaces the mirror; command stages that arrived meanwhile are handled after it, and no event may. */
  async resubscribe() {
    const header = await this.client.result("bn.subscribe", {})
    const early = this.client.notes.splice(0)
    await this.start(header)
    for (const note of early) {
      assertNotEquals(note.method, "bn.events", "an event between a resync and its snapshot")
      this.handle(note)
    }
  }

  async start(subscribed?: Value) {
    const header = subscribed ?? await this.client.result("bn.subscribe", {})
    const parts: Value[] = []
    while (parts.length < header.parts) {
      const note = await this.client.note()
      assertEquals(note.method, "bn.snapshot.part")
      parts.push(note.params)
    }
    this.mirror = Mirror.fromSnapshot(header, parts)
  }

  async pay() {
    if (this.window === 0 || this.#unpaid < Math.max(1, this.window / 2)) return
    const frames = this.#unpaid
    this.#unpaid = 0
    await this.client.result("bn.credit", { frames })
  }

  refused = ""

  handle(note: Value) {
    const { method, params } = note
    if (method === "bn.events" || method === "bn.command") this.#unpaid++
    if (method === "bn.events") {
      this.mirror.applyEvents(params.epoch, params.events)
    } else if (method === "bn.command") {
      if (params.error) this.refused = params.error
      const stages = this.stages.get(params.command_id) ?? ["received"]
      stages.push(params.stage)
      this.stages.set(params.command_id, stages)
    } else if (!["bn.loading", "bn.progress", "bn.resync"].includes(method)) {
      throw new Error(`unexpected notification ${method}`)
    }
  }

  /** Submit against what the mirror believes, then follow pushes until the command is terminal. */
  async submit(operation: Value) {
    this.kinds.add(operation.kind)
    const mirror = this.mirror
    const top = mirror.interaction.stack.at(-1)
    const offered = operation.kind === "invoke"
      ? top?.affordances.find((entry: Value) => entry.id === operation.affordance_id)
      : undefined
    const { command_id: id } = await this.client.result("bn.command.submit", {
      epoch: mirror.at.epoch,
      // An invoke names what its affordance's scope asks for; every other operation, the boundary.
      expect: operation.kind === "invoke"
        ? offered?.expect === "boundary"
          ? { revision: mirror.at.revision, boundary_id: mirror.interaction.boundary_id }
          : { screen_id: top.screen_id }
        : {
          revision: mirror.at.revision,
          boundary_id: mirror.interaction.boundary_id,
          schema_id: mirror.interaction.interaction?.schema_id ?? null,
        },
      operation,
    })
    this.stages.set(id, ["received"])
    while (!["completed", "rejected", "interrupted"].includes(this.stages.get(id)!.at(-1)!)) {
      const note = await this.client.note()
      this.handle(note)
      if (note.method === "bn.resync") await this.resubscribe()
      await this.pay()
    }
    return this.stages.get(id)!.at(-1)!
  }

  /** Submit and expect the command to complete. */
  async run(operation: Value) {
    assertEquals(
      await this.submit(operation),
      "completed",
      `${JSON.stringify(operation)} ${this.refused}`,
    )
  }

  /** Choose the entry; the stage is returned, as loading a world ends its command `interrupted`. */
  async pick(label: string) {
    const choice = this.mirror.interaction.interaction?.choices?.find((entry: Value) =>
      entry.enabled && entry.label === label
    )
    if (choice) return await this.submit({ kind: "choose", choice_id: choice.id })
    // A screen of components has no choices: its tab or row is picked by its own affordance.
    const pick = stackPick(this.mirror, label)
    assert(pick, `no choice ${label}`)
    return await this.submit({ kind: "invoke", ...pick })
  }

  async choose(label: string) {
    assertEquals(await this.pick(label), "completed", label)
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

  /** Let the tutorial's pop-ups out of the way. */
  clear() {
    return this.acknowledge(() => this.mirror.interaction.interaction === null)
  }
}
