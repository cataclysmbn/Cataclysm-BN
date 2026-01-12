# Deno Script Development

Write Deno/TypeScript scripts following Cataclysm: Bright Nights conventions.

## Trigger Phrases

- "create deno script"
- "write deno script"
- "add typescript script"
- "new deno tool"

## Required Conventions

### 1. Shebang and Permissions

Start scripts with the proper shebang and required permissions:

```typescript
#!/usr/bin/env -S deno run --allow-read --allow-write --allow-run --allow-env
```

Common permission flags:

- `--allow-read` - File system read access
- `--allow-write` - File system write access
- `--allow-run` - Run subprocesses
- `--allow-env` - Access environment variables
- `--allow-net` - Network access (only if needed)

### 2. Module Documentation

Add a module-level JSDoc comment explaining the script's purpose:

```typescript
/**
 * @module
 *
 * Brief description of what this script does
 *
 * Longer explanation if needed with examples or usage notes.
 */
```

This pattern is used in existing scripts like:

- `scripts/gen_cli_docs.ts`
- `scripts/utils.ts`
- `scripts/affected_files.ts`

### 3. Import Conventions

Use JSR imports for standard library and well-known packages:

```typescript
import { Command } from "@cliffy/command"
import { exists } from "@std/fs"
import { join } from "@std/path"
import { partition } from "@std/collections"
import * as v from "@valibot/valibot"
```

**DO NOT use npm or http imports** unless absolutely necessary. Prefer JSR.

### 4. Validation with Valibot

For data validation, use Valibot (NOT Zod):

```typescript
import * as v from "@valibot/valibot"

const MySchema = v.object({
  name: v.string(),
  count: v.pipe(v.number(), v.minValue(0)),
  tags: v.array(v.string()),
})

type MyType = v.InferOutput<typeof MySchema>

const parser = v.safeParser(MySchema)
const result = parser(data)
if (result.success) {
  const validated = result.output
}
```

### 5. CLI with Cliffy

For command-line interfaces, use Cliffy:

```typescript
import { Command } from "@cliffy/command"

const main = new Command()
  .name("script-name")
  .version("1.0.0")
  .description("What this script does")
  .option("-o, --output <file>", "Output file path")
  .arguments("<input:string>")
  .action(async ({ output }, input) => {
    // Implementation
  })

if (import.meta.main) {
  await main.parse(Deno.args)
}
```

### 6. File Operations

Use `@std/fs` for file system operations:

```typescript
import { exists, walk } from "@std/fs"
import { basename, join } from "@std/path"

if (await exists("path/to/file")) {
  const content = await Deno.readTextFile("path/to/file")
}

for await (const entry of walk("src", { exts: [".ts"] })) {
  console.log(entry.path)
}
```

### 7. Running Commands

Use `Deno.Command` for subprocesses:

```typescript
const cmd = new Deno.Command("git", {
  args: ["status"],
  stdout: "piped",
  stderr: "piped",
})

const { success, stdout, stderr } = await cmd.output()
if (success) {
  const output = new TextDecoder().decode(stdout)
}
```

### 8. Error Handling

Handle errors with proper types:

```typescript
try {
  await someOperation()
} catch (error) {
  const message = error instanceof Error ? error.message : String(error)
  console.error(`Error: ${message}`)
  Deno.exit(1)
}
```

### 9. Color Output

Define colors as utility functions:

```typescript
const colors = {
  red: (text: string) => `\x1b[31m${text}\x1b[0m`,
  yellow: (text: string) => `\x1b[33m${text}\x1b[0m`,
  green: (text: string) => `\x1b[32m${text}\x1b[0m`,
  dim: (text: string) => `\x1b[2m${text}\x1b[0m`,
}

console.log(colors.green("Success!"))
console.warn(colors.yellow("[WARNING] Something happened"))
console.error(colors.red("[ERROR] Failed"))
```

## Example Scripts to Reference

Look at these existing scripts for patterns:

1. **CLI Tool**: `scripts/gen_cli_docs.ts`
   - Shows proper shebang, module docs, CLI parsing
2. **Data Processing**: `scripts/utils.ts`
   - Shows Valibot usage, type utilities
3. **GitHub API**: `scripts/affected_files.ts`
   - Shows async iteration, external API calls
4. **Git Hooks**: `tools/hooks/pre-commit.ts`
   - Shows command execution, file categorization
5. **Installer**: `tools/hooks/install-hooks.ts`
   - Shows user interaction, file manipulation

## Project Structure

Scripts location:

- General utilities: `scripts/`
- Build/development tools: `tools/`
- Git hooks: `tools/hooks/`

## Testing

Before submitting:

```sh
deno check your-script.ts
deno fmt your-script.ts
deno lint your-script.ts
```

Make the script executable:

```sh
chmod +x your-script.ts
```

## Documentation

Add to `deno.jsonc` tasks if the script should be easily runnable:

```json
{
  "tasks": {
    "your-task": "deno run --allow-... scripts/your-script.ts"
  }
}
```

## Must Do

1. **ALWAYS** start with shebang: `#!/usr/bin/env -S deno run [permissions]`
2. **ALWAYS** add `@module` JSDoc comment
3. **ALWAYS** use Valibot for validation (NOT Zod)
4. **ALWAYS** prefer JSR imports over npm/http
5. **ALWAYS** use Cliffy for CLI interfaces
6. **ALWAYS** check with `deno check` before considering complete
7. **ALWAYS** make script executable (`chmod +x`)
8. **ALWAYS** use `if (import.meta.main)` guard for executable scripts

## Must Not Do

1. **NEVER** use Zod (use Valibot)
2. **NEVER** use npm: imports (use jsr:)
3. **NEVER** forget to add required permissions in shebang
4. **NEVER** commit without running `deno check` and `deno fmt`
5. **NEVER** use `@ts-ignore` or `@ts-expect-error`
6. **NEVER** leave scripts non-executable

## Common Patterns

### Check if Command Exists

```typescript
const commandExists = async (cmd: string): Promise<boolean> => {
  try {
    const process = new Deno.Command("command", {
      args: ["-v", cmd],
      stdout: "piped",
      stderr: "piped",
    })
    const { success } = await process.output()
    return success
  } catch {
    return false
  }
}
```

### Get Git Repository Root

```typescript
const getRepoRoot = async (): Promise<string> => {
  const cmd = new Deno.Command("git", {
    args: ["rev-parse", "--show-toplevel"],
    stdout: "piped",
  })
  const { stdout } = await cmd.output()
  return new TextDecoder().decode(stdout).trim()
}
```

### Read JSON Files Recursively

```typescript
import { walk } from "@std/fs"

for await (const entry of walk("data", { exts: [".json"] })) {
  const content = await Deno.readTextFile(entry.path)
  const data = JSON.parse(content)
  // Process data
}
```

### Parallel Async Operations

```typescript
const results = await Promise.all([
  operation1(),
  operation2(),
  operation3(),
])
```

## When to Write a Deno Script

Use Deno scripts for:

- Data migration/transformation
- Build automation
- Development tools
- Git hooks
- Code generation
- Documentation generation
- CLI utilities

Don't use Deno scripts for:

- Core game logic (use C++)
- Performance-critical operations (use C++)
- Things that should be in-game (use Lua bindings)
