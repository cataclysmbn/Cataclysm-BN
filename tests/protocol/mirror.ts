/**
 * @module
 * Client-side reconstruction of the 0.1 stream: snapshot header plus parts, then events.
 * This is the reference algorithm the engine's reconstruction tests mirror (design 3.4).
 */
import { assert, assertEquals } from "@std/assert"

type Pos = { dim: string; x: number; y: number; z: number }
// deno-lint-ignore no-explicit-any
type Value = any

export type Clock = { epoch: string; sequence: string; revision: string }

const key = (pos: Pos) => `${pos.dim}|${pos.x}|${pos.y}|${pos.z}`
const inside = (area: { min: Pos; max: Pos }, pos: Pos) =>
  pos.dim === area.min.dim && pos.x >= area.min.x && pos.x <= area.max.x &&
  pos.y >= area.min.y && pos.y <= area.max.y && pos.z >= area.min.z && pos.z <= area.max.z

/** The newest log lines a snapshot carries. */
const maximumLogLines = 100

export class Mirror {
  at!: Clock
  coverage?: { min: Pos; max: Pos }
  /** The squares a map click selects (the terrain window); replaced whole by an event. */
  view?: { min: Pos; max: Pos }
  interaction!: Value
  avatar?: Value
  environment?: Value
  /** The planned, unconfirmed auto-move route; empty when nothing is planned. */
  route: Pos[] = []
  /** The native message log in order; a repeat replaces the line with the same id. */
  messages: { id: string; text: string; kind: string; color: string; count: number }[] = []
  cells = new Map<string, Value>()
  entities = new Map<string, Value>()

  static fromSnapshot(header: Value, parts: Value[]): Mirror {
    const mirror = new Mirror()
    mirror.at = header.at
    mirror.coverage = header.coverage
    mirror.view = header.view
    mirror.interaction = header.interaction
    mirror.avatar = header.avatar
    mirror.environment = header.environment
    mirror.route = header.route ?? []
    mirror.messages = header.messages ?? []
    for (const entity of header.entities) mirror.entities.set(entity.id, entity)
    assertEquals(parts.length, header.parts, "every announced part arrives")
    parts.forEach((part, index) => {
      assertEquals(part.index, index)
      assertEquals(part.last, index === parts.length - 1)
      assertEquals(part.at, header.at)
      for (const cell of part.cells) mirror.cells.set(key(cell.at), cell)
    })
    return mirror
  }

  /** Copy that shares nothing mutable with this mirror. */
  clone(): Mirror {
    const copy = new Mirror()
    copy.at = structuredClone(this.at)
    copy.coverage = structuredClone(this.coverage)
    copy.view = structuredClone(this.view)
    copy.interaction = structuredClone(this.interaction)
    copy.avatar = structuredClone(this.avatar)
    copy.environment = structuredClone(this.environment)
    copy.route = structuredClone(this.route)
    copy.messages = structuredClone(this.messages)
    copy.cells = structuredClone(this.cells)
    copy.entities = structuredClone(this.entities)
    return copy
  }

  /**
   * Apply events in order. Any gap, duplicate, epoch change or unfit change throws and leaves
   * this mirror exactly as it was: the batch is applied to a copy and then adopted.
   */
  applyEvents(epoch: string, events: Value[]) {
    const next = this.clone()
    next.#apply(epoch, events)
    Object.assign(this, next)
  }

