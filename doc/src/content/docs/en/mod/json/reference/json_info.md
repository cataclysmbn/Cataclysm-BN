---
title: JSON INFO
---

:::note

This document is being split into multiple pages.

:::

:::danger

Many of the JSON files are not documented yet or is outdated. Check relevent source files to be
sure.

:::

This document describes the contents of the json files used in Cataclysm: Dark days ahead. You are
probably reading this if you want to add or change content of Catacysm: Dark days ahead and need to
learn more about what to find where and what each file and property does.

## Navigating the JSON

A lot of the JSON involves cross-references to other JSON entities. To make it easier to navigate,
we provide a script `tools/json_tools/cddatags.py` that can build a `tags` file for you.

To run the script you'll need Python 3. On Windows you'll probably need to install that, and
associate `.py` files with Python. Then open a command prompt, navigate to your CDDA folder, and run
`tools\json_tools\cddatags.py`.

To use this feature your editor will need [ctags support](http://ctags.sourceforge.net/). When
that's working you should be able to easily jump to the definition of any entity. For example, by
positioning your cursor over an id and hitting the appropriate key combination.

- In Vim, this feature exists by default, and you can jump to a definition using
  [`^\]`](http://vimdoc.sourceforge.net/htmldoc/tagsrch.html#tagsrch.txt).
- In Notepad++ go to "Plugins" -> "Plugins Admin" and enable the "TagLEET" plugin. Then select any
  id and press Alt+Space to open the references window.

## The `type` Property

Entries are distinguished by their `type` property. This property is mandatory for all entries.
Setting this entry to 'armor' for example means the game will expect properties specific to armor in
that entry. Also ties in with [`copy-from`](./items/json_inheritance.md#copy-from), if you want to
[inherit properties of another object](./items/json_inheritance.md), it must be of the same tipe.

## Formatting

When editing JSON files make sure you apply the correct formatting as shown below.

### Time duration

A string containing one or more pairs of number and time duration unit. Number and unit, as well as
each pair, can be separated by an arbitrary amount of spaces. Available units:

- "hours", "hour", "h" - one hour
- "days", "day", "d" - one day
- "minutes", "minute", "m" - one minute
- "turns", "turn", "t" - one turn,

Examples:

- " +1 day -23 hours 50m " `(1*24*60 - 23*60 + 50 == 110 minutes)`
- "1 turn 1 minutes 9 turns" (1 minute and 10 seconds because 1 turn is 1 second)

### Other formatting

```json
"//" : "comment", // Preferred method of leaving comments inside json files.
```

Some json strings are extracted for translation, for example item names, descriptions, etc. The
exact extraction is handled in `lang/extract_json_strings.py`. Apart from the obvious way of writing
a string without translation context, the string can also have an optional translation context (and
sometimes a plural form), by writing it like:

```json
"name": { "ctxt": "foo", "str": "bar", "str_pl": "baz" }
```

or, if the plural form is the same as the singular form:

```json
"name": { "ctxt": "foo", "str_sp": "foo" }
```

You can also add comments for translators by adding a "//~" entry like below. The order of the
entries does not matter.

```json
"name": {
    "//~": "as in 'foobar'",
    "str": "bar"
}
```

[Currently, only some JSON values support this syntax](../../../i18n/reference/translation#supported-json-values).

## Description and content of each JSON file

This section describes each json file and their contents. Each json has their own unique properties
that are not shared with other Json files (for example 'chapters' property used in books does not
apply to armor). This will make sure properties are only described and used within the context of
the appropriate JSON file.

## `data/json/` JSONs

### Ascii_arts

| Identifier | Description                                                                                    |
| ---------- | ---------------------------------------------------------------------------------------------- |
| id         | Unique ID. Must be one continuous word, use underscores if necessary.                          |
| picture    | Array of string, each entry is a line of an ascii picture and must be at most 42 columns long. |

```json
{
  "type": "ascii_art",
  "id": "cashcard",
  "picture": [
    "",
    "",
    "",
    "       <color_white>╔═══════════════════╗",
    "       <color_white>║                   ║",
    "       <color_white>║</color> <color_yellow>╔═   ╔═╔═╗╔═║ ║</color>   <color_white>║",
    "       <color_white>║</color> <color_yellow>║═ ┼ ║ ║═║╚╗║═║</color>   <color_white>║",
    "       <color_white>║</color> <color_yellow>╚═   ╚═║ ║═╝║ ║</color>   <color_white>║",
    "       <color_white>║                   ║",
    "       <color_white>║   RIVTECH TRUST   ║",
    "       <color_white>║                   ║",
    "       <color_white>║                   ║",
    "       <color_white>║ 555 993 55221 066 ║",
    "       <color_white>╚═══════════════════╝"
  ]
}
```

### Body_parts

| Identifier        | Description                                                                                                                                                                                                                                                                             |
| ----------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| id                | (_mandatory_) Unique ID. Must be one continuous word, use underscores if necessary.                                                                                                                                                                                                     |
| name              | (_mandatory_) In-game name displayed.                                                                                                                                                                                                                                                   |
| accusative        | (_mandatory_) Accusative form for this bodypart.                                                                                                                                                                                                                                        |
| heading           | (_mandatory_) How it's displayed in headings.                                                                                                                                                                                                                                           |
| heading_multiple  | (_optional_) Plural form of heading. (default: heading value)                                                                                                                                                                                                                           |
| hp_bar_ui_text    | (_optional_) How it's displayed next to the hp bar in the panel. (default: empty string)                                                                                                                                                                                                |
| encumbrance_text  | (_optional_) Description of effect when encumbered. (default: empty string)                                                                                                                                                                                                             |
| main_part         | (_optional_) What is the main part this one is attached to. (default: self)                                                                                                                                                                                                             |
| base_hp           | (_optional_) The amount of hp this part has before any modification. (default: `60`)                                                                                                                                                                                                    |
| opposite_part     | (_optional_) What is the opposite part ot this one in case of a pair. (default: self)                                                                                                                                                                                                   |
| essential         | (_optional_) Whether the character dies if this part drops to `0` HP.                                                                                                                                                                                                                   |
| hit_size          | (_optional_) Float. Size of the body part when doing an unweighted selection. (default: `0.`)                                                                                                                                                                                           |
| hit_size_relative | (_optional_) Float. Hit sizes for attackers who are smaller, equal in size, and bigger. (default: `[ 0, 0, 0 ]`                                                                                                                                                                         |
| hit_difficulty    | (_optional_) Float. How hard is it to hit a given body part, assuming "owner" is hit. Higher number means good hits will veer towards this part, lower means this part is unlikely to be hit by inaccurate attacks. Formula is `chance *= pow(hit_roll, hit_difficulty)` (default: `0`) |
| side              | (_optional_) Which side this body part is on. Default both.                                                                                                                                                                                                                             |
| stylish_bonus     | (_optional_) Mood bonus associated with wearing fancy clothing on this part. (default: `0`)                                                                                                                                                                                             |
| hot_morale_mod    | (_optional_) Mood effect of being too hot on this part. (default: `0`)                                                                                                                                                                                                                  |
| cold_morale_mod   | (_optional_) Mood effect of being too cold on this part. (default: `0`)                                                                                                                                                                                                                 |
| bionic_slots      | (_optional_) How many bionic slots does this part have.                                                                                                                                                                                                                                 |

```json
{
  "id": "torso",
  "type": "body_part",
  "name": "torso",
  "accusative": { "ctxt": "bodypart_accusative", "str": "torso" },
  "heading": "Torso",
  "heading_multiple": "Torso",
  "hp_bar_ui_text": "TORSO",
  "encumbrance_text": "Dodging and melee is hampered.",
  "main_part": "torso",
  "opposite_part": "torso",
  "hit_size": 45,
  "hit_size_relative": [20, 33.33, 36.57],
  "hit_difficulty": 1,
  "side": "both",
  "legacy_id": "TORSO",
  "stylish_bonus": 6,
  "hot_morale_mod": 2,
  "cold_morale_mod": 2,
  "base_hp": 60,
  "bionic_slots": 80
}
```

### Bionics

| Identifier                                                                                           | Description                                                                                                                                                                     |
| ---------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| id                                                                                                   | (_mandatory_) Unique ID. Must be one continuous word, use underscores if necessary.                                                                                             |
| name                                                                                                 | (_mandatory_) In-game name displayed.                                                                                                                                           |
| description                                                                                          | (_mandatory_) In-game description.                                                                                                                                              |
| flags                                                                                                | (_optional_) A list of flags. See JSON_FLAGS.md for supported values.                                                                                                           |
| act_cost                                                                                             | (_optional_) How many kJ it costs to activate the bionic. Strings can be used "1 kJ"/"1000 J"/"1000000 mJ" (default: `0`)                                                       |
| deact_cost                                                                                           | (_optional_) How many kJ it costs to deactivate the bionic. Strings can be used "1 kJ"/"1000 J"/"1000000 mJ" (default: `0`)                                                     |
| react_cost                                                                                           | (_optional_) How many kJ it costs over time to keep this bionic active, does nothing without a non-zero "time". Strings can be used "1 kJ"/"1000 J"/"1000000 mJ" (default: `0`) |
| time                                                                                                 | (_optional_) How long, when activated, between drawing cost. If 0, it draws power once. (default: `0`)                                                                          |
| upgraded_bionic                                                                                      | (_optional_) Bionic that can be upgraded by installing this one.                                                                                                                |
| required_bionics                                                                                     | (_optional_) Bionics which are required to install this bionic, and which cannot be uninstalled if this bionic is installed                                                     |
| available_upgrades                                                                                   | (_optional_) Upgrades available for this bionic, i.e. the list of bionics                                                                                                       |
| having this one referenced by `upgraded_bionic`.                                                     |                                                                                                                                                                                 |
| and how much this bionic encumber them.                                                              |                                                                                                                                                                                 |
| carrying capacity in grams, can be negative. Strings can be used - "5000 g" or "5 kg" (default: `0`) |                                                                                                                                                                                 |
|                                                                                                      | weight_capacity_modifier                                                                                                                                                        |
| (default: `1`)                                                                                       |                                                                                                                                                                                 |
| when this bionic is installed (e.g. because it replaces the fault biological part).                  |                                                                                                                                                                                 |
| included_bionics                                                                                     | (_optional_) Additional bionics that are installed automatically when this bionic                                                                                               |
| is installed. This can be used to install several bionics from one CBM item, which is useful as each |                                                                                                                                                                                 |
| of those can be activated independently.                                                             |                                                                                                                                                                                 |
| with another. If true this bionic does not require a CBM item to be defined. (default: `false`)      |                                                                                                                                                                                 |
| env_protec                                                                                           | (_optional_) How much environmental protection does this bionic provide on the                                                                                                  |
| specified body parts.                                                                                |                                                                                                                                                                                 |
| provide on the specified body parts.                                                                 |                                                                                                                                                                                 |
| bionic provide on the specified body parts.                                                          |                                                                                                                                                                                 |
| protect does this bionic provide on the specified body parts.                                        |                                                                                                                                                                                 |
| A list of body parts occupied by this bionic, and the number of bionic slots it take on those parts. |                                                                                                                                                                                 |
|                                                                                                      | capacity                                                                                                                                                                        |
| kJ"/"1000 J"/"1000000 mJ" (default: `0`)                                                             |                                                                                                                                                                                 |
| bionic can use to produce bionic power.                                                              |                                                                                                                                                                                 |
| allows you to plug your power banks to an external power source (solar backpack, UPS, vehicle etc)   |                                                                                                                                                                                 |
| via a cable. (default: `false`)                                                                      |                                                                                                                                                                                 |
| store.                                                                                               |                                                                                                                                                                                 |
| `0`)                                                                                                 |                                                                                                                                                                                 |
| (default: `1`)                                                                                       |                                                                                                                                                                                 |
| converted into power. Useful for CBM using PERPETUAL fuel like `muscle`, `wind` or `sun_light`.      |                                                                                                                                                                                 |
| (default: `0`)                                                                                       |                                                                                                                                                                                 |
| power. (default: `false`)                                                                            |                                                                                                                                                                                 |
| diminishing fuel_efficiency. Float between 0.0 and 1.0. (default: `nullopt`)                         |                                                                                                                                                                                 |
| (_optional_) `emit_id` of the field emitted by this bionic when it produces energy. Emit_ids are     |                                                                                                                                                                                 |
| defined in `emit.json`.                                                                              |                                                                                                                                                                                 |
| designated as follow: "DEX", "INT", "STR", "PER".                                                    |                                                                                                                                                                                 |
| enchantments applied by this CBM (see MAGIC.md for instructions on enchantment. NB: enchantments are |                                                                                                                                                                                 |
| not necessarily magic.)                                                                              |                                                                                                                                                                                 |
| installing this CBM, and lose when you uninstall this CBM. Spell classes are automatically gained.   |                                                                                                                                                                                 |
| fake_item                                                                                            | (_optional_) ID of fake item used by this bionic. Mandatory for gun and weapon                                                                                                  |
| bionics.                                                                                             |                                                                                                                                                                                 |

```json
{
    "id"           : "bio_batteries",
    "name"         : "Battery System",
    "active"       : false,
    "act_cost"     : 0,
    "time"         : 1,
    "fuel_efficiency": 1,
    "stat_bonus": [ [ "INT", 2 ], [ "STR", 2 ] ],
    "fuel_options": [ "battery" ],
    "fuel_capacity": 500,
    "encumbrance"  : [ [ "torso", 10 ], [ "arm_l", 10 ], [ "arm_r", 10 ], [ "leg_l", 10 ], [ "leg_r", 10 ], [ "foot_l", 10 ], [ "foot_r", 10 ] ],
    "description"  : "You have a battery draining attachment, and thus can make use of the energy contained in normal, everyday batteries. Use 'E' to consume batteries.",
    "canceled_mutations": ["HYPEROPIC"],
    "included_bionics": ["bio_blindfold"]
},
{
    "id": "bio_purifier",
    "type": "bionic",
    "name": "Air Filtration System",
    "description": "Surgically implanted in your trachea is an advanced filtration system.  If toxins, or airborne diseases find their way into your windpipe, the filter will attempt to remove them.",
    "occupied_bodyparts": [ [ "torso", 4 ], [ "mouth", 2 ] ],
    "env_protec": [ [ "mouth", 7 ] ],
    "bash_protec": [ [ "leg_l", 3 ], [ "leg_r", 3 ] ],
    "cut_protec": [ [ "leg_l", 3 ], [ "leg_r", 3 ] ],
    "bullet_protec": [ [ "leg_l", 3 ], [ "leg_r", 3 ] ],
    "learned_spells": [ [ "mint_breath", 2 ] ],
    "flags": [ "BIONIC_NPC_USABLE" ]
}
```

Bionics effects are defined in the code and new effects cannot be created through JSON alone. When
adding a new bionic, if it's not included with another one, you must also add the corresponding CBM
item in `data/json/items/bionics.json`. Even for a faulty bionic.

### Dreams

| Identifier | Description                                                          |
| ---------- | -------------------------------------------------------------------- |
| messages   | List of potential dreams.                                            |
| category   | Mutation category needed to dream.                                   |
| strength   | Mutation category strength required (1 = 20-34, 2 = 35-49, 3 = 50+). |

```json
{
  "messages": [
    "You have a strange dream about birds.",
    "Your dreams give you a strange feathered feeling."
  ],
  "category": "MUTCAT_BIRD",
  "strength": 1
}
```

### Disease

| Identifier         | Description                                                                                              |
| ------------------ | -------------------------------------------------------------------------------------------------------- |
| id                 | Unique ID. Must be one continuous word, use underscores if necessary.                                    |
| min_duration       | The minimum duration the disease can last. Uses strings "x m", "x s","x d".                              |
| max_duration       | The maximum duration the disease can last.                                                               |
| min_intensity      | The minimum intensity of the effect applied by the disease                                               |
| max_intensity      | The maximum intensity of the effect.                                                                     |
| health_threshold   | The amount of health above which one is immune to the disease. Must be between -200 and 200. (optional ) |
| symptoms           | The effect applied by the disease.                                                                       |
| affected_bodyparts | The list of bodyparts on which the effect is applied. (optional, default to num_bp)                      |

```json
{
  "type": "disease_type",
  "id": "bad_food",
  "min_duration": "6 m",
  "max_duration": "1 h",
  "min_intensity": 1,
  "max_intensity": 1,
  "affected_bodyparts": ["TORSO"],
  "health_threshold": 100,
  "symptoms": "foodpoison"
}
```

### Names

```json
{ "name" : "Aaliyah", "gender" : "female", "usage" : "given" }, // Name, gender, "given"/"family"/"city" (first/last/city name).
```

### Scent_types

| Identifier        | Description                                                                    |
| ----------------- | ------------------------------------------------------------------------------ |
| id                | Unique ID. Must be one continuous word, use underscores if necessary.          |
| receptive_species | Species able to track this scent. Must use valid ids defined in `species.json` |

```json
{
  "type": "scent_type",
  "id": "sc_flower",
  "receptive_species": ["MAMMAL", "INSECT", "MOLLUSK", "BIRD"]
}
```

### Scores and Achievements

Scores are defined in two or three steps based on _events_. To see what events exist and what data
they contain, read [`event.h`](../../../../../../../src/event.h).

Each event contains a certain set of fields. Each field has a string key and a `cata_variant` value.
The fields should provide all the relevant information about the event.

For example, consider the `gains_skill_level` event. You can see this specification for it in
`event.h`:

```json
template<>
struct event_spec<event_type::gains_skill_level> {
    static constexpr std::array<std::pair<const char *, cata_variant_type>, 3> fields = {{
            { "character", cata_variant_type::character_id },
            { "skill", cata_variant_type::skill_id },
            { "new_level", cata_variant_type::int_ },
        }
    };
};
```

From this, you can see that this event type has three fields:

- `character`, with the id of the character gaining the level.
- `skill`, with the id of the skill gained.
- `new_level`, with the integer level newly acquired in that skill.

Events are generated by the game when in-game circumstances dictate. These events can be transformed
and summarized in various ways. There are three concepts involved: event streams, event statistics,
and scores.

- Each `event_type` defined by the game generates an event stream.
- Further event streams can be defined in json by applying an `event_transformation` to an existing
  event stream.
- An `event_statistic` summarizes an event stream into a single value (usually a number, but other
  types of value are possible).
- A `score` uses such a statistic to define an in-game score which players can see.

#### `event_transformation`

An `event_transformation` can modify an event stream, producing another event stream.

The input stream to be transformed is specified either as an `"event_type"`, to use one of the
built-in event type streams, or an `"event_transformation"`, to use another json-defined transformed
event stream.

Any or all of the following alterations can be made to the event stream:

- Add new fields to each event based on event field transformations. The event field transformations
  can be found in
  [`event_field_transformation.cpp`](https://github.com/cataclysmbnteam/Cataclysm-BN/blob/main/src/event_field_transformations.cpp).
- Filter events based on the values they contain to produce a stream containing some subset of the
  input stream.
- Drop some fields which are not of interest in the output stream.

Here are examples of each modification:

```json
"id": "avatar_kills_with_species",
"type": "event_transformation",
"event_type": "character_kills_monster", // Transformation acts upon events of this type
"new_fields": { // A dictionary of new fields to add to the event
    // The key is the new field name; the value should be a dictionary of one element
    "species": {
        // The key specifies the event_field_transformation to apply; the value specifies
        // the input field whose value should be provided to that transformation.
        // So, in this case, we are adding a new field 'species' which will
        // contain the species of the victim of this kill event.
        "species_of_monster": "victim_type"
    }
}
```

```json
"id": "moves_on_horse",
"type": "event_transformation",
"event_type" : "avatar_moves", // An event type.  The transformation will act on events of this type
"value_constraints" : { // A dictionary of constraints
    // Each key is the field to which the constraint applies
    // The value specifies the constraint.
    // "equals" can be used to specify a constant string value the field must take.
    // "equals_statistic" specifies that the value must match the value of some statistic (see below)
    "mount" : { "equals": "mon_horse" }
}
// Since we are filtering to only those events where 'mount' is 'mon_horse', we
// might as well drop the 'mount' field, since it provides no useful information.
"drop_fields" : [ "mount" ]
```

#### `event_statistic`

As with `event_transformation`, an `event_statistic` requires an input event stream. That input
stream can be specified in the same was as for `event_transformation`, via one of the following two
entries:

```json
"event_type" : "avatar_moves" // Events of this built-in type
"event_transformation" : "moves_on_horse" // Events resulting from this json-defined transformation
```

Then it specifies a particular `stat_type` and potentially additional details as follows:

The number of events:

```json
"stat_type" : "count"
```

The sum of the numeric value in the specified field across all events:

```json
"stat_type" : "total"
"field" : "damage"
```

The maximum of the numeric value in the specified field across all events:

```json
"stat_type" : "maximum"
"field" : "damage"
```

The minimum of the numeric value in the specified field across all events:

```json
"stat_type" : "minimum"
"field" : "damage"
```

Assume there is only a single event to consider, and take the value of the given field for that
unique event:

```json
"stat_type": "unique_value",
"field": "avatar_id"
```

Regardless of `stat_type`, each `event_statistic` can also have:

```json
// Intended for use in describing scores and achievement requirements.
"description": "Number of things"
```

#### `score`

Scores simply associate a description to an event for formatting in tabulations of scores. The
`description` specifies a string which is expected to contain a `%s` format specifier where the
value of the statistic will be inserted.

Note that even though most statistics yield an integer, you should still use `%s`.

If the underlying statistic has a description, then the score description is optional. It defaults
to "<statistic description>: <value>".

```json
"id": "score_headshots",
"type": "score",
"description": "Headshots: %s",
"statistic": "avatar_num_headshots"
```

#### `achievement`

Achievements are goals for the player to aspire to, in the usual sense of the term as popularised in
other games.

An achievement is specified via requirements, each of which is a constraint on an `event_statistic`.
For example:

```json
{
  "id": "achievement_kill_zombie",
  "type": "achievement",
  // The achievement name and description are used for the UI.
  // Description is optional and can provide extra details if you wish.
  "name": "One down, billions to go\u2026",
  "description": "Kill a zombie",
  "requirements": [
    // Each requirement must specify the statistic being constrained, and the
    // constraint in terms of a comparison against some target value.
    { "event_statistic": "num_avatar_zombie_kills", "is": ">=", "target": 1 }
  ]
},
```

The `"is"` field must be `">="`, `"<="` or `"anything"`. When it is not `"anything"` the `"target"`
must be present, and must be an integer.

There are further optional fields:

```json
"hidden_by": [ "other_achievement_id" ]
```

Give a list of other achievement ids. This achievement will be hidden (i.e. not appear in the
achievements UI) until all of the achievements listed have been completed.

Use this to prevent spoilers or to reduce clutter in the list of achievements.

```json
"skill_requirements": [ { "skill": "archery", "is": ">=", "level": 5 } ]
```

This allows a skill level requirement (either an upper or lower bound) on when the achievement can
be claimed. The `"skill"` field uses the id of a skill.

Note that like `"time_constraint"` below achievements can only be captured when a statistic listed
in `"requirements"` changes.

```json
"kill_requirements": [ { "faction": "ZOMBIE", "is": ">=", "count": 1 }, { "monster": "mon_sludge_crawler", "is": ">=", "count": 1 } ],
```

This allows a kill requirement (either an upper or lower bound) on when the achievement can be
claimed. Can be defined with either `"faction"` or `"monster"` as a target, using species ids from
`species.json` or a specific monster id.

Only one of the `"monster"`/`"faction"` fields can be used per entry. If neither are used, any
monster will fulfill the requirements.

NPCs cannot currently be defined as a target.

Note that like `"time_constraint"` below achievements can only be captured when a statistic listed
in `"requirements"` changes.

```json
"time_constraint": { "since": "game_start", "is": "<=", "target": "1 minute" }
```

This allows putting a time limit (either a lower or upper bound) on when the achievement can be
claimed. The `"since"` field can be either `"game_start"` or `"cataclysm"`. The `"target"` describes
an amount of time since that reference point.

Note that achievements can only be captured when a statistic listed in their requirements changes.
So, if you want an achievement which would normally be triggered by reaching some time threshold
(such as "survived a certain amount of time") then you must place some requirement alongside it to
trigger it after that time has passed. Pick some statistic which is likely to change often, and add
an `"anything"` constraint on it. For example:

```json
{
  "id": "achievement_survive_one_day",
  "type": "achievement",
  "description": "The first day of the rest of their unlives",
  "time_constraint": { "since": "game_start", "is": ">=", "target": "1 day" },
  "requirements": [ { "event_statistic": "num_avatar_wake_ups", "is": "anything" } ]
},
```

This is a simple "survive a day" but is triggered by waking up, so it will be completed when you
wake up for the first time after 24 hours into the game.

### Skills

```json
"id" : "smg",  // Unique ID. Must be one continuous word, use underscores if necessary
"name" : "submachine guns",  // In-game name displayed
"description" : "Your skill with submachine guns and machine pistols. Halfway between a pistol and an assault rifle, these weapons fire and reload quickly, and may fire in bursts, but they are not very accurate.", // In-game description
"tags" : ["gun_type"]  // Special flags (default: none)
```

## `json/` JSONs

### Harvest

```json
{
    "id": "jabberwock",
    "type": "harvest",
    "message": "You messily hack apart the colossal mass of fused, rancid flesh, taking note of anything that stands out.",
    "entries": [
      { "drop": "meat_tainted", "type": "flesh", "mass_ratio": 0.33 },
      { "drop": "fat_tainted", "type": "flesh", "mass_ratio": 0.1 },
      { "drop": "jabberwock_heart", "base_num": [ 0, 1 ], "scale_num": [ 0.6, 0.9 ], "max": 3, "type": "flesh" }
    ],
},
{
  "id": "mammal_large_fur",
  "//": "drops large stomach",
  "type": "harvest",
  "entries": [
    { "drop": "meat", "type": "flesh", "mass_ratio": 0.32 },
    { "drop": "meat_scrap", "type": "flesh", "mass_ratio": 0.01 },
    { "drop": "lung", "type": "flesh", "mass_ratio": 0.0035 },
    { "drop": "liver", "type": "offal", "mass_ratio": 0.01 },
    { "drop": "brain", "type": "flesh", "mass_ratio": 0.005 },
    { "drop": "sweetbread", "type": "flesh", "mass_ratio": 0.002 },
    { "drop": "kidney", "type": "offal", "mass_ratio": 0.002 },
    { "drop": "stomach_large", "scale_num": [ 1, 1 ], "max": 1, "type": "offal" },
    { "drop": "bone", "type": "bone", "mass_ratio": 0.15 },
    { "drop": "sinew", "type": "bone", "mass_ratio": 0.00035 },
    { "drop": "fur", "type": "skin", "mass_ratio": 0.02 },
    { "drop": "fat", "type": "flesh", "mass_ratio": 0.07 }
  ]
},
{
  "id": "CBM_SCI",
  "type": "harvest",
  "entries": [
    {
      "drop": "bionics_sci",
      "type": "bionic_group",
      "flags": [ "NO_STERILE", "NO_PACKED" ],
      "faults": [ "fault_bionic_salvaged" ]
    },
    { "drop": "meat_tainted", "type": "flesh", "mass_ratio": 0.25 },
    { "drop": "fat_tainted", "type": "flesh", "mass_ratio": 0.08 },
    { "drop": "bone_tainted", "type": "bone", "mass_ratio": 0.1 }
  ]
},
```

#### `id`

Unique id of the harvest definition.

#### `type`

Should always be `harvest` to mark the object as a harvest definition.

#### `message`

Optional message to be printed when a creature using the harvest definition is butchered. May be
omitted from definition.

#### `entries`

Array of dictionaries defining possible items produced on butchering and their likelihood of being
produced. `drop` value should be the `id` string of the item to be produced.

`type` value should be a string with the associated body part the item comes from. Acceptable values
are as follows: `flesh`: the "meat" of the creature. `offal`: the "organs" of the creature. these
are removed when field dressing. `skin`: the "skin" of the creature. this is what is ruined while
quartering. `bone`: the "bones" of the creature. you will get some amount of these from field
dressing, and the rest of them from butchering the carcass. `bionic`: an item gained by dissecting
the creature. not restricted to CBMs. `bionic_group`: an item group that will give an item by
dissecting a creature. not restricted to groups containing CBMs.

`flags` value should be an array of strings. It's the flags that will be added to te items of that
entry upon harvesting.

`faults` value should be an array of `fault_id` strings. It's the faults that will be added to te
items of that entry upon harvesting.

For every `type` other then `bionic` and `bionic_group` following entries scale the results:
`base_num` value should be an array with two elements in which the first defines the minimum number
of the corresponding item produced and the second defines the maximum number. `scale_num` value
should be an array with two elements, increasing the minimum and maximum drop numbers respectively
by element value * survival skill. `max` upper limit after `bas_num` and `scale_num` are calculated
using `mass_ratio` value is a multiplier of how much of the monster's weight comprises the
associated item. to conserve mass, keep between 0 and 1 combined with all drops. This overrides
`base_num`, `scale_num` and `max`

For `type`s: `bionic` and `bionic_group` following enrties can scale the results: `max` this value
(in contrary to `max` for other `type`s) corresponds to maximum butchery roll that will be passed to
check_butcher_cbm() in activity_handlers.cpp; view check_butcher_cbm() to see corresponding
distribution chances for roll values passed to that function

### Weapon Category

Used to classify weapons (guns or melee) into groups, mainly for use in martial arts.

```json
{
  "type": "weapon_category",
  "id": "WEAP_CAT",
  "name": "Weapon Category"
}
```

`"name"` is a translatable string used for UI display in martial arts UI, while ID is used for JSON
entries.

## Obsoletion and migration

For maps, you remove the item from all the places it can spawn, remove the mapgen entries, and add
the overmap terrain id into `data/json/obsoletion/migration_oter_ids.json`, to migrate oter_id
`underground_sub_station` and `sewer_sub_station` into their rotatable versions, note that if mapgen
has already generated this area this will only alter the tile shown on the overmap:

```json
{
  "type": "oter_id_migration",
  "//": "obsoleted in 0.4",
  "old_directions": false,
  "new_directions": false,
  "oter_ids": {
    "underground_sub_station": "underground_sub_station_north",
    "sewer_sub_station": "sewer_sub_station_north"
  }
}
```

If `old_directions` option is enabled each entry will create four migrations: for `old_north`,
`old_west`, `old_south`, and `old_east`. What they will be migrated to depends of value of
`new_directions` option. If it is set to `true` then terrains will be migrated to same directions:
`old_north` to `new_north`, `old_east` to `new_east` and such. If `new_directions` is set to
`false`, then all four terrains will be migrated to one plain `new`. For both of those cases you
only need to specify plain `old` and `new` names in `oter_ids` map, without any suffixes.
