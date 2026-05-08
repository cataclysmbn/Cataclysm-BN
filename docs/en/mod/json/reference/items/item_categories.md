---
title: Item Categories
---

### Item Category

When you sort your inventory by category, these are the categories that are displayed.

| Identifier     | Description                                                                                                                                                                                                                                                                                                                                               |
| -------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| id             | Unique ID. Must be one continuous word, use underscores if necessary                                                                                                                                                                                                                                                                                      |
| name           | The name of the category. This is what shows up in-game when you open the inventory.                                                                                                                                                                                                                                                                      |
| zone           | The corresponding loot_zone (see loot_zones.json)                                                                                                                                                                                                                                                                                                         |
| sort_rank      | Used to sort categories when displaying. Lower values are shown first                                                                                                                                                                                                                                                                                     |
| priority_zones | When set, items in this category will be sorted to the priority zone if the conditions are met. If the user does not have the priority zone in the zone manager, the items get sorted into zone set in the 'zone' property. It is a list of objects. Each object has 3 properties: ID: The id of a LOOT_ZONE (see LOOT_ZONES.json), flags: array of flags |

```json
{
  "id": "armor",
  "name": "ARMOR",
  "zone": "LOOT_ARMOR",
  "sort_rank": -21,
  "priority_zones": [{ "id": "LOOT_FARMOR", "flags": ["RAINPROOF"] }]
}
```
