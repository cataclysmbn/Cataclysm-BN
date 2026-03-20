# Lua

## New default

Use Lua anywhere frequency is low enough.

That means the old rule of "Lua only for final rename" is too strict for this feature.

## Split of work

### C++ owns hot path

- schema load
- inventory scan
- candidate filtering
- builder state
- fast preview accumulators
- save/load
- stack keys
- validation of history mode

### Lua may own cold path

- full stat fold
- name selection
- trait / tag fold
- result item creation
- schema-specific byproducts
- compact fallback uncraft rules
- oddball mod logic

That should be read as "Lua owns schema-specific cold logic after C++ normalization", not "Lua touches every live item path directly".

## Why this split

Hot path is where perf matters.

Cold path is where flexibility matters.

So:

- preview stays cheap in C++
- finalize and rare rebuilds can call Lua

## Proposed Lua hooks

Use short names.

- `lua.fast` optional preview supplement
- `lua.full` full stat fold
- `lua.name` naming
- `lua.make` result item creation
- `lua.unc` fallback uncraft

Example:

```json
{
  "lua": {
    "full": "proc.food.full",
    "name": "proc.food.name",
    "make": "proc.food.make"
  }
}
```

## Hook data in

- schema id
- normalized chosen facts
- fast preview blob
- history mode
- crafter context if needed

## Hook data out

`lua.full` returns compiled values such as:

- kcal
- vitamins
- allergens
- tags
- final mass / vol
- final display fields

`lua.make` returns either:

- a result item spec
- or edits to apply to the archetype item

Recommended rule:

- Lua decides what should be created
- C++ validates and materializes it into typed item data

This still offloads schema-specific make logic while keeping save/load and stacking deterministic.

## Safety rules

- no Lua on every cursor move
- no background-thread Lua
- all Lua output is normalized into a typed C++ proc blob before save
- stack keys depend on normalized output, not raw Lua tables
- Lua should consume fact rows and return fact-derived results, not hold long-lived ownership of live items

## Fallback rules

If Lua is missing or errors:

- log the issue
- fall back to `drv.fast` when possible
- reject finalize if the schema requires Lua-only output

## Subtask cut

1. new proc Lua hook registration
2. typed bridge for chosen parts
3. typed bridge for compiled blob
4. `lua.make` result bridge
5. error / fallback path
