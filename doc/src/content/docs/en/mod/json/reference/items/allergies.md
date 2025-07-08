---
title: allergies
---

## Basic Info

Allergies are based on the vitamins system for the purpose of inheritability and avoiding a need to
create several variants of items / make tough calls as to the material of foods.

As an initial measure, these vitamins were applied based on materials to prevent regression and
having to add the vitamins all by hand. However, unless an item has `NUTRIENT_OVERRIDE`, cooking
_should_ correctly reflect what actually went into it. Naturally spawned items will have whatever
their materials would indicate.

If a food item has veggy allergens in it, a carnivore will be unable to eat it. Vice versa for an
herbivore and something with meat allergens.

## Current 'allergens':

- `"egg_allergen"`
- `"fruit_allergen"`
- `"human_flesh_vitamin"` (This one is a special case, as it's instead used for morale maluses /
  boons due to Cannibalism)
- `"junk_allergen"`
- `"meat_allergen"`
- `"milk_allergen"`
- `"nut_allergen"`
- `"veggy_allergen"`
- `"wheat_allergen"` (Actually just general bread / baked goods)

## Current dietary restrictions:

Full bans:

- Carnivore: veggy_allergen, fruit_allergen, wheat_allergen, nut_allergen
- Herbivore / Ruminant: meat_allergen, egg_allergen Dislikes:
- Vegetarian
- Anti-plant
- Lactose intolerance
- Fruit intolerance
- Junkfood intolerance
- Grain intolerance
