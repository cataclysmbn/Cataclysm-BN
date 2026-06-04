# Other Items

## Compact rule

`compact` is not `id only`.

It must carry enough state to stop repair / uncraft exploits.

At minimum:

- `id`
- `n`
- `hp`
- `dmg`
- `chg`
- `fault`
- `mat`
- nested `proc` if needed

## Recoverable gear

Default `compact` kinds:

- melee
- armor
- furn
- vpart

Reason:

- exact child tree is often overkill
- id-only summaries are unsafe

## `knife spear` rule

Do not map child outputs to parent durability alone.

Instead store per-part summaries.

This prevents:

- `50% spear -> 100% knife`

and also preserves child proc state.

## Guns and tools

Do not replace current BN modular behavior.

Plan:

- procgen builds the core
- BN gunmods / toolmods / mags / ammo stay on top
- default mode for the core is `full`

This matches how BN already treats current assembly state.

## Ammo

Keep ammo simple.

Default:

- mode `none`
- compact only if a real use case appears

## Item subtasks

1. universal compact summaries
2. compact uncraft
3. damage routing to part summaries
4. spear example
5. gun/tool core bridge
