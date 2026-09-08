#!/usr/bin/env -S deno test --allow-read --allow-write --allow-run --allow-env
/** @module Regression tests for streamed output, exit status, and timeout cleanup. */
import { assert, assertEquals, assertRejects } from "@std/assert"
import { join } from "@std/path"
import { watchTest } from "./test-watchdog.ts"

Deno.test("watchdog preserves output and exit codes", async () => {
  const directory = await Deno.makeTempDir()
  try {
    for (const code of [0, 7]) {
      const name = `exit-${code}`
      assertEquals(
        await watchTest({
          command: Deno.execPath(),
          args: [
            "eval",
            `console.log('stdout marker'); console.error('stderr marker'); Deno.exit(${code})`,
          ],
          name,
          directory,
          timeoutMs: 30_000,
        }),
        code,
      )
      assertEquals(await Deno.readTextFile(join(directory, name, "stdout.log")), "stdout marker\n")
      assertEquals(await Deno.readTextFile(join(directory, name, "stderr.log")), "stderr marker\n")
    }
  } finally {
    await Deno.remove(directory, { recursive: true })
  }
})

Deno.test("watchdog collects diagnostics while alive and kills after diagnostic failure", async () => {
  const directory = await Deno.makeTempDir()
  let pid = 0
  try {
    const code = await watchTest({
      command: Deno.execPath(),
      args: ["eval", "console.log('running'); setInterval(() => {}, 1000)"],
      name: "hang",
      directory,
      timeoutMs: 2000,
      diagnose: async (childPid, path) => {
        pid = childPid
        if (Deno.build.os === "linux") await Deno.stat(`/proc/${pid}`)
        assertEquals(await Deno.readTextFile(join(path, "stdout.log")), "running\n")
        throw new Error("diagnostic fixture failure")
      },
    })
    assertEquals(code, 124)
    assert(pid > 0)
    assert(
      (await Deno.readTextFile(join(directory, "hang", "diagnostic-error.txt"))).includes(
        "diagnostic fixture failure",
      ),
    )
    if (Deno.build.os === "linux") {
      await assertRejects(() => Deno.stat(`/proc/${pid}`), Deno.errors.NotFound)
    }
  } finally {
    await Deno.remove(directory, { recursive: true })
  }
})

Deno.test("watchdog rejects invalid timeout before starting", async () => {
  for (const timeoutMs of [0, -1, Infinity, NaN]) {
    await assertRejects(
      () =>
        watchTest({
          command: "must-not-start",
          args: [],
          name: "invalid",
          directory: "unused",
          timeoutMs,
        }),
      Error,
      "positive and finite",
    )
  }
})
