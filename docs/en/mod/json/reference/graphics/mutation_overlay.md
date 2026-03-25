# Mutation overlay ordering

The file `mutation_ordering.json` defines the order that character overlays are rendered ingame.
It can reorder mutations, bionics, effects, worn items, and wielded items. The layering value from
0 (bottom) - 9999 (top) sets the order, though worn and wielded overlays use higher default values
unless overridden.

Example:

```json
[
  {
    "type": "overlay_order",
    "overlay_ordering": [
      {
        "id": [
          "BEAUTIFUL",
          "BEAUTIFUL2",
          "BEAUTIFUL3",
          "LARGE",
          "PRETTY",
          "RADIOACTIVE1",
          "RADIOACTIVE2",
          "RADIOACTIVE3",
          "REGEN"
        ],
        "order": 1000
      },
      {
        "id": ["HOOVES", "ROOTS1", "ROOTS2", "ROOTS3", "TALONS"],
        "order": 4500
      },
      {
        "id": "worn_backpack",
        "order": 5400
      },
      {
        "id": "FLOWERS",
        "order": 5000
      },
      {
        "id": [
          "PROF_CYBERCOP",
          "PROF_FED",
          "PROF_PD_DET",
          "PROF_POLICE",
          "PROF_SWAT",
          "PHEROMONE_INSECT"
        ],
        "order": 8500
      },
      {
        "id": [
          "bio_armor_arms",
          "bio_armor_legs",
          "bio_armor_torso",
          "bio_armor_head",
          "bio_armor_eyes"
        ],
        "order": 500
      }
    ]
  }
]
```

## `id`

(string)

The overlay ID. Can be provided as a single string, or an array of strings. The order value
provided will be applied to all items in the array.

Legacy mutation and bionic IDs such as `ELFA_EARS` and `bio_armor_head` still work. To reorder
other overlays, use the full overlay ID such as `worn_backpack`, `wielded_katana`,
`mutation_ELFA_EARS`, or `effect_onfire`.

## `order`

(integer)

The ordering value of the mutation overlay. Values range from 0 - 9999, 9999 being the topmost drawn
layer. Mutations that are not in any list will default to 9999.
