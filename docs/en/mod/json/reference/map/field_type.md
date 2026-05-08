# Field Types

```json
{
  "type": "field_type", // this is a field type
  "id": "fd_gum_web", // id of the field
  "immune_mtypes": ["mon_spider_gum"], // list of monster immune to this field
  "intensity_levels": [
    {
      "name": "shadow", // name of this level of intensity
      "light_override": 3.7
    }
    //light level on the tile occupied by this field will be set at 3.7 not matter the ambient light.
  ],
  "bash": {
    "str_min": 1, // lower bracket of bashing damage required to bash
    "str_max": 3, // higher bracket
    "sound_vol": 2, // noise made when succesfully bashing the field
    "sound_fail_vol": 2, // noise made when failing to bash the field
    "sound": "shwip", // sound on success
    "sound_fail": "shwomp", // sound on failure
    "msg_success": "You brush the gum web aside.", // message on success
    "move_cost": 120, // how many moves it costs to succesfully bash that field (default: 100)
    "items": [ // item dropped upon succesful bashing
      { "item": "2x4", "count": [5, 8] },
      { "item": "nail", "charges": [6, 8] },
      {
        "item": "splinter",
        "count": [3, 6]
      },
      { "item": "rag", "count": [40, 55] },
      { "item": "scrap", "count": [10, 20] }
    ]
  }
}
```
