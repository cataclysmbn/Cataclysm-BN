---
title: Monster Factions
---

:::note
This article was recently split off from `JSON INFO` and likely could use additional work
:::

### Monster Factions

| Identifier     | Description                                                                                                                      |
| -------------- | -------------------------------------------------------------------------------------------------------------------------------- |
| `name`         | Unique ID. Must be one continuous word, use underscores when necessary.                                                          |
| `base_faction` | Optional base faction. Relations to other factions are inherited from it and relations of other factions to this one check this. |
| `by_mood`      | Be hostile towards this faction when angry, neutral otherwise. Default attitude to all other factions.                           |
| `neutral`      | Always be neutral towards this faction.                                                                                          |
| `friendly`     | Always be friendly towards this faction. By default a faction is friendly towards itself.                                        |
| `hate`         | Always be hostile towards this faction. Will change target to monsters of this faction if available.                             |

```json
{
  "name": "cult",
  "base_faction": "zombie",
  "by_mood": ["blob"],
  "neutral": ["nether"],
  "friendly": ["blob"],
  "hate": ["fungus"]
}
```
