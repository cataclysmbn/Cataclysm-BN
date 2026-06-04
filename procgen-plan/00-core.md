# Core Model

## What this is

This should not become a full-engine ECS rewrite.

It should become an `item`-centric shell with an ECS-lite proc core.

That means:

- BN world items stay as `item`
- recipes stay the entry point
- proc building uses normalized data rows + systems
- finalized results go back into `item`

## Shell vs core

### Shell

The shell is the existing BN side:

- `recipe`
- `item`
- `itype`
- `contents`
- save/load
- current gunmod/toolmod/magazine logic

### Core

The core is the new proc pipeline:

- `part_fact`
- `pick`
- `fast_blob`
- `full_blob`
- `hist`
- small systems that transform those values

## Why this shape

It keeps hot work cheap without trying to retrofit the whole game into entities/components/systems.

The proc core borrows the useful parts of ECS:

- flat facts
- stable ids
- system passes
- no deep object graph walking on the hot path

## Core rows

The builder should normalize eligible items into compact facts.

Example fact:

```json
{
  "ix": 17,
  "id": "bread_sourdough",
  "tag": ["bread"],
  "mat": ["wheat"],
  "mass": 90,
  "dens": 0.24,
  "kcal": 250,
  "alg": ["wheat"],
  "hp": 1.0,
  "chg": 1,
  "proc": null
}
```

The builder should work on fact indexes, not copied live items, until finalize.

## Core state

Minimal builder state:

- `cand[slot] -> [fact_ix...]`
- `pick[slot] -> [fact_ix...]`
- `sum` fast accumulator
- `rev` inventory revision
- `mode` history mode

This is the ECS-lite heart of the feature.

## Systems

Recommended systems:

1. `scan` - inventory to fact rows
2. `match` - facts to slots
3. `pick` - add/remove chosen facts
4. `fast` - update preview blob
5. `full` - cold full fold
6. `make` - build result spec / item
7. `save` - normalize proc payload
8. `stack` - derive stack key
9. `unc` - restore disassembly outputs

## Hot and cold split

Hot systems:

- `scan`
- `match`
- `pick`
- `fast`
- `stack`

Cold systems:

- `full`
- `make`
- `unc`

Rule:

- hot systems stay typed and local in C++
- cold systems may delegate most schema-specific work to Lua

## Normalization boundary

Lua should never be handed arbitrary live UI state and then become the source of truth.

Instead:

- C++ builds normalized fact tables
- Lua reads normalized facts and picks
- Lua returns normalized outputs
- C++ validates and stores typed results

This keeps Lua flexible without making saves, stacking, or tests fuzzy.
