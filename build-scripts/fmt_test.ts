/** @module Verify staged formatter boundaries in isolated Git repositories. */

const formatter = new URL("./fmt.sh", import.meta.url)
const assert = (condition: boolean, message: string) => {
  if (!condition) throw new Error(message)
}

async function fixture(
  check: (root: string, run: (args?: string[]) => Promise<void>) => Promise<void>,
) {
  const root = await Deno.makeTempDir({ prefix: "cata-fmt-test-" })
  const env = {
    PATH: `${root}/bin:/usr/bin:/bin`,
    HOME: `${root}/home`,
    GIT_CONFIG_NOSYSTEM: "1",
    GIT_CONFIG_GLOBAL: "/dev/null",
    GIT_CONFIG_SYSTEM: "/dev/null",
    GIT_TERMINAL_PROMPT: "0",
    LANG: "C.UTF-8",
  }
  async function command(executable: string, args: string[]) {
    const result = await new Deno.Command(executable, {
      args,
      cwd: root,
      clearEnv: true,
      env,
      stdout: "piped",
      stderr: "piped",
    }).output()
    assert(result.success, new TextDecoder().decode(result.stderr))
  }
  try {
    for (const dir of ["bin", "home", "build-scripts"]) {
      await Deno.mkdir(`${root}/${dir}`)
    }
    await Deno.copyFile(formatter, `${root}/build-scripts/fmt.sh`)
    // Fixture executables record selection and stand in for external formatters.
    // No package manager, network, global configuration, or real Lua tool runs.
    const recorder =
      `#!/usr/bin/env -S ${Deno.execPath()} run --no-config --no-lock --allow-read=${root} --allow-write=${root}
const root = ${JSON.stringify(root)};
const name = new URL(import.meta.url).pathname.split('/').at(-1);
const args = Deno.args;
await Deno.writeTextFile(root + '/calls.jsonl', JSON.stringify({name, args}) + '\\n', {append: true});
if (name === 'deno') {
  const lua = args[0] === 'task';
  const files = args.slice(lua ? 3 : 1);
  const selected = files.length ? files : (lua ? ['staged.lua', 'unstaged.lua', 'untracked.lua'] : ['staged.md', 'staged.ts', 'unstaged.md', 'untracked.md']);
  for (const file of selected) {
    await Deno.writeTextFile(root + '/' + file, (await Deno.readTextFile(root + '/' + file)) + 'formatted\\n');
  }
}
`
    for (
      const path of ["bin/deno", "build-scripts/format-cpp.sh", "build-scripts/format-json.sh"]
    ) {
      await Deno.writeTextFile(`${root}/${path}`, recorder)
      await Deno.chmod(`${root}/${path}`, 0o755)
    }
    await command("/usr/bin/git", ["init", "--quiet"])
    for (const file of ["staged.md", "staged.ts", "staged.lua", "unstaged.md", "unstaged.lua"]) {
      await Deno.writeTextFile(`${root}/${file}`, "original\n")
    }
    await command("/usr/bin/git", [
      "add",
      "--",
      "staged.md",
      "staged.ts",
      "staged.lua",
      "unstaged.md",
      "unstaged.lua",
    ])
    await command("/usr/bin/git", [
      "-c",
      "user.name=Formatter Test",
      "-c",
      "user.email=formatter@example.invalid",
      "-c",
      "core.hooksPath=/dev/null",
      "commit",
      "--quiet",
      "-m",
      "fixture",
    ])
    for (const file of ["unstaged.md", "unstaged.lua", "untracked.md", "untracked.lua"]) {
      await Deno.writeTextFile(`${root}/${file}`, "private\n")
    }
    await check(root, (args = []) => command("/usr/bin/bash", ["build-scripts/fmt.sh", ...args]))
  } finally {
    await Deno.remove(root, { recursive: true })
  }
}

Deno.test("staged formatters select MD TS and Lua without reaching private files", async () => {
  await fixture(async (root, run) => {
    // Make a staged change, rather than merely re-add unchanged fixture files.
    for (const file of ["staged.md", "staged.ts", "staged.lua"]) {
      await Deno.writeTextFile(`${root}/${file}`, "staged\n")
    }
    const staged = await new Deno.Command("/usr/bin/git", {
      args: ["add", "--", "staged.md", "staged.ts", "staged.lua"],
      cwd: root,
      clearEnv: true,
      env: {
        PATH: "/usr/bin:/bin",
        HOME: `${root}/home`,
        GIT_CONFIG_NOSYSTEM: "1",
        GIT_CONFIG_GLOBAL: "/dev/null",
        GIT_CONFIG_SYSTEM: "/dev/null",
      },
    }).output()
    assert(staged.success, "fixture staging failed")
    await run()
    for (const file of ["unstaged.md", "unstaged.lua", "untracked.md", "untracked.lua"]) {
      assert(
        await Deno.readTextFile(`${root}/${file}`) === "private\n",
        `formatter reached ${file}`,
      )
    }
    for (const file of ["staged.md", "staged.ts", "staged.lua"]) {
      assert(
        await Deno.readTextFile(`${root}/${file}`) === "staged\nformatted\n",
        `formatter missed ${file}`,
      )
    }
  })
})

