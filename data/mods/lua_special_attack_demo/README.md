# Lua special attack demo

This standalone debug mod demonstrates these `Monster` Lua methods:

- `has_special_attack(attack_id)`
- `special_attack_ready(attack_id)`
- `use_special_attack(attack_id)`
- `get_special_attack_ids()`
- `special_attack_enabled(attack_id)` / `set_special_attack_enabled(attack_id, enabled)`
- `get_special_attack_cooldown(attack_id)` / `set_special_attack_cooldown(attack_id, turns)`

It contains two monsters: a tap demonstrator for the query/use semantics, and a phase
boss demonstrating the enable/disable pattern.

It does not depend on or modify the Lua AI Examples mod.

## Running the demonstration

1. Enable **Lua Special Attack Demo** in a test world and restart the game.
2. From the debug monster menu, spawn **Lua special attack demonstrator**
   (`mon_lua_special_attack_demo`) on an empty tile 4–5 tiles away in clear sight.
   Spawn only one so its messages are easy to follow.
3. Advance time one action at a time. Disable safe mode if it prevents waiting near the
   hostile-intent training robot. The robot is immobile and its attack deals zero damage.
4. Monster creation randomizes the initial cooldown. Once ready, attempts from several
   tiles away report `FAILED`, `Ready afterward: true`, and actor move cost `0`.
   This shows that readiness does not predict target or range checks and that actor failure
   does not consume cooldown.
5. Walk next to the robot and advance a turn. It reports `USED`,
   `Ready afterward: false`, and actor move cost `100`. A dodge still counts as handled use.
6. Stay adjacent. It reports `WAIT` until the normal cooldown expires, then uses the attack
   again. Move away after cooldown to see failed attempts resume.

`AI #` counts Lua AI calls, not seconds or player turns. Player speed and action duration can
change how many messages appear per player action. The configured cooldown is 7; the Lua code
neither reads its remaining value nor changes it. Messages are limited to the same z-level and
10-tile range.

The Lua AI invokes the actor at most once per action, spends at least 100 moves, and returns
`true` so the stock AI and stock special-attack scheduler do not run for this monster.

## Phase boss demonstration

`mon_lua_boss_phase_demo` shows the pattern for bosses: declare every attack in JSON, then
keep all but the current phase's attack disabled and let the stock scheduler choose from
what is left.

1. Spawn **Lua phase boss demonstrator** (`mon_lua_boss_phase_demo`) adjacent to you.
2. It announces `entering phase_opening` and from then on only performs its OPENING pattern.
3. Damage it below 2/3 HP, then below 1/3, to see it switch to MIDGAME and ENRAGE. Each
   phase message appears once, and only that phase's attack is ever performed.

Two things make this pattern safe. `boss_turn` enumerates with `get_special_attack_ids()`
instead of hard-coding IDs, so attacks inherited via `copy-from` or added by another mod
are covered. And it returns `false`, handing the turn back to the stock AI, so movement,
pathing, floor traps, drowning and trap-escape checks all still run — unlike the tap
demonstrator, which returns `true` and therefore takes over the entire turn.

## One special attack per action

A successful `use_special_attack` consumes the action's special-attack budget, so the stock
scheduler will not add a second attack on top of the one Lua picked. This means Lua can fire
exactly the attack it wants and still return `false` to keep stock movement. The budget is
per action, not per game turn, and it is cleared at the start of every `monster::move()`,
matching the stock scheduler's own once-per-action limit. A *failed* attempt does not consume
the budget, so the stock scheduler can still act.
