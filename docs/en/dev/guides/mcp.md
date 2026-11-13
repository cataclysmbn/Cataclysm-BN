---
title: Playing through MCP
---

The `cataclysm-bn-mcp` executable is an MCP server on stdio. An agent plays the game through its screens and controls.

```sh
cmake --preset linux-full -B out/build/linux-full-mcp -DMCP=ON -DTILES=ON -DCURSES=OFF
cmake --build out/build/linux-full-mcp --target cataclysm-bn-mcp
```

Add this server to the MCP host. Pass both `--userdir` and `--configdir`. Without them, an XDG build can read and write your normal configuration. Add `--world NAME` to load a world at start. Add `--replay-record PATH` to record the session. Play it again with the [replay guide](replay.md).

```json
{
  "mcpServers": {
    "bn": {
      "command": "/path/to/Cataclysm-BN/out/build/linux-full-mcp/src/cataclysm-bn-mcp",
      "args": [
        "--datadir",
        "/path/to/Cataclysm-BN/data/",
        "--userdir",
        "/path/to/bn-agent-profile/",
        "--configdir",
        "/path/to/bn-agent-profile/config/"
      ]
    }
  }
}
```

Every call except `ping` fails until the host sends `notifications/initialized`. The server also offers the resources `bn://screen` and `bn://state`.

| Tool             | Result                                                                              |
| ---------------- | ----------------------------------------------------------------------------------- |
| `bn.observe`     | The composed screen: `width`, `height`, `cursor`, `rows`, `styles`.                 |
| `bn.state`       | The world header. `part` reads one map part.                                        |
| `bn.press`       | Sends `keys`. Answers with the screen at the next input boundary.                   |
| `bn.actions`     | `input_id`, `category`, `timeout_ms`, and the `actions` with their bindings.        |
| `bn.interaction` | The top screen of the contract stack: `screen_id` and its affordances.              |
| `bn.interact`    | Invokes one affordance (`affordance_id`, `args`). Answers with the next top screen. |

`bn.interact` takes the same path as `bn.command.submit`. Send one input. Read the result before you choose the next input. One input can open another menu.

```json
{ "name": "bn.press", "arguments": { "keys": [{ "action": "inventory" }] } }
```

Put `input_id` from `bn.actions` in an event. The server rejects the event when the screen has changed since.

## Limits

- A client that sent `bn.hello` owns the input. `bn.press` and `bn.interact` then fail.
- A result larger than 1 MiB is a tool error.
- One key event uses exactly one of `action`, `key` (with optional `text`), `text`, or `mouse`.
- `modifiers` needs `key`.
- `keys` holds 1 to 256 events.
