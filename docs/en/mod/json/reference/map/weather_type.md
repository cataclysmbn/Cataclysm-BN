# Weather Types

Weather type specifies conditions under which it can occur (temperature, humidity, pressure,
windpower, time of day, etc.) and what effects it causes on the game world and reality bubble.

When selecting weather type, the game goes over the list defined in region settings and selects the
last entry that is considered eligible under current conditions. If none of the entries are
eligible, invalid weather type `"none"` will be used.

### Fields

| Identifier        | Description                                                                                                                                  |
| ----------------- | -------------------------------------------------------------------------------------------------------------------------------------------- |
| id                | (_mandatory_) Unique ID. Must be one continuous word, use underscores if necessary.                                                          |
| name              | (_mandatory_) In-game name displayed.                                                                                                        |
| color             | (_mandatory_) Color of in-game name.                                                                                                         |
| glyph             | (_mandatory_) Glyph used on overmap.                                                                                                         |
| map_color         | (_mandatory_) Color of overmap glyph.                                                                                                        |
| ranged_penalty    | (_mandatory_) Penalty to ranged attacks.                                                                                                     |
| sight_penalty     | (_mandatory_) Sight penalty, aka multiplier to tile transparency.                                                                            |
| light_modifier    | (_mandatory_) Flat bonus to ambient light.                                                                                                   |
| sound_attn        | (_mandatory_) Sound attenuation (flat reduction to volume).                                                                                  |
| dangerous         | (_mandatory_) If true, prompts for activity interrupt.                                                                                       |
| precip            | (_mandatory_) Amount of associated precipitation. Valid values are: `none`, `very_light`, `light` and `heavy`.                               |
| rains             | (_mandatory_) Whether said precipitation falls as rain.                                                                                      |
| acidic            | (_optional_) Whether said precipitation is acidic.                                                                                           |
| sound_category    | (_optional_) Sound effects to play. Valid values are: `silent`, `drizzle`, `rainy`, `thunder`, `flurries`, `snowstorm` and `snow`.           |
| sun_intensity     | (_mandatory_) Sunlight intensity. Valid values are: `none`, `light`, `normal`, and `high`. Normal and high are considered "direct sunlight". |
| weather_animation | (_optional_) Weather animation in reality bubble. [Details](#weather_animation)                                                              |
| effects           | (_optional_) `[string, int]` pair array for the effects the weather causes. [Details](#effects)                                              |
| requirements      | (_optional_) Conditions under which this weather type will be eligible to be selected. [Details](#requirements)                              |

### weather_animation

All members are mandatory.

| Identifier | Description                                             |
| ---------- | ------------------------------------------------------- |
| factor     | Display density: 0 is none, 1 will blot out the screen. |
| glyph      | Glyph to use in ASCII mode.                             |
| color      | Glyph color.                                            |
| tile       | Graphical tile to use in TILES mode.                    |

### effects

`int` here is effect intensity.

| Identifier | Description                                                                         |
| ---------- | ----------------------------------------------------------------------------------- |
| wet        | wets player by `int` amount                                                         |
| thunder    | thunder sound with chance 1 in `int`                                                |
| lightning  | 1 in `int` chance of sound plus message and possible super charging electric fields |
| light_acid | causes pain unless waterproof                                                       |
| acid_rain  | causes more pain unless waterproof                                                  |
| morale     | causes player to feel a morale effect                                               |
| effect     | causes player to become afflicted with a status effect                              |

Example morale:

```json
{
  "name": "morale",
  "intensity": 3, // this effect will be applied every X turns.
  "bonus": 2, // the bonus the morale provides.
  "bonus_max": 60, // the maximum amount the morale can effect the player.
  "duration": "180 s", // amount of time the morale will last.
  "decay_start": "60 s", // amount of time before the effect starts counting down the duration.
  "morale_id_str": "morale_weather_rainbow", // id of the morale applied to the player.
  "morale_msg": "You stare in awe at the rainbow.", // message to display in chat when the player is afflicted.
  "morale_msg_frequency": 8, // chance to display this message every time the player is afflicted.
  "message_type": 0 // type of message to display: good, bad, mixed, etc.
}
```

Example effect:

```json
{
  "name": "effect",
  "intensity": 3, // this effect will be applied every X turns.
  "duration": "30 s", // amount of time the effect will last.
  "effect_id_str": "emp",
  "effect_intensity": 0, // intensity of the effect applied.
  "precipitation_name": "brain waves", // name of precipitation to display when "The <PRECIPITATION> is blocked by your umbrella!" type messages display.
  "ignore_armor": true, // ignores all protection.
  "bodypart_string": "head", // bodypart to apply the effect on.
  "effect_msg": "You feel an odd wave-like sensation pass through your head.", // message to display in chat when the player is afflicted
  "effect_msg_frequency": 16, // chance to display this message every time the player is afflicted.
  "effect_msg_blocked_frequency": 32, // chance to display this message every time the player blocks the effect with clothing.
  "message_type": 2, // type of message to display: good, bad, mixed, etc.
  "clothing_protection": 0, // one in X chance to block precipitation.
  "umbrella_protection": 0 // one in X chance to block precipitation.
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
