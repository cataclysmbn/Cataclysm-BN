# Recipe

## Why the old shape is not enough

The current system is `AND of OR` on item alternatives.

That shape works for static recipes, but it is the wrong tool for proc builders.

Do not encode a sandwich as giant OR soup.

Instead:

- `recipe` stays the entry point
- `schema` owns selection

## Recipe changes

Add optional proc fields to `recipe`.

- `proc` bool
- `proc_id` schema id
- `builder_name`
- `builder_desc`

Static recipes stay unchanged.

## What `requirements` still does

For proc recipes, `requirements` should only gate entry.

Good uses:

- tools
- qualities
- heat source
- station
- mandatory base item presence

Bad uses:

- full selection space of all valid fillings or parts

## Search changes

Add schema-aware indexes.

- role index
- tag index
- result index
- category index

Examples:

- `c:bread` matches builders with a bread slot
- `c:meat` matches builders with a meat slot
- `q:cut` behaves as before

## Related recipes

For proc recipes, related view should prefer:

- sibling builders
- builders sharing roles
- recipes using the built result

Do not try to flatten every accepted part into the current related-recipe menu.

## Uncraft lookup

Priority order:

1. `full` exact state
2. `compact` part summaries
3. `none` fallback outputs from schema or static recipe

## Planned file touches

- `src/recipe.h`
- `src/recipe.cpp`
- `src/recipe_dictionary.cpp`
- `src/craft_command.cpp`

## Subtask cut

Keep this area in multiple tiny commits:

1. recipe data fields
2. schema load plumbing
3. search index
4. related-recipe handling
