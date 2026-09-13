#!/usr/bin/env -S deno run --allow-read --allow-write --allow-run --allow-env
/** @module Stream test output and collect bounded hang diagnostics before killing a shard. */
import { join } from "@std/path"

export type WatchOptions = {
  command: string
  args: string[]
  env?: Record<string, string>
  name: string
  directory: string
  timeoutMs: number
  diagnose?: (pid: number, directory: string) => Promise<void>
}

const diagnosticCommand = async (command: string, args: string[], path: string) => {
  try {
    const child = new Deno.Command(command, {
      args,
      stdout: "piped",
      stderr: "piped",
    }).spawn()
    const timer = setTimeout(() => {
      try {
        child.kill("SIGKILL")
      } catch { /* Already exited. */ }
    }, 30_000)
    const result = await child.output().finally(() => clearTimeout(timer))
    await Deno.writeFile(path, result.stdout)
    await Deno.writeFile(path, result.stderr, { append: true })
    await Deno.writeTextFile(path, `\nExit code: ${result.code}\n`, { append: true })
  } catch (error) {
    await Deno.writeTextFile(path, `${error}\n`, { append: true })
  }
}

const diagnose = async (pid: number, directory: string) => {
  if (Deno.build.os === "windows") {
    const procdump = Deno.env.get("CATA_TEST_PROCDUMP") ?? "procdump.exe"
    await diagnosticCommand(procdump, [
      "-accepteula",
      "-ma",
      String(pid),
      join(directory, "hang.dmp"),
    ], join(directory, "dump.txt"))
    const cdb = Deno.env.get("CATA_TEST_CDB")
    if (cdb) {
      await diagnosticCommand(
        cdb,
        ["-z", join(directory, "hang.dmp"), "-c", "~* kb; q"],
        join(directory, "stacks.txt"),
      )
    }
  } else {
    await diagnosticCommand(
      "ps",
      ["-e", "-o", "pid,ppid,stat,etime,pcpu,pmem,args"],
      join(directory, "processes.txt"),
    )
    await diagnosticCommand("gdb", [
      "--batch",
      "-nx",
      "-p",
      String(pid),
      "-ex",
      "set pagination off",
      "-ex",
      "thread apply all bt",
      "-ex",
      "detach",
    ], join(directory, "stacks.txt"))
  }
}

export const watchTest = async (options: WatchOptions): Promise<number> => {
  if (!Number.isFinite(options.timeoutMs) || options.timeoutMs <= 0) {
    throw new Error("Test timeout must be positive and finite")
  }
  const directory = join(options.directory, options.name)
  await Deno.mkdir(directory, { recursive: true })
  const linux = Deno.build.os === "linux"
  const child = new Deno.Command(linux ? "setsid" : options.command, {
    args: linux ? [options.command, ...options.args] : options.args,
    env: options.env,
    stdout: "piped",
    stderr: "piped",
  }).spawn()
  await Deno.writeTextFile(
    join(directory, "command.json"),
    JSON.stringify(
      {
        ...options,
        diagnose: undefined,
        pid: child.pid,
        started: new Date().toISOString(),
      },
      null,
      2,
    ),
  )
  console.log(`[${options.name}] PID ${child.pid}`)
  const stream = async (
    input: ReadableStream<Uint8Array>,
    filename: string,
    output: Pick<Deno.FsFile, "write">,
  ) => {
    const file = await Deno.open(join(directory, filename), {
      create: true,
      write: true,
      truncate: true,
    })
    try {
      for await (const chunk of input) {
        for (let offset = 0; offset < chunk.length;) {
          offset += await file.write(chunk.subarray(offset))
        }
        for (let offset = 0; offset < chunk.length;) {
          offset += await output.write(chunk.subarray(offset))
        }
      }
    } finally {
      file.close()
    }
  }
  const streams = Promise.all([
    stream(child.stdout, "stdout.log", Deno.stdout),
    stream(child.stderr, "stderr.log", Deno.stderr),
  ])
  let timer: ReturnType<typeof setTimeout> | undefined
  const expired = new Promise<null>((resolve) => {
    timer = setTimeout(() => resolve(null), options.timeoutMs)
  })
  try {
    const status = await Promise.race([child.status, expired])
    if (status === null) {
      console.error(`[${options.name}] Timed out; collecting thread stacks before termination`)
      await Deno.writeTextFile(
        join(directory, "timeout.txt"),
        `Timeout after ${options.timeoutMs}ms\n`,
      )
      try {
        await (options.diagnose ?? diagnose)(child.pid, directory)
      } catch (error) {
        await Deno.writeTextFile(join(directory, "diagnostic-error.txt"), `${error}\n`)
      } finally {
        if (Deno.build.os === "windows") {
          await diagnosticCommand(
            "taskkill",
            ["/PID", String(child.pid), "/T", "/F"],
            join(directory, "termination.txt"),
          )
        } else if (linux) {
          await diagnosticCommand(
            "kill",
            ["-KILL", "--", `-${child.pid}`],
            join(directory, "termination.txt"),
          )
        }
        try {
          child.kill("SIGKILL")
        } catch { /* Already exited. */ }
      }
      await child.status
      await streams
      return 124
    }
    await streams
    return status.code
  } finally {
    clearTimeout(timer)
  }
}
