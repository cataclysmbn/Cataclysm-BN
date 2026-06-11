---
title: Nested Recipe Categories
---

### Nested Recipe Categories

Nested categories let you create hierarchical groupings inside the crafting UI.
They are JSON objects with `type: "nested_category"` that behave like recipes for
listing and filtering, but they do not produce items. A nested category appears
as a collapsible entry and expands into the recipes (or other nested categories)
listed in its `nested_category_data`.

Use nested categories when a subcategory has too many entries or you want an
extra grouping layer without adding new craft tabs.

### Fields

| Identifier           | Description                                                                                    |
| -------------------- | ---------------------------------------------------------------------------------------------- |
| id                   | Unique ID for the nested category.                                                             |
| type                 | Must be `nested_category`.                                                                     |
| nested_name          | Display name shown in the crafting UI. Recommended because nested categories have no `result`. |
| category             | Crafting category (`CC_*`) where the entry appears.                                            |
| subcategory          | Crafting subcategory where the entry appears.                                                  |
| description          | Optional description shown in the crafting UI.                                                 |
| nested_category_data | Array of recipe IDs or nested category IDs to display under this entry.                        |

### Behavior Notes

- `nested_category_data` can reference standard recipes (by recipe ID) or other
  nested categories (by `id`). Recipe IDs are the recipe's own ID, which is the
  result item ID with any `id_suffix` applied.
- Nested categories should not define `result`, components, tools, or time. They
  are UI-only entries and cannot be crafted.
- Avoid circular references between nested categories. Cycles are ignored in
  the UI and can hide entries.

### Example

```json
[
  {
    "type": "nested_category",
    "id": "hot_drinks",
    "nested_name": "hot drinks",
    "category": "CC_FOOD",
    "subcategory": "CSC_FOOD_DRINKS",
    "description": "Warm beverages and infusions.",
    "nested_category_data": [
      "coffee",
      "tea",
      "herbal_tea"
    ]
  }
]
```
