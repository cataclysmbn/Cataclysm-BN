# Mapgen Color Palette

```jsonc
{
  "type": "mapgen_color_palette", // Mandatory type
  "id": "plaster_wall_palette", // Id of the palette
  "colors": [
    "Emerald Green", // Named Colors
    "Purple", // Fuzzy Matched Named Colors
    "Carmine Red", // With a default weight of 100
    {
      "color": "#ffffff", // Can also be hex codes
      "weight": 600, // With a specified weight
    },
  ],
}
```
