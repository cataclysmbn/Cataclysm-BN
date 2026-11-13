# Character Values

## Character Values

Jsonized value for accessing character wide bounded values within other places.

### Fields

| Identifier     | Description                                                                                            |
| -------------- | ------------------------------------------------------------------------------------------------------ |
| id             | (_mandatory_) Unique string id                                                                         |
| desc           | (_optional_) Info for how this will scale, used in enchantment desc.                                   |
| var_value      | (_optional_) Reads the integer of this character value as the value                                    |
| drain_resource | (_optional_) Hardcode character value that is used, see references [below](#hardcode-character-values) |
| min_value      | (_optional_) Minimum value that this resource can have. Used for capping effects                       |
| max_value      | (_optional_) Maximum value that this resource can have. Used for capping effects                       |

## Hardcode Character Values

| Identifier   | Description                                                          |
| ------------ | -------------------------------------------------------------------- |
| STR          | Current modified strength of the character                           |
| DEX          | Current modified dexterity of the character                          |
| INT          | Current modified intelligence of the character                       |
| PER          | Current modified perception of the character                         |
| HEALTH       | Current health value of the character                                |
| HP           | Health points of the limb with the most health                       |
| MANA         | Currently availiable mana stores                                     |
| STAMINA      | Currently available stamina stores                                   |
| BIONIC_POWER | Current bionic power in kilojoules                                   |
| FATIGUE      | Current fatigue of the character                                     |
| THIRST       | Current thirst of the character                                      |
| PAIN         | Current unmodified pain ( I.E. before painkillers ) of the character |
| RAD          | Current radiation levels of the character                            |
| HUNGER       | Percent of drained kcal, 0 is 100%, 100 is dead                      |
| MORALE       | Current morale levels of the character from all sources              |
| FOCUS        | Current focus levels ( not effective focus ) of the character        |
| NONE         | Always 0                                                             |

## Predefined Character Values.

These are the predefined values which can be used by any mod.

| Identifier    | Pair         |
| ------------- | ------------ |
| strength      | STR          |
| dexterity     | DEX          |
| intelligence  | INT          |
| perception    | PER          |
| health        | HEALTH       |
| health_points | HP           |
| mana          | MANA         |
| stamina       | STAMINA      |
| bionic_power  | BIONIC_POWER |
| fatigue       | FATIGUE      |
| thirst        | THIRST       |
| pain          | PAIN         |
| hunger        | HUNGER       |
| morale        | MORALE       |
| focus         | FOCUS        |
