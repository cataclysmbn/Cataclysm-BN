# Hunting data

Hunting data definitions specify the types of prey that can be attracted by different types of bait in various habitats. This data is used by the hunting mechanics to determine what creatures may be caught when using snares.<br><br>
Here is an example of a hunting data definition in JSON format:

```json
{
  "type": "hunting_data",
  "id": "default_hunting",
  "used_for": ["f_snare"], // List of "furniture types" that use this hunting data.
  "habitats": { // Habitat-specific prey lists. For now, 3 options are supported: "forest", "field", and "swamp".
    "forest": {
      // Each habitat contains bait types as keys (e.g., "SMALL_GAME", "LARGE_GAME"), each mapping to a list of prey entries.
      "SMALL_GAME": [
        { "prey": "mon_squirrel", "weight": 100 },
        { "prey": "mon_rabbit", "weight": 80 },
        { "prey": "mon_chipmunk", "weight": 60 }
      ],
      "LARGE_GAME": [{ "prey": "mon_deer", "weight": 40 }, { "prey": "mon_fox_red", "weight": 30 }]
    },
    "field": {
      "SMALL_GAME": [
        { "prey": "mon_rabbit", "weight": 120 },
        { "prey": "mon_groundhog", "weight": 80 }
      ],
      "LARGE_GAME": [{ "prey": "mon_deer", "weight": 50 }]
    },
    "swamp": {
      "SMALL_GAME": [
        { "prey": "mon_muskrat", "weight": 100 },
        { "prey": "mon_otter", "weight": 60 }
      ],
      "LARGE_GAME": [{ "prey": "mon_beaver", "weight": 70 }]
    }
  }
}
```

`used_for`: This is a list of furniture types (e.g., "f_snare") that utilize this hunting data definition. Be careful, the id should be partial(`"f_snare"`), not whole one(`"f_snare_empty"`). You can assign the same hunting data to multiple snares if needed, but snares **cannot** have two different hunting data assigned to them.<br><br>

`habitats`: This section defines different habitat types (e.g., "forest", "field", "swamp"). Each habitat contains bait types as keys (e.g., "SMALL_GAME", "LARGE_GAME"), each mapping to a list of prey entries.<br>
3x3 overmap tiles are scanned around the snare to calculate the weightings for each habitat type. The below habitat types are supported:

- `forest`: This is weighed by the overmap terrain including "forest" in its id.
- `field`: This is weighed by the overmap terrain including "field" or "farm" in its id.
- `swamp`: This is weighed by the overmap terrain including "swamp" or "bog" in its id.

## What is "bait type"?

Bait types are the flags assigned to bait items that determine which prey lists are used when hunting. For example, a "SMALL_GAME" bait type will attract small game animals like squirrels and rabbits, while a "LARGE_GAME" bait type will attract larger animals like deer and foxes.<br><br>
If you wish new bait types, like "FISH" or "BIRDS", you can add them by adding "BAIT_FISH" or "BAIT_BIRDS" flags in `flags.json`. And then adding the corresponding prey lists in the hunting data definitions.

## Hunting mechanics

First of all, you need to build a snare from construction menu(`*`). The snare requires wire or cable and a stick to construct. Once built, you can interact with the empty snare to set bait. The bait item must have a bait type flag that matches one of the bait types defined in the hunting data for the snare's habitat.<br><br>
When bait is set, the snare will transform into being set, like `f_snare_set`. The snare will remain set for a random duration between 4 to 8 hours. during this time, it would be better not to be close to the snare, or it may scare away potential prey. In mechanics, the player's presence in same overmap tile as the snare reduces the chance of catching prey.<br><br>
After the bait duration expires, the snare will automatically transform into either closed state(`f_snare_closed`). If prey was caught, you can interact with the closed snare to retrieve the caught prey item. If no prey was caught, the snare will revert to empty state(`f_snare_empty`), allowing you to set new bait and try again.

## Adding new snares

To add a new type of snare, you need to create a new furniture definition in `furniture.json` for the empty, set, and closed states of the snare. Each one has same `examine` function `hunting_snare` to handle the snare interactions.<br><br>

Then, create a new hunting data definition in `hunting_data.json` that specifies the prey lists for different habitats. Finally, ensure that the snare construction recipe in `construction.json` references the correct furniture types and that the bait items have the appropriate bait type flags.

## Is it infinite meat?

No. If you keep using the snares in same location, the prey population will decrease over time, reducing the chances of catching anything. It's advisable to move your snares to different locations periodically to allow prey populations to recover.
