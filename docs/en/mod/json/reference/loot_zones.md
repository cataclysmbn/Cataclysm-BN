---
title: Loot zones
---

`LOOT_ZONE` defines a zone type. Its translatable `name` is used in the picker and
as the suggested instance name; saved instance names remain editable literal text.

## Category storage and saves

The checklist combines storage categories into one `LOOT_CUSTOM` zone. Area rules
and compatible activities create separate native zones on the same rectangle.
Functions requiring setup, such as farming and construction, open individually.
Manual text filters remain a separate option using the existing filter syntax.

Category storage saves a `categories` array of stable zone IDs. Manual filters
save `mark` instead. Existing custom zones without `categories` retain their text
filter behavior. Category zones also save `mark: "-"` so older versions match
nothing rather than mis-sort items. Saving in an older version loses the category
selection; recreate it after upgrading.

Selecting only Corpses, Wood or Seeds retains the native type. Mixed storage also
supports the corresponding butchery, plank-cutting and seed-selection activities.
Sorting prefers explicit text filters, available specialized storage, narrower
category selections, then dump zones. Existing transfer and capacity rules apply.

## Checklist groups

`data/json/loot_zone_groups.json` contains `LOOT_ZONE_GROUP` definitions:

```json
{
  "type": "LOOT_ZONE_GROUP",
  "id": "tools",
  "name": "Tools",
  "kind": "storage",
  "sort_order": 20,
  "categories": ["LOOT_WORKSHOP_TOOLS", "LOOT_ENTRY_TOOLS", "LOOT_TOOLS"]
}
```

- `id`: stable group ID.
- `name`: required translatable title; accepts a string or translation object.
- `kind`: `storage` (default), `activity`, or `rules`, displayed in that order.
- `sort_order`: order within the section, default 0; ties use the group ID.
- `categories`: ordered zone type IDs, default empty. Unavailable types are skipped
  and duplicate members appear only in their first group.

Groups use the standard generic-factory loader, including abstracts, `copy-from`,
`extend` and `delete`. To modify a group, inherit from it explicitly:

```json
{
  "type": "LOOT_ZONE_GROUP",
  "id": "tools",
  "copy-from": "tools",
  "extend": { "categories": ["MY_MOD_TOOL_STORAGE"] }
}
```

Ungrouped storage types remain selectable, ordered by stable ID. Storage types
are discovered from `LOOT_` IDs, item-category zone mappings and configured storage
groups. New item categories still need their usual destination zone mapping.
Other native types appear under "Other zone types".

## Interface options

**Use zone manager** is enabled by default. Disabling it restores the standard
picker without changing saved zones or sorting. **Zone category display limit**
defaults to 8; 0 displays all. Additional categories are summarized as "and others".
This limit affects only the details panel.
