# Digital media

## Recipe groups

Fixed groups list recipes and an optional skill level required to consult them:

```json
{
  "type": "digital_recipe_group",
  "id": "my_recipe_references",
  "recipes": [{ "id": "3d_printer_basic", "skill_level": 7 }]
}
```

Omitting `skill_level` uses the recipe's difficulty. A group can also select
random recipes, including recipes added by mods:

```json
{
  "type": "digital_recipe_group",
  "id": "my_cooking_recipes",
  "random_recipes": {
    "count": 1,
    "category": "CC_FOOD",
    "min_difficulty": 0,
    "max_difficulty": 3,
    "difficulty_chance_offset": 0
  }
}
```

`category` is optional. Difficulty defaults to an unrestricted nonnegative
range, and `count` defaults to one. An optional `candidates` array restricts the
pool to specific recipe IDs. Recipes marked `never_learn` are excluded unless
`include_unlearnable` is true. Random selections do not repeat fixed entries
or other selections from the same group.

When `difficulty_chance_offset` is present, each matching recipe has a
one-in-`max(1, difficulty + offset)` chance to enter the pool before selection.
Omit this field to include every matching candidate. The built-in cooking group uses food recipes of difficulty 0–3 with offset 0;
the scientific group uses difficulty 3 and above with offset 1, matching the
legacy random recipe filters.

`initialize_digital_recipe_file` resolves an item's `digital_recipe_group`
property once and stores the result in its `DIGITAL_RECIPES` variable.
`digital_file_recipes` reads that cached result. An empty result is retained
too, rather than rerolled on every access.
