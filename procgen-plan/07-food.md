# Food First

## First live schema

Start with one builder:

- `sandwich`

Keep the first archetype minimal:

- `proc_sandwich`

## Food defaults

- mode default = `none`
- `compact` allowed for debugging and later mods
- list count stays `1`
- preview is incremental
- finalize can call Lua for full fold + result creation

## Sandwich roles

- `bread` x2
- `veg` 0..N
- `cheese` 0..N
- `cond` 0..N
- `meat` 0..N

## Food math split

### Fast C++ preview

- mass
- kcal
- allergens bitset
- rough vol from mass / dens
- count by role

### Full Lua finalize

- exact nutrition fold
- vitamin fold
- trait fold
- naming
- result item edits
- special cases like `manwich`

## Material use

Food parts should expose a small proc profile:

- dens
- kcal
- vitamins
- allergens
- tags
- spoil / rot class

Avoid full recursive component use in the hot path.

## Name order

1. schema rule
2. Lua override
3. generic fallback

## Food subtasks

1. proc food archetype
2. sandwich schema load
3. builder wiring
4. fast preview
5. Lua full fold
6. Lua make
7. consume / inspect integration
