# Schema

## Shape

Static recipes stay.

Proc recipes gain a linked schema.

Core objects:

- `proc_schema`
- `proc_pick`
- `proc_blob`
- `proc_hist`

## Schema keys

Use a short, stable shape.

```json
{
  "id": "sandwich",
  "type": "PROC",
  "cat": "food",
  "res": "proc_sandwich",
  "hist": {
    "def": "none",
    "ok": ["none", "compact", "full"]
  },
  "slot": [],
  "drv": {},
  "unc": {},
  "lua": {}
}
```

## Slot keys

```json
{
  "id": "top",
  "role": "bread",
  "min": 1,
  "max": 1,
  "rep": false,
  "ok": ["tag:bread"],
  "no": ["flag:LIQUID"]
}
```

Suggested field meanings:

- `id` local slot id
- `role` semantic part role
- `min` / `max` count bounds
- `rep` repeat allowed
- `ok` accept queries
- `no` reject queries

## Query style

Allowed query atoms:

- `tag:bread`
- `tag:meat`
- `flag:LIQUID`
- `mat:wood`
- `itype:knife_butcher`
- `qual:CUT>=1`

Keep query atoms flat and cheap to test.

## Fact layer

Each candidate item should be normalized into a `fact` row before slot matching.

The schema matches against facts, not arbitrary live item methods, on the hot path.

Suggested fact shape:

```json
{
  "ix": 17,
  "id": "knife_butcher",
  "tag": ["knife", "blade"],
  "mat": ["steel"],
  "mass": 820,
  "dens": 7.8,
  "kcal": 0,
  "alg": [],
  "hp": 0.52,
  "chg": 0,
  "proc": null
}
```

Builder picks should store `ix` references until finalize.

## Derive block

Two layers:

- `fast` for hot preview
- `full` for finalize / inspect / save

```json
{
  "drv": {
    "fast": {
      "mass": "sum(parts.mass)",
      "kcal": "sum(parts.kcal)",
      "alg": "parts.alg | parts.flag",
      "dens": "wavg(parts.dens, parts.mass)"
    },
    "full": "lua:proc.food.full"
  }
}
```

Default rule:

- preview uses `fast`
- finalize uses `full`
- `fast` should stay fixed-op and data-local
- `full` may call Lua and do deeper folds

If a schema is simple enough, `full` may equal `fast`.

## Compact shape

`compact` must be universal enough for all kinds.

```json
{
  "role": "head",
  "id": "knife_butcher",
  "n": 1,
  "hp": 0.52,
  "dmg": 2,
  "chg": 0,
  "fault": [],
  "flag": [],
  "mat": "steel",
  "var": {},
  "proc": null
}
```

Notes:

- `proc` nests another compact summary if the child is also proc
- `hp` prevents durability reset exploits
- `var` is reserved for small schema-specific leftovers
- the shape stays universal even if some fields are unused in a given category

## Sandwich example

```json
{
  "id": "sandwich",
  "type": "PROC",
  "cat": "food",
  "res": "proc_sandwich",
  "hist": {
    "def": "none",
    "ok": ["none", "compact", "full"]
  },
  "slot": [
    { "id": "top", "role": "bread", "min": 1, "max": 1, "ok": ["tag:bread"] },
    { "id": "bot", "role": "bread", "min": 1, "max": 1, "ok": ["tag:bread"] },
    { "id": "veg", "role": "veg", "min": 0, "max": 8, "rep": true, "ok": ["tag:veg"] },
    { "id": "chs", "role": "cheese", "min": 0, "max": 4, "rep": true, "ok": ["tag:cheese"] },
    { "id": "sau", "role": "cond", "min": 0, "max": 4, "rep": true, "ok": ["tag:cond"] },
    { "id": "meat", "role": "meat", "min": 0, "max": 6, "rep": true, "ok": ["tag:meat"] }
  ],
  "drv": {
    "fast": {
      "mass": "sum(parts.mass)",
      "kcal": "sum(parts.kcal)",
      "alg": "parts.alg | parts.flag",
      "dens": "wavg(parts.dens, parts.mass)",
      "vol": "mass / max(dens, 1) * 0.92"
    },
    "full": "lua:proc.food.full"
  },
  "lua": {
    "name": "proc.food.name",
    "make": "proc.food.make"
  }
}
```

## Spear example

```json
{
  "id": "knife_spear",
  "type": "PROC",
  "cat": "melee",
  "res": "proc_melee",
  "hist": {
    "def": "compact",
    "ok": ["compact", "full"]
  },
  "slot": [
    { "id": "head", "role": "blade", "min": 1, "max": 1, "ok": ["tag:blade", "tag:knife"] },
    { "id": "shaft", "role": "shaft", "min": 1, "max": 1, "ok": ["tag:pole"] },
    { "id": "bind", "role": "bind", "min": 1, "max": 2, "ok": ["tag:bind"] }
  ],
  "drv": {
    "fast": {
      "mass": "sum(parts.mass)",
      "cut": "head.cut",
      "pierce": "head.pierce + shaft.flex * 0.2",
      "dur": "min(head.hp, shaft.hp, bind.hp * 0.8)"
    },
    "full": "lua:proc.melee.full"
  },
  "lua": {
    "make": "proc.melee.make"
  }
}
```
