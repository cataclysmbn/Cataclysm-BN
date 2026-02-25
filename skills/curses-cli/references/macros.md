# Macro Workflows

This reference covers how to discover and use runtime Lua action-menu macros from `curses-cli`, and how to add new macros in game code.

## Use macros from CLI

```bash
deno task pr:verify:curses-cli start --state-file /tmp/curses-macro.json --render-webp false
deno task pr:verify:curses-cli available-macros-json --state-file /tmp/curses-macro.json
deno task pr:verify:curses-cli inputs-jsonl --state-file /tmp/curses-macro.json
deno task pr:verify:curses-cli send-input --state-file /tmp/curses-macro.json --id macro:bn_macro_agent_context
deno task pr:verify:curses-cli stop --state-file /tmp/curses-macro.json
```

Notes:

- `available-macros-json` returns runtime macro entries exported by the game.
- `inputs-jsonl` includes macros as `macro:<id>` entries when macro export is present.
- Macro IDs are stable and should be used for automation scripts.

## Add a new macro in Lua stdlib

1. Add function logic in `data/lua/lib/action_menu_macros.lua`.
2. Register via `gapi.register_action_menu_entry({ ... })` with:
   - `id`: stable snake_case ID (for CLI `macro:<id>`)
   - `name`: user-facing label
   - `description`: concise automation-friendly description
   - `category`: grouping label (for action menu UX)
   - `fn`: callable Lua function
3. Keep behavior deterministic and side effects explicit.

Minimal pattern:

```lua
gapi.register_action_menu_entry({
  id = "bn_macro_example",
  name = locale.gettext("Example Macro"),
  description = locale.gettext("Prints a compact state message for automation."),
  category = "info",
  fn = function()
    gapi.add_msg("[AI] example macro ran")
  end,
})
```

## C++ export path used by CLI

- Action keys JSON: written by `input_context::handle_input()` in `src/input.cpp` to `CATA_AVAILABLE_KEYS_JSON`.
- Macro JSON: written by `lua_action_menu::dump_entries_to_json()` in `src/lua_action_menu.cpp` to `CATA_AVAILABLE_MACROS_JSON`.
- CLI launch sets both env vars in `scripts/curses-cli/common.ts` (`buildLaunchCommand`).

## Validation checklist

```bash
deno task pr:verify:curses-cli start --state-file /tmp/curses-macro-validate.json --render-webp false
deno task pr:verify:curses-cli available-macros-json --state-file /tmp/curses-macro-validate.json
deno task pr:verify:curses-cli inputs-jsonl --state-file /tmp/curses-macro-validate.json
deno task pr:verify:curses-cli stop --state-file /tmp/curses-macro-validate.json
```

Expected:

- New macro appears in `available-macros-json`.
- `inputs-jsonl` contains `macro:<id>`.
- `send-input --id macro:<id>` executes without mode drift.
