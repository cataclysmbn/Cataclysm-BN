# File descriptions

Here's a quick summary of what each of the JSON files contain, broken down by folder. This list is
not comprehensive, but covers the broad strokes.

## `data/json/`

| Filename                   | Description                                                           |
| -------------------------- | --------------------------------------------------------------------- |
| achievements.json          | achievements                                                          |
| anatomy.json               | a listing of player body parts - do not edit                          |
| ascii_arts.json            | ascii arts for item descriptions                                      |
| bionics.json               | bionics, does NOT include bionic effects                              |
| body_parts.json            | an expansion of anatomy.json - do not edit                            |
| clothing_mods.json         | definition of clothing mods                                           |
| construction.json          | definition of construction menu tasks                                 |
| default_blacklist.json     | a standard blacklist of joke monsters                                 |
| doll_speech.json           | talk doll speech messages                                             |
| dreams.json                | dream text and linked mutation categories                             |
| disease.json               | disease definitions                                                   |
| effects.json               | common effects and their effects                                      |
| emit.json                  | smoke and gas emissions                                               |
| flags.json                 | common flags and their descriptions                                   |
| furniture.json             | furniture, and features treated like furniture                        |
| game_balance.json          | various options to tweak game balance                                 |
| gates.json                 | gate terrain definitions                                              |
| harvest.json               | item drops for butchering corpses                                     |
| health_msgs.json           | messages displayed when the player wakes                              |
| item_actions.json          | descriptions of standard item actions                                 |
| item_category.json         | item categories and their default sort                                |
| item_groups.json           | item spawn groups                                                     |
| lab_notes.json             | lab computer messages                                                 |
| martialarts.json           | martial arts styles and buffs                                         |
| materials.json             | material types                                                        |
| monster_attacks.json       | monster attacks                                                       |
| monster_drops.json         | monster item drops on death                                           |
| monster_factions.json      | monster factions                                                      |
| monstergroups.json         | monster spawn groups                                                  |
| monstergroups_egg.json     | monster spawn groups from eggs                                        |
| monsters.json              | monster descriptions, mostly zombies                                  |
| morale_types.json          | morale modifier messages                                              |
| mutation_category.json     | messages for mutation categories                                      |
| mutation_ordering.json     | draw order for mutation and CBM overlays in tiles mode                |
| mutations.json             | traits/mutations                                                      |
| names.json                 | names used for NPC/player name generation                             |
| overmap_connections.json   | connections for roads and tunnels in the overmap                      |
| overmap_terrain.json       | overmap terrain                                                       |
| player_activities.json     | player activities                                                     |
| professions.json           | profession definitions                                                |
| recipes.json               | crafting/disassembly recipes                                          |
| regional_map_settings.json | settings for the entire map generation                                |
| road_vehicles.json         | vehicle spawn information for roads                                   |
| rotatable_symbols.json     | rotatable symbols - do not edit                                       |
| scent_types.json           | type of scent available                                               |
| scores.json                | scores                                                                |
| skills.json                | skill descriptions and ID's                                           |
| snippets.json              | flier/poster descriptions                                             |
| species.json               | monster species                                                       |
| speech.json                | monster vocalizations                                                 |
| statistics.json            | statistics and transformations used to define scores and achievements |
| start_locations.json       | starting locations for scenarios                                      |
| techniques.json            | generic for items and martial arts                                    |
| terrain.json               | terrain types and definitions                                         |
| test_regions.json          | test regions                                                          |
| tips.json                  | tips of the day                                                       |
| tool_qualities.json        | standard tool qualities and their actions                             |
| traps.json                 | standard traps                                                        |
| tutorial.json              | messages for the tutorial (that is out of date)                       |
| vehicle_groups.json        | vehicle spawn groups                                                  |
| vehicle_parts.json         | vehicle parts, does NOT affect flag effects                           |
| vitamin.json               | vitamins and their deficiencies                                       |

selected subfolders

## `data/json/items/`

See below for specifics on the various items

| Filename                     | Description                                                        |
| ---------------------------- | ------------------------------------------------------------------ |
| ammo.json                    | common base components like batteries and marbles                  |
| ammo_types.json              | standard ammo types by gun                                         |
| archery.json                 | bows and arrows                                                    |
| armor.json                   | armor and clothing                                                 |
| bionics.json                 | Compact Bionic Modules (CBMs)                                      |
| biosignatures.json           | animal waste                                                       |
| books.json                   | books                                                              |
| chemicals_and_resources.json | chemical precursors                                                |
| comestibles.json             | food/drinks                                                        |
| containers.json              | containers                                                         |
| crossbows.json               | crossbows and bolts                                                |
| fake.json                    | fake items for bionics or mutations                                |
| fuel.json                    | liquid fuels                                                       |
| grenades.json                | grenades and throwable explosives                                  |
| handloaded_bullets.json      | random ammo                                                        |
| melee.json                   | anything that doesn't go in the other item jsons, melee weapons    |
| migration.json               | conversions of non-existent items from save games to current items |
| newspaper.json               | flyers, newspapers, and survivor notes. snippets.json for messages |
| obsolete.json                | items being removed from the game                                  |
| ranged.json                  | guns                                                               |
| software.json                | software for SD-cards and USB sticks                               |
| tool_armor.json              | clothes and armor that can be (a)ctivated                          |
| toolmod.json                 | modifications of tools                                             |
| tools.json                   | tools and items that can be (a)ctivated                            |
| vehicle_parts.json           | components of vehicles when they aren't on the vehicle             |

### `data/json/items/comestibles`

## `data/json/requirements/`

Standard components and tools for crafting

| Filename                  | Description                               |
| ------------------------- | ----------------------------------------- |
| ammo.json                 | ammo components                           |
| cooking_components.json   | common ingredient sets                    |
| cooking_requirements.json | cooking tools and heat sources            |
| materials.json            | thread, fabric, and other basic materials |
| toolsets.json             | sets of tools commonly used together      |
| uncraft.json              | common results of taking stuff apart      |
| vehicle.json              | tools to work on vehicles                 |

## `data/json/vehicles/`

Groups of vehicle definitions with self-explanatory names of files:

| Filename             |
| -------------------- |
| bikes.json           |
| boats.json           |
| cars.json            |
| carts.json           |
| custom_vehicles.json |
| emergency.json       |
| farm.json            |
| helicopters.json     |
| military.json        |
| trains.json          |
| trucks.json          |
| utility.json         |
| vans_busses.json     |
| vehicles.json        |
