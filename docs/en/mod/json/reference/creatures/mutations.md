# Mutations & Traits

> [!NOTE]
>
> This page is currently under-construction and was recently split off from `JSON INFO`

### Traits/Mutations fields

```json
"id": "LIGHTEATER",  // Unique ID
"name": "Optimist",  // In-game name displayed
"points": 2,         // Point cost of the trait. Positive values cost points and negative values give points
"visibility": 0,     // Visibility of the trait for purposes of NPC interaction (default: 0)
"ugliness": 0,       // Ugliness of the trait for purposes of NPC interaction (default: 0)
"cut_dmg_bonus": 3, // Bonus to unarmed cut damage (default: 0)
"pierce_dmg_bonus": 3, // Bonus to unarmed pierce damage (default: 0.0)
"bash_dmg_bonus": 3, // Bonus to unarmed bash damage (default: 0)
"butchering_quality": 4, // Butchering quality of this mutations (default: 0)
"rand_cut_bonus": { "min": 2, "max": 3 }, // Random bonus to unarmed cut damage between min and max.
"rand_bash_bonus": { "min": 2, "max": 3 }, // Random bonus to unarmed bash damage between min and max.
"bodytemp_modifiers" : [100, 150], // Range of additional bodytemp units (these units are described in 'weather.h'. First value is used if the person is already overheated, second one if it's not.
"bodytemp_sleep" : 50, // Additional units of bodytemp which are applied when sleeping
"initial_ma_styles": [ "style_crane" ], // (optional) A list of ids of martial art styles of which the player can choose one when starting a game.
"mixed_effect": false, // Whether the trait has both positive and negative effects. This is purely declarative and is only used for the user interface. (default: false)
"description": "Nothing gets you down!" // In-game description
"starting_trait": true, // Can be selected at character creation (default: false)
"valid": false,      // Can be mutated ingame (default: true)
"purifiable": false, //Sets if the mutation be purified (default: true)
"profession": true, //Trait is a starting profession special trait. (default: false)
"debug": false,     //Trait is for debug purposes (default: false)
"player_display": true, //Trait is displayed in the `@` player display menu
"category": ["BIRD", "INSECT"], // Categories containing this mutation
"prereqs": ["SKIN_ROUGH"], // Needs these mutations before you can mutate toward this mutation
"prereqs2": ["LEAVES"], //Also need these mutations before you can mutate towards this mutation. When both set creates 2 different mutation paths, random from one is picked. Only use together with "prereqs"
"threshold_tier": 1, //Required tier of one of the categories' thresholds to mutate
"cancels": ["ROT1", "ROT2", "ROT3"], // Cancels these mutations when mutating
"prevents": ["ROT1", "ROT2", "ROT3"], // Prevents you from recieving these mutations when mutating
"changes_to": ["FASTHEALER2"], // Can change into these mutations when mutating further
"leads_to": [], // Mutations that add to this one
"passive_mods" : { //increases stats with the listed value. Negative means a stat reduction
            "per_mod" : 1, //Possible values per_mod, str_mod, dex_mod, int_mod
            "str_mod" : 2
},
"wet_protection":[{ "part": "HEAD", // Wet Protection on specific bodyparts
                    "good": 1 } ] // "neutral/good/ignored" // Good increases pos and cancels neg, neut cancels neg, ignored cancels both
"vitamin_rates": [ [ "vitC", -1200 ] ], // How much extra vitamins do you consume per minute. Negative values mean production
"vitamins_absorb_multi":  [ "flesh", [ [ "vitA", 0 ], [ "vitB", 0 ], [ "vitC", 0 ], [ "calcium", 0 ], [ "iron", 0 ] ], [ "all", [ [ "vitA", 2 ], [ "vitB", 2 ], [ "vitC", 2 ], [ "calcium", 2 ], [ "iron", 2 ] ] ] ], // multiplier of vitamin absorption based on material. "all" is every material. supports multiple materials.
"craft_skill_bonus": [ [ "electronics", -2 ], [ "tailor", -2 ], [ "mechanics", -2 ] ], // Skill affected by the mutation and their bonuses. Bonuses can be negative, a bonus of 4 is worth 1 full skill level.
"restricts_gear" : [ "TORSO" ], //list of bodyparts that get restricted by this mutation
"allowed_items" : [ "TORSO" ], //Items with these flags can be worn regardless of restrictions.
"allowed_items_only" : true, //If true and this mutation restricts gear, then only `allowed_items` can be worn with this, excluding even OVERSIZE items that would otherwise be wearable. (default: false)
"allow_soft_gear" : true, //If there is a list of 'restricts_gear' this sets if the location still allows items made out of soft materials (Only one of the types need to be soft for it to be considered soft). (default: false)
"destroys_gear" : true, //If true, destroys the gear in the 'restricts_gear' location when mutated into. (default: false)
"encumbrance_always" : [ // Adds this much encumbrance to selected body parts
    [ "ARM_L", 20 ],
    [ "ARM_R", 20 ]
],
"encumbrance_covered" : [ // Adds this much encumbrance to selected body parts, but only if the part is covered by not-OVERSIZE worn equipment
    [ "HAND_L", 50 ],
    [ "HAND_R", 50 ]
],
"armor" : [ // Protects selected body parts this much. Resistances use syntax like `PART RESISTANCE` below.
    [
        [ "ALL" ], // Shorthand that applies the selected resistance to the entire body
        { "bash" : 2 } // The resistance provided to the body part(s) selected above
    ],
    [   // NOTE: Resistances are applies in order and ZEROED between applications!
        [ "ARM_L", "ARM_R" ], // Overrides the above settings for those body parts
        { "bash" : 1 }        // ...and gives them those resistances instead
    ]
],
"dodge_modifier" : 1, // Bonus granted to your effective change to dodge. Negative values inflict a penalty instead.
"speed_modifier" : 2.0, // Multiplies your current speed by this amount.  Higher means more speed, with 1.0 being no change.
"movecost_modifier" : 0.5, // Multiplies the amount of moves it takes to perform most actions, with lower meaning faster actions. 
"movecost_flatground_modifier" : 0.5, // Multiplies the movecost of terrain/furniture that is easy to move over. Lower means faster movement.
"movecost_obstacle_modifier" : 0.5, // Multipliers the movecost across rough terrain, lower means faster movement
"movecost_swim_modifier" : 0.5, // Multiplies the movecost needed to swim. Lower is faster.
"falling_damage_multiplier" : 0.5, // Multiplier on how much damage you take when falling into pits, falling off ledges, or being tossed by a hulk. Zero grants immunity.
"attackcost_modifier" : 0.5, // Multiplier on the movecost of attacks. Lower is better.
"weight_capacity_modifier" : 0.5, // Multiplies how much weight you can carry before being penalized.
"hearing_modifier" : 0.5, // Multiplier for how good your hearing is, higher values mean detecting sounds from farther away. A value of zero renders you completely deaf.
"noise_modifier" : 0.5, // Multiplier for how much noise you make while moving, with zero making your footsteps silent.
"packmule_modifier" : 1.0, // Multiplies the storage space granted by backpacks and similar worn containers.
"crafting_speed_modifier" : 1.0, // Multiplies the crafting speed of the player.
"construction_speed_modifier" : 1.0, // Multiplies the construction speed of the player.
"stealth_modifier" : 0, // Percentage to be subtracted from player's visibility range, capped to 60. Negative values work, but are not very effective due to the way vision ranges are capped
"night_vision_range" : 0.0, // Night range vision. Only the best and the worst value out of all mutations are added. A value of 8 or higher will allow reading in complete darkness as though the player were in dim lighting. (default: 0.0)
"active" : true, //When set the mutation is an active mutation that the player needs to activate (default: false)
"starts_active" : true, //When true, this 'active' mutation starts active (default: false, requires 'active')
"cost" : 8, // Cost to activate this mutation. Needs one of the hunger, thirst, or fatigue values set to true. (default: 0)
"time" : 100, //Sets the amount of (turns * current player speed ) time units that need to pass before the cost is to be paid again. Needs to be higher than one to have any effect. (default: 0)
"hunger" : true, //If true, activated mutation increases hunger by cost. (default: false)
"thirst" : true, //If true, activated mutation increases thirst by cost. (default: false)
"fatigue" : true, //If true, activated mutation increases fatigue by cost. (default: false)
"stamina" : true, //If true, activated mutation decreases stamina by cost. (default: false)
"mana" : true, //If true, activated mutation decreases mana by cost. (default: false)
"bionic" : true, //If true, activated mutation decreases bionic power by cost. (default: false)
"pain" : true, //If true, activated mutation increases pain by cost. (default: false)
"health" : true, //If true, activated mutation decreases health by cost. (default: false)
"scent_modifier": 0.0,// float affecting the intensity of your smell. (default: 1.0)
"scent_intensity": 800,// int affecting the target scent toward which you current smell gravitates. (default: 500)
"scent_mask": -200,// int added to your target scent value. (default: 0)
"scent_type": "sc_flower",// scent_typeid, defined in scent_types.json, The type scent you emit. (default: empty)
"bleed_resist": 1000, // Int quantifiying your resistance to bleed effect, if its > to the intensity of the effect you don't get any bleeding. (default: 0)
"fat_to_max_hp": 1.0, // Amount of hp_max gained for each unit of bmi above character_weight_category::normal. (default: 0.0)
"healthy_rate": 0.0, // How fast your health can change. If set to 0 it never changes. (default: 1.0)
"weakness_to_water": 5, // How much damage water does to you, negative values heal you. (default: 0)
"ignored_by": [ "ZOMBIE" ], // List of species ignoring you. (default: empty)
"anger_relations": [ [ "MARSHMALLOW", 20 ], [ "GUMMY", 5 ], [ "CHEWGUM", 20 ] ], // List of species angered by you and by how much, can use negative value to calm.  (default: empty)
"can_only_eat": [ "junk" ], // List of materiel required for food to be comestible for you. (default: empty)
"can_only_heal_with": [ "bandage" ], // List of med you are restricted to, this includes mutagen,serum,aspirin,bandages etc... (default: empty)
"can_heal_with": [ "caramel_ointement" ], // List of med that will work for you but not for anyone. See `CANT_HEAL_EVERYONE` flag for items. (default: empty)
"allowed_category": [ "ALPHA" ], // List of category you can mutate into. (default: empty)
"no_cbm_on_bp": [ "TORSO", "HEAD", "EYES", "MOUTH", "ARM_L" ], // List of body parts that can't receive cbms. (default: empty)
"body_size": "LARGE", // Increase or decrease size, only one size mutation at a time will be valid. Allowed values are: `TINY`, `SMALL`, `LARGE`, `HUGE`. `MEDIUM` can be specified but has no effect, as this is the default size for players and NPCs.
"lumination": [ [ "HEAD", 20 ], [ "ARM_L", 10 ] ], // List of glowing bodypart and the intensity of the glow as a float. (default: empty)
"metabolism_modifier": 0.333, // Extra metabolism rate multiplier. 1.0 doubles usage, -0.5 halves.
"fatigue_modifier": 0.5, // Extra fatigue rate multiplier. 1.0 doubles usage, -0.5 halves.
"fatigue_regen_modifier": 0.333, // Modifier for the rate at which fatigue and sleep deprivation drops when resting.
"healing_awake": 1.0, //  Affects healing while awake. This and resting is affected by healthiness awake_rate = heal_rate * healing_awake
"healing_resting": 0.5, // Affects healing while asleep. asleep_rate = at_rest_quality * heal_rate * ( 1.0f + healing_resting)
"mending_modifier": 1.2 // Multiplier on how fast your limbs mend - This value would make your limbs mend 20% faster
"stamina_regen_modifier": 0.5 // Extra stamina regeneration rate multiplier. 1.0 doubles regen, -0.5 halves.
"max_stamina_modifier": 1.5 // Multiplier on maximum stamina.  Stamina regeneration is proportionally modified by the same rate, so this includes stamina_regen_modifier implicitly. See Character::update_stamina in Character.cpp.
"transform": { "target": "BIOLUM1", // Trait_id of the mutation this one will transfomr into
               "msg_transform": "You turn your photophore OFF.", // message displayed upon transformation
               "active": false , // Will the target mutation start powered ( turn ON ).
               "moves": 100, // how many moves this costs. (default: 0)
"enchantments": [ "MEP_INK_GLAND_SPRAY" ], // Applies this enchantment to the player. See magic.md and effects_json.md
"mutagen_target_modifier": 5         // Increases or decreases how mutations prefer to balance out when mutating from mutant toxins, negative values push the target value lower (default: 0)
}
```
