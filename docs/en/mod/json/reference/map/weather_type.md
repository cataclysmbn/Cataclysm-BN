# Weather Types

Weather type specifies conditions under which it can occur (temperature, humidity, pressure,
windpower, time of day, etc.) and what effects it causes on the game world and reality bubble.

When selecting weather type, the game goes over the list defined in region settings and selects the
last entry that is considered eligible under current conditions. If none of the entries are
eligible, invalid weather type `"none"` will be used.

### Fields

| Identifier     | Description                                                                                                                                  |
| -------------- | -------------------------------------------------------------------------------------------------------------------------------------------- |
| id             | (_mandatory_) Unique ID. Must be one continuous word, use underscores if necessary.                                                          |
| name           | (_mandatory_) In-game name displayed.                                                                                                        |
| glyph          | (_mandatory_) Glyph used on overmap.                                                                                                         |
| color          | (_optional_) Color of in-game name.                                                                                                          |
| map_color      | (_optional_) Color of overmap glyph.                                                                                                         |
| ranged_penalty | (_mandatory_) Penalty to ranged attacks.                                                                                                     |
| sight_penalty  | (_mandatory_) Sight penalty, aka multiplier to tile transparency.                                                                            |
| light_modifier | (_mandatory_) Flat bonus to ambient light.                                                                                                   |
| sound_attn     | (_mandatory_) Sound attenuation (flat reduction to volume).                                                                                  |
| dangerous      | (_mandatory_) If true, prompts for activity interrupt.                                                                                       |
| precip         | (_mandatory_) Amount of associated precipitation. Valid values are: `none`, `very_light`, `light` and `heavy`.                               |
| rains          | (_mandatory_) Whether said precipitation falls as rain.                                                                                      |
| acidic         | (_optional_) Whether said precipitation is acidic.                                                                                           |
| sound_category | (_optional_) Sound effects to play. Valid values are: `silent`, `drizzle`, `rainy`, `thunder`, `flurries`, `snowstorm` and `snow`.           |
| sun_intensity  | (_mandatory_) Sunlight intensity. Valid values are: `none`, `light`, `normal`, and `high`. Normal and high are considered "direct sunlight". |
| animation      | (_optional_) Weather animation in reality bubble. [Details](#weather_animation)                                                              |
| effects        | (_optional_) `[string, int]` pair array for the effects the weather causes. [Details](#effects)                                              |
| requirements   | (_optional_) Conditions under which this weather type will be eligible to be selected. [Details](#requirements)                              |

### animation

All members are mandatory.

| Identifier | Description                                             |
| ---------- | ------------------------------------------------------- |
| factor     | Display density: 0 is none, 1 will blot out the screen. |
| glyph      | Glyph to use in ASCII mode.                             |
| color      | Glyph color.                                            |
| tile       | Graphical tile to use in TILES mode.                    |

### effects

#### Universal fields

| Identifier | Description                                                                                |
| ---------- | ------------------------------------------------------------------------------------------ |
| name       | (_mandatory_) One of the below listed effects, completely changes the json loaded for some |
| intensity  | (_mandatory_) Intensity of the effect to give                                              |

##### Names

- `morale`: Gives player morale, requires additional defined fields
- `effect`: Gives playerrs an effect, requires additional defined fields
- `wet`: Causes players to become wet, amount is intensity
- `thunder`: Thunder is caused with a one in intensity chance
- `lightning`: Lighning is caused with a one in intensity chance

#### Morale

| Identifier           | Description                                                                                                     |
| -------------------- | --------------------------------------------------------------------------------------------------------------- |
| intensity            | (_mandatory_) This effect is applied every x turns                                                              |
| bonus                | (_mandatory_) Amount of morale each stack gives                                                                 |
| max_bonus            | (_mandatory_) Max amount of morale gained                                                                       |
| duration             | (_mandatory_) Duration of the morale effect                                                                     |
| decay_start          | (_mandatory_) How long until the effect starts to lower in intensity, total duration is this and duration added |
| morale_id_str        | (_mandatory_) Id of the morale effect applied                                                                   |
| morale_msg           | (_mandatory_) Message to give when gaining the morale                                                           |
| morale_msg_frequency | (_mandatory_) Percent chance to display the message on gaining effect                                           |
| message_type         | (_mandatory_) Type of message; 0 == Good, 1 == Bad, 2 == Mixed, 3 == Warning, 4 == Info, 5 == Neutral           |

##### Example:

```json
{
  "name": "morale",
  "intensity": 3,
  "bonus": 2,
  "bonus_max": 60,
  "duration": "180 s",
  "decay_start": "60 s",
  "morale_id_str": "morale_weather_rainbow",
  "morale_msg": "You stare in awe at the rainbow.",
  "morale_msg_frequency": 8,
  "message_type": 0
}
```

#### Effect

| Identifier                   | Description                                                                                                                                                    |
| ---------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| intensity                    | (_mandatory_) This effect will be applied every x turns                                                                                                        |
| duration                     | (_mandatory_) How long the effect lasts                                                                                                                        |
| effect_id_str                | (_mandatory_) String of the effect id to give                                                                                                                  |
| effect_intensity             | (_mandatory_) Intensity of the effect to give                                                                                                                  |
| bodypart_string              | (_optional_) Bodypart to apply the effect to, default whole body                                                                                               |
| effect_msg                   | (_mandatory_) Message to give when gaining the effect                                                                                                          |
| effect_msg_frequency         | (_mandatory_) Percent chance to display the message on gaining effect                                                                                          |
| effect_msg_blocked_frequency | (_mandatory_) Percent chance to display the protection message on preventing effect                                                                            |
| message_type                 | (_mandatory_) Type of message; 0 == Good, 1 == Bad, 2 == Mixed, 3 == Warning, 4 == Info, 5 == Neutral                                                          |
| precipitation_name           | (_mandatory_) Name to display in string "The <field> is blocked by your umbrella!" messages appear                                                             |
| protection_data              | (_mandatory_) Array of `check` flags or traits ( along with `DEFAULT` which is always applied ) and `odds` ( one in x chance ) to be protected from the effect |

##### Example:

```json
{
  "name": "effect",
  "intensity": 3,
  "duration": "30 s",
  "effect_id_str": "emp",
  "effect_intensity": 0,
  "precipitation_name": "brain waves",
  "bodypart_string": "head",
  "effect_msg": "You feel an odd wave-like sensation pass through your head.",
  "effect_msg_frequency": 16,
  "effect_msg_blocked_frequency": 32,
  "message_type": 2,
  "protection_data": [
    { "check": "RAIN_PROTECT", "odds": 1 },
    { "check": "ACIDPROOF", "odds": 1 },
    { "check": "DEFAULT", "odds": 2 }
  ]
}
```

### requirements

All members are optional.

| Identifier            | Description                                                                                                                                                                                                                                       |
| --------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| pressure_min          | Min pressure                                                                                                                                                                                                                                      |
| pressure_max          | Max pressure                                                                                                                                                                                                                                      |
| humidity_min          | Min humidity                                                                                                                                                                                                                                      |
| humidity_max          | Max humidity                                                                                                                                                                                                                                      |
| temperature_min       | Min temperature                                                                                                                                                                                                                                   |
| temperature_max       | Max temperature                                                                                                                                                                                                                                   |
| windpower_min         | Min windpower                                                                                                                                                                                                                                     |
| windpower_max         | Max windpower                                                                                                                                                                                                                                     |
| humidity_and_pressure | If there are pressure and humidity requirements are they both required or just one                                                                                                                                                                |
| acidic                | Does this require acidic precipitation                                                                                                                                                                                                            |
| time                  | Time of day. Valid values are: day, night, and both.                                                                                                                                                                                              |
| required_weathers     | Will only be selected if conditions match for any of the specified types, i.e. rain can only happen if the conditions for clouds, light drizzle or drizzle are present. Required weathers should be "before" this one in the region weather list. |

### Example

```json
{
  "id": "lightning",
  "type": "weather_type",
  "name": "Lightning Storm",
  "color": "c_yellow",
  "map_color": "h_yellow",
  "glyph": "%",
  "ranged_penalty": 4,
  "sight_penalty": 1.25,
  "light_modifier": -45,
  "sound_attn": 8,
  "dangerous": false,
  "precip": "heavy",
  "rains": true,
  "acidic": false,
  "effects": [{ "name": "thunder", "intensity": 50 }, { "name": "lightning", "intensity": 600 }],
  "tiles_animation": "weather_rain_drop",
  "weather_animation": { "factor": 0.04, "color": "c_light_blue", "glyph": "," },
  "sound_category": "thunder",
  "sun_intensity": "none",
  "requirements": { "pressure_max": 990, "required_weathers": ["thunder"] }
}
```
