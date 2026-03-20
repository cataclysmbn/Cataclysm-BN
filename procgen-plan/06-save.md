# Save

## Proc payload

Every proc item gets one optional envelope.

```json
{
  "proc": {
    "id": "sandwich",
    "mode": "none",
    "fp": "sandwich:de2f3c20",
    "blob": { "...": "..." },
    "hist": { "...": "optional" }
  }
}
```

Use terse field names inside the saved payload too.

The saved payload must come from normalized typed data, never directly from ad-hoc Lua tables.

## `none`

Save:

- id
- mode
- fp
- blob

Do not save exact parts.

## `compact`

Save:

- id
- mode
- fp
- blob
- hist.parts[]

`parts[]` uses the universal summary shape from `02-schema.md`.

## `full`

Save exact child tree.

Prefer reusing BN item save format for child items rather than inventing another subtree format.

## Stack rules

Stack keys must include:

- type id
- proc id
- mode
- fp
- gameplay-relevant blob values
- freshness bucket if food rots

## Inspect rules

Info UI should prefer compiled blob values over rebuilding from children.

That is especially important for food.

For proc items, child data is provenance; compiled blob is gameplay truth.

## Uncraft rules

- `none` -> schema fallback only
- `compact` -> restore from summaries
- `full` -> restore exact state

## Subtask cut

1. item payload struct
2. save/load
3. stack key changes
4. inspect read path
5. uncraft read path
