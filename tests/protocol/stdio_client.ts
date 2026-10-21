/**
 * @module
 * A client that drives a real engine process over stdio, one JSON-RPC frame per line, checking every
 * message it receives against the schema.
 */
import { assert } from "@std/assert"
import { Ajv2020 } from "npm:ajv@8.17.1/dist/2020.js"

// deno-lint-ignore no-explicit-any
type Value = any
const schemaFile = new URL("../../docs/schema/engine-client/0.2.schema.json", import.meta.url)
// Every message the engine really sends is checked against the schema.
const schema: Value = JSON.parse(await Deno.readTextFile(schemaFile))
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
/** What a hello names: the first 16 hex digits of the SHA-256 of the raw bytes of the schema. */
export const schemaHash = "sha256:" + Array.from(
  new Uint8Array(await crypto.subtle.digest("SHA-256", await Deno.readFile(schemaFile))),
  (byte) => byte.toString(16).padStart(2, "0"),
).join("").slice(0, 16)

/** The engine is killed after this long without protocol traffic, busy or not: loading steps and activities send heartbeats. */
export const stall = 120_000

/** Rejects when `work` outlives `ms`, so a hung engine fails the test instead of the whole run. */
export const withDeadline = <T>(work: Promise<T>, ms: number, why: string) => {
  let timer: ReturnType<typeof setTimeout> | undefined
  const expired = new Promise<never>((_, reject) => {
    timer = setTimeout(() => reject(new Error(why)), ms)
  })
  return Promise.race([work, expired]).finally(() => clearTimeout(timer))
}

export class Client {
  #process: Deno.ChildProcess
  #writer: WritableStreamDefaultWriter<Uint8Array>
  #reader: ReadableStreamDefaultReader<Uint8Array>
  #decoder = new TextDecoder()
  #buffer = ""
  #id = 0
  /** Notifications that arrived while waiting for something else, in order. */
  notes: Value[] = []
  /** Responses to raw frames written with `raw`, which no `request` waits for. */
  replies: Value[] = []
  /** Pause after every chunk read, to drain the pipe slowly. */
  readDelayMs = 0
  /** Bytes read from the engine's stdout. */
  bytes = 0

  /** Starts `binary --client=mcp ...args`. */
  constructor(binary: string, args: string[], stderr: "null" | "piped" = "null") {
    this.#process = new Deno.Command(binary, {
      args: ["--client=mcp", ...args],
      stdin: "piped",
      stdout: "piped",
      stderr,
    }).spawn()
    this.#writer = this.#process.stdin.getWriter()
    this.#reader = this.#process.stdout.getReader()
  }

  async #line(): Promise<Value> {
    let timer: ReturnType<typeof setTimeout> | undefined
    try {
      while (!this.#buffer.includes("\n")) {
        clearTimeout(timer)
        timer = setTimeout(() => this.#process.kill("SIGKILL"), stall)
        const part = await this.#reader.read()
        assert(!part.done, "engine closed stdout")
        this.bytes += part.value.byteLength
        this.#buffer += this.#decoder.decode(part.value, { stream: true })
        if (this.readDelayMs) await new Promise((resolve) => setTimeout(resolve, this.readDelayMs))
      }
    } finally {
      clearTimeout(timer)
    }
    const end = this.#buffer.indexOf("\n")
    const line = this.#buffer.slice(0, end)
    this.#buffer = this.#buffer.slice(end + 1)
    const message = JSON.parse(line)
    // A batch is answered by one array of responses.
    return Array.isArray(message) ? message : { ...message, receivedAt: performance.now() }
  }

  /** Next frame that is not the response to a raw frame. */
  async #next(): Promise<Value> {
    while (true) {
      const message = await this.#line()
      if (message.method !== undefined || !this.rawIds.has(JSON.stringify(message.id))) {
        return message
      }
      this.replies.push(message)
    }
  }

  /** The ids written with `raw`; a malformed frame is answered with id null. */
  rawIds = new Set<string>()

  /**
   * Queues `text` as it is, one frame per line, and expects a response with each id in `ids`.
   * It does not wait for the engine to take the bytes: a pipe holds little, and an engine blocked on
   * its own output reads nothing until the client does.
   */
  raw(text: string, ids: unknown[] = []) {
    for (const id of ids) this.rawIds.add(JSON.stringify(id))
    this.#queue(text)
  }

  #queue(text: string) {
    this.#writer.write(new TextEncoder().encode(text)).catch(() => {})
  }

  /** Reads until `count` responses to raw frames have arrived; notifications on the way wait in `notes`. */
  async repliesUntil(count: number) {
    while (this.replies.length < count) {
      // Not `#next`, which keeps reading past a reply until a notification comes: none may follow.
      const message = await this.#line()
      if (Array.isArray(message)) {
        this.replies.push(...message)
        continue
      }
      if (message.method === undefined && this.rawIds.has(JSON.stringify(message.id))) {
        this.replies.push(message)
        continue
      }
      this.#validateNotification(message)
      this.notes.push(message)
    }
  }

  #validateNotification(message: Value) {
    const def = schema["x-notifications"][message.method]
    validate(def, message.params, message.method)
  }

  /** Drops what was queued for the engine and closes its stdin, as a client that died; a write in flight ends when the engine reads or exits. */
  closeInput() {
    this.#writer.abort().catch(() => {})
  }

  signal(sig: Deno.Signal) {
    this.#process.kill(sig)
  }

  /** Waits for the engine to leave; `stderr` is its text when piped. */
  async exit(ms: number) {
    const stderr = this.#process.stderr
    return await withDeadline(
      Promise.all([this.#process.status, stderr ? new Response(stderr).text() : ""]),
      ms,
      "the engine outlived its client",
    ).then(([status, text]) => ({ status, stderr: text }))
  }

  /** Reads and drops what the engine writes until it closes stdout, as a client that still reads while it sends a bad frame. */
  async drain() {
    for (let part = await this.#reader.read(); !part.done; part = await this.#reader.read());
  }

  /** Closes the engine's stdout for good, as a client that died. */
  async abandonOutput() {
    await this.#reader.cancel()
  }

  async request(method: string, params: unknown): Promise<Value> {
    const id = ++this.#id
    this.#queue(JSON.stringify({ jsonrpc: "2.0", id, method, params }) + "\n")
    while (true) {
      const message = await this.#next()
      if (message.id === id) {
        if (message.error) {
          validate("application_error", message.error.data, `${method} error`)
        } else {
          const def = schema["x-methods"][method].result
          validate(def, message.result, `${method} result`)
        }
        return message
      }
      assert(message.id === undefined, `unexpected response ${JSON.stringify(message)}`)
      this.#validateNotification(message)
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
    const message = await this.#next()
    this.#validateNotification(message)
    return message
  }

  async close() {
    await this.#writer.close()
    const status = await this.#process.status
    assert(status.success, `engine exit ${JSON.stringify(status)}`)
  }

  get pid() {
    return this.#process.pid
  }

  kill() {
    try {
      this.#process.kill("SIGKILL")
    } catch { /* already gone */ }
  }
}
