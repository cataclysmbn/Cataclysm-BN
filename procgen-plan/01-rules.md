# Rules

## Scope

Add a procedural composition layer that can grow from food into melee, armor, ammo, guns, tools, furniture, and vehicle parts.

The first live target is a sandwich builder.

## Non-goals for the first wave

- no rewrite of all static recipes
- no giant schema DSL
- no per-combination recipe generation
- no per-frame Lua
- no exact uncraft for `none`

## Hard defaults

- `1` builder recipe per schema
- `none | compact | full` are the only history modes
- outer model stays `item`-centric
- inner proc core stays data-oriented
- preview is hot path
- finalize is cold path
- hot path stays in C++
- cold path can lean on Lua
- hot path uses flat facts + indexes over copied item trees

## Terse naming policy

Prefer short words over long compounds.

Good:

- `cat`
- `res`
- `hist`
- `slot`
- `drv`
- `unc`
- `lua`
- `pick`
- `tag`
- `mat`

Bad:

- `result_archetype_definition`
- `derive_weight_sum`
- `component_selection_descriptor`

Keep names short but readable. Avoid one-letter keys unless the meaning is obvious from the surrounding object.

## History defaults

| kind             | def       | allow                     |
| ---------------- | --------- | ------------------------- |
| food             | `none`    | `none`, `compact`, `full` |
| med / disposable | `none`    | `none`, `compact`         |
| ammo             | `none`    | `none`, `compact`         |
| melee / armor    | `compact` | `compact`, `full`         |
| item             | `compact` | `none`, `compact`, `full` |
| gun / tool core  | `full`    | `compact`, `full`         |
| furn / vpart     | `compact` | `compact`, `full`         |

## Formula style

Use symbols in plan text and schema examples.

- `+` sum
- `-` subtract
- `*` multiply
- `/` divide
- `|` union
- `&` intersect
- `!` exclude
- `min()` minimum
- `max()` maximum
- `clamp()` clamp
- `wavg()` weighted average
- `worst()` worst source

Examples:

- mass = `sum(parts.mass)`
- dens = `wavg(parts.dens, parts.mass)`
- vol = `mass / max(dens, 1)`
- kcal = `sum(parts.kcal)`
- allergen = `parts.alg | parts.flag`
- fp = `hash(schema + sorted(parts) + mode)`

## Hot vs cold work

Hot work:

- recipe list
- builder open
- candidate filtering
- selection add/remove
- preview refresh

Cold work:

- finalize
- name polish
- deep stat fold
- exact output creation
- rare uncraft

Rule:

- C++ owns hot work
- Lua may own cold schema logic
- normalized typed blobs remain the save/stack source of truth
