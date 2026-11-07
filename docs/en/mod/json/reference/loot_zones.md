---
title: Loot zones
---

The manager combines selected storage categories into one `LOOT_CUSTOM` zone.
Area rules create separate native zones. Manual text filters remain a separate
option. Selecting only Corpses, Wood or Seeds uses the native storage type.

## Groups

`data/json/loot_zone_groups.json` defines `LOOT_ZONE_GROUP` objects:

- `id`: required, stable group ID.
- `name`: required, translatable title.
- `kind`: `storage` (default), `activity`, or `rules`. Sections appear in this order.
- `sort_order`: order within the section, default 0. Ties use the group ID.
- `categories`: ordered zone type IDs, default empty. Missing types are skipped;
  duplicates appear only in their first group.

The standard loader supports abstracts, `copy-from`, `extend` and `delete`.
For example, a mod can add a member to the existing tools group:

```json
{
  "type": "LOOT_ZONE_GROUP",
  "id": "tools",
  "copy-from": "tools",
  "extend": { "categories": ["MY_MOD_TOOL_STORAGE"] }
}
```

Ungrouped storage types remain selectable, ordered by ID. They are discovered
from `LOOT_` IDs and item-category zone mappings. Adding a group does not define
sorting rules: each item category still needs its destination zone mapping.

## Sorting and saves

Sorting prefers manual filters, available specialized storage, narrower category
selections, then dump zones. Existing transfer and capacity rules still apply.

Category zones save a `categories` array of stable zone IDs. Existing custom zones
without this field retain their text filters. Category zones also save
`mark: "-"` so older versions match nothing. Saving them in an older version
loses the category selection; it must be recreated after upgrading.

## Settings

- **Use zone manager**: enabled by default. Disabling it restores the old picker
  without changing saved zones or sorting.
- **Zone category display limit**: defaults to 8; 0 shows all. Extra categories
  appear as "and others". This changes only the details panel.
