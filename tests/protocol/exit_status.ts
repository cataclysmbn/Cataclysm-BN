/**
 * @module
 * Runs a command and writes its exit status as JSON to a file, for a host that does not pass its
 * child's status on (gamescope exits 0 whatever its child did).
 *
 * deno run --allow-run --allow-write tests/protocol/exit_status.ts <status.json> <command> [args...]
 */
const [file, command, ...args] = Deno.args
const status = await new Deno.Command(command, { args }).spawn().status
await Deno.writeTextFile(file, JSON.stringify(status))
