# Mutation overlay ordering

The file `mutation_ordering.json` defines the order that visual mutation and bionic overlays are
rendered on a character ingame. The layering value from 0 (bottom) - 9999 (top) sets the order.

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

The internal ID of the mutation. Can be provided as a single string, or an array of strings. The
order value provided will be applied to all items in the array.

## `order`

(integer)

The ordering value of the mutation overlay. Values range from 0 - 9999, 9999 being the topmost drawn
layer. Mutations that are not in any list will default to 9999.