  #apply(epoch: string, events: Value[]) {
    assertEquals(epoch, this.at.epoch, "epoch change needs a new subscribe")
    for (const event of events) {
      assertEquals(BigInt(event.sequence), BigInt(this.at.sequence) + 1n, "sequence is contiguous")
      const changes = event.changes ?? {}
      const stateful = Object.keys(changes).length > 0
      assertEquals(
        BigInt(event.revision),
        BigInt(this.at.revision) + (stateful ? 1n : 0n),
        "revision advances with state",
      )
      this.applyChanges(changes)
      if (event.type === "message.logged") this.#log(event.data)
      this.at = { epoch, sequence: event.sequence, revision: event.revision }
    }
  }

  #log(line: Mirror["messages"][number]) {
    const at = this.messages.findIndex((entry) => entry.id === line.id)
    if (at >= 0) this.messages[at] = line
    else this.messages.push(line)
    // The stream keeps the newest lines only.
    if (this.messages.length > maximumLogLines) this.messages.shift()
  }

  /** Mutates in place; callers apply it to a copy (see applyEvents). */
  applyChanges(changes: Value) {
    if (changes.coverage) {
      this.coverage = changes.coverage
      for (const [k, cell] of this.cells) {
        if (!inside(changes.coverage, cell.at)) this.cells.delete(k)
      }
      for (const [k, entity] of this.entities) {
        if (!inside(changes.coverage, entity.at)) this.entities.delete(k)
      }
    }
    if (changes.view) this.view = changes.view
    for (const cell of changes.cells ?? []) this.cells.set(key(cell.at), cell)
    for (const pos of changes.forgotten ?? []) {
      assert(this.cells.delete(key(pos)), "forgotten names a known cell")
    }
    for (const entity of changes.entities ?? []) this.entities.set(entity.id, entity)
    for (const gone of changes.gone ?? []) {
      assert(this.entities.delete(gone.id), "gone names a known entity")
    }
    if (changes.avatar) {
      // The members named in `keep` carry over from the previous avatar.
      const { keep = [], ...next } = changes.avatar
      for (const member of keep) next[member] = this.avatar?.[member]
      this.avatar = next
    }
    if (changes.environment) this.environment = changes.environment
    if (changes.route) this.route = changes.route
    if (changes.interaction) {
      // The engine leaves out `actions` when the new boundary keeps the previous ones.
      const actions = changes.interaction.actions ?? this.interaction.actions
      this.interaction = { ...changes.interaction, actions }
    }
  }

  /** Order-independent comparable form of the stream: state plus the message log a snapshot carries. */
  state() {
    const sorted = <T>(map: Map<string, T>) => [...map.entries()].sort(([a], [b]) => a < b ? -1 : 1)
    return {
      at: this.at,
      coverage: this.coverage ?? null,
      view: this.view ?? null,
      interaction: this.interaction,
      avatar: this.avatar ?? null,
      environment: this.environment ?? null,
      route: this.route,
      messages: this.messages,
      cells: sorted(this.cells),
      entities: sorted(this.entities),
    }
  }
}

/** The item rows of every list of a screen's components, header rows left out, with the affordance their list activates. */
export function itemRows(screen: Value): { id: string; label: string; activate: string }[] {
  const lists = (node: Value): Value[] =>
    !node ? [] : [
      ...(node.kind === "list" ? [node] : []),
      ...(node.child ? lists(node.child) : []),
      ...(node.children ?? []).flatMap(lists),
    ]
  return lists(screen.root).flatMap((list) =>
    list.rows.filter((row: Value) => !row.header).map((row: Value) => ({
      id: row.id,
      label: row.label,
      activate: list.activate,
    }))
  )
}

/** Whether the title screen is the top of the stack: the main menu publishes components, not a choice list. */
export const atMainMenu = (mirror: Mirror): boolean =>
  mirror.interaction.stack.at(-1)?.context === "MAIN_MENU"

/**
 * The invoke that picks `key` on the top screen of the stack, for a screen that publishes components
 * instead of choices: the tab, then the list row, whose id is `key` or whose label starts with it.
 */
export function stackPick(
  mirror: Mirror,
  key: string,
): { affordance_id: string; args: { row: string } } | undefined {
  const parts = (node: Value): Value[] =>
    !node
      ? []
      : [node, ...(node.child ? parts(node.child) : []), ...(node.children ?? []).flatMap(parts)]
  const named = (entry: Value) => entry.id === key || entry.label?.startsWith(key)
  const components = parts(mirror.interaction.stack.at(-1)?.root)
  for (const tabs of components.filter((part) => part.kind === "tabs" && part.select)) {
    const tab = tabs.tabs.find((entry: Value) => entry.enabled && named(entry))
    if (tab) return { affordance_id: tabs.select, args: { row: tab.id } }
  }
  for (const list of components.filter((part) => part.kind === "list" && part.activate)) {
    const row = list.rows.find((entry: Value) => !entry.header && entry.enabled && named(entry))
    if (row) return { affordance_id: list.activate, args: { row: row.id } }
  }
}
