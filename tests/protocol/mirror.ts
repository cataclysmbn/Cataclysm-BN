/**
 * @module
 * Client-side reconstruction of the 1.0 stream: snapshot header plus parts, then events.
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

export class Mirror {
  at!: Clock
  coverage?: { min: Pos; max: Pos }
  interaction!: Value
  avatar?: Value
  environment?: Value
  cells = new Map<string, Value>()
  entities = new Map<string, Value>()

  static fromSnapshot(header: Value, parts: Value[]): Mirror {
    const mirror = new Mirror()
    mirror.at = header.at
    mirror.coverage = header.coverage
    mirror.interaction = header.interaction
    mirror.avatar = header.avatar
    mirror.environment = header.environment
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
    copy.interaction = structuredClone(this.interaction)
    copy.avatar = structuredClone(this.avatar)
    copy.environment = structuredClone(this.environment)
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
      this.at = { epoch, sequence: event.sequence, revision: event.revision }
    }
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
    for (const cell of changes.cells ?? []) this.cells.set(key(cell.at), cell)
    for (const pos of changes.forgotten ?? []) {
      assert(this.cells.delete(key(pos)), "forgotten names a known cell")
    }
    for (const entity of changes.entities ?? []) this.entities.set(entity.id, entity)
    for (const gone of changes.gone ?? []) {
      assert(this.entities.delete(gone.id), "gone names a known entity")
    }
    if (changes.avatar) this.avatar = changes.avatar
    if (changes.environment) this.environment = changes.environment
    if (changes.interaction) this.interaction = changes.interaction
  }

  /** Order-independent comparable form. */
  state() {
    const sorted = <T>(map: Map<string, T>) => [...map.entries()].sort(([a], [b]) => a < b ? -1 : 1)
    return {
      at: this.at,
      coverage: this.coverage ?? null,
      interaction: this.interaction,
      avatar: this.avatar ?? null,
      environment: this.environment ?? null,
      cells: sorted(this.cells),
      entities: sorted(this.entities),
    }
  }
}