for (const file of ["staged space.md", "staged space.lua"]) {
  Deno.test(`staging only ${file} selects one formatter and restages its result`, async () => {
    await fixture(async (root, run) => {
      const git = async (args: string[]) => {
        const result = await new Deno.Command("/usr/bin/git", {
          args,
          cwd: root,
          clearEnv: true,
          env: {
            PATH: "/usr/bin:/bin",
            HOME: `${root}/home`,
            GIT_CONFIG_NOSYSTEM: "1",
            GIT_CONFIG_GLOBAL: "/dev/null",
            GIT_CONFIG_SYSTEM: "/dev/null",
          },
          stdout: "piped",
          stderr: "piped",
        }).output()
        assert(result.success, new TextDecoder().decode(result.stderr))
        return new TextDecoder().decode(result.stdout)
      }
      await Deno.writeTextFile(`${root}/${file}`, "staged\n")
      await git(["add", "--", file])
      await run()
      const calls = (await Deno.readTextFile(`${root}/calls.jsonl`)).trim().split("\n").map((
        line,
      ) => JSON.parse(line))
      const expected = file.endsWith(".lua") ? ["task", "dprint", "fmt", file] : ["fmt", file]
      assert(
        calls.length === 1 && JSON.stringify(calls[0].args) === JSON.stringify(expected),
        "wrong formatter selection",
      )
      assert(
        await git(["show", `:${file}`]) === "staged\nformatted\n",
        "formatted result not restaged",
      )
      for (const private_file of ["unstaged.md", "unstaged.lua", "untracked.md", "untracked.lua"]) {
        assert(
          await Deno.readTextFile(`${root}/${private_file}`) === "private\n",
          `formatter reached ${private_file}`,
        )
      }
    })
  })
}

Deno.test("staged non-game JSON goes to deno fmt and game JSON to the native formatter", async () => {
  await fixture(async (root, run) => {
    await Deno.mkdir(`${root}/.github`)
    await Deno.mkdir(`${root}/data`)
    await Deno.writeTextFile(`${root}/.github/vcpkg.json`, "{}\n")
    await Deno.writeTextFile(`${root}/data/item.json`, "[]\n")
    const add = await new Deno.Command("/usr/bin/git", {
      args: ["add", "--", ".github/vcpkg.json", "data/item.json"],
      cwd: root,
      clearEnv: true,
      env: { PATH: "/usr/bin:/bin", HOME: `${root}/home`, GIT_CONFIG_GLOBAL: "/dev/null" },
    }).output()
    assert(add.success, "fixture staging failed")
    await run()
    const calls = (await Deno.readTextFile(`${root}/calls.jsonl`)).trim().split("\n").map((line) =>
      JSON.parse(line)
    )
    const deno = calls.find((call) => call.name === "deno")
    const native = calls.find((call) => call.name === "format-json.sh")
    assert(
      JSON.stringify(deno?.args) === '["fmt",".github/vcpkg.json"]',
      "non-game JSON lost its deno fmt path",
    )
    assert(
      JSON.stringify(native?.args) === '["data/item.json"]',
      "game JSON must reach the native formatter only",
    )
  })
})

Deno.test("empty staging invokes no formatter", async () => {
  await fixture(async (root, run) => {
    await run()
    try {
      await Deno.stat(`${root}/calls.jsonl`)
      throw new Error("empty staging invoked a formatter")
    } catch (error) {
      if (!(error instanceof Deno.errors.NotFound)) throw error
    }
  })
})

Deno.test("explicit --all preserves intentional whole repository formatting", async () => {
  await fixture(async (root, run) => {
    await run(["--all"])
    const calls = (await Deno.readTextFile(`${root}/calls.jsonl`)).trim().split("\n").map((line) =>
      JSON.parse(line)
    )
    assert(calls.length === 4, "--all must invoke all four formatters")
    assert(JSON.stringify(calls[0].args) === '["fmt"]', "--all lost broad docs formatting")
    assert(
      JSON.stringify(calls[1].args) === '["task","dprint","fmt"]',
      "--all lost broad Lua formatting",
    )
    assert(
      calls[2].name === "format-cpp.sh" && calls[3].name === "format-json.sh",
      "--all lost existing helper calls",
    )
  })
})
