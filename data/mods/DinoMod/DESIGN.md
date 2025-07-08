# Design Document SPOILERS AHEAD
The core idea of DinoMod is this: https://tvtropes.org/pmwiki/pmwiki.php/Main/EverythingsBetterWithDinosaurs
The purpose of this mod is to make Cataclysm a more fun game to play and develop for, showing off new mechanics and bringing life to parts of the game that aren't as well-developed.

# What belongs in DinoMod?
Dinosaurs and content based around dinosaurs that for whatever reason can't just go into the vanilla game.

# Realism vs. Fun
There is more space in mods for fun. In general the vanilla living dinos should be as realistic as possible to the science we know but the zombie, CBM, and fungal dinos can go anywhere as long as it's fun.

# Adding, replacing, removing
This mod is about adding content. In general it should have as light a touch on the vanilla game as possible. This helps us keep it working by itself and with other mods and Cataclysm variants.

# How to contribute
This mod is distributed with the base game, so any content contributed will have to be submitted to the Github site. You can talk with the mod maintainers about your ideas on Discord.

# Where should new dinosaurs spawn?
North American dinos should be added to dinosaur and wilderness monster groups. Zombie variants should be added to the zinosaur monster groups. Dinos from other parts of the world should be added to the labs monster group and/or get a new dedicated lab finale variant just for them, especially good for finales if they're very dangerous. Small dinos that can't zombify are good options for CBM dinos. Fungal zombie variants get added to fungal spawn lists.

# How to add a dinosaur
As of this writing, each dinosaur touches at least ten different JSON files, listed here by folder. Please put the new dino in the same order in all files, near similar dinos, organized by real world taxonomy. 

Main DinoMod folder: 

* cooking_components.json is where you add the dinosaur egg to allow it to be cooked, 
* egg.json is where you create the dino egg

monstergroups folder:

* dinosaur.json is where you add the dinosaur to spawn in special DinoMod locations
* monstergroups_egg.json is where you add the hatchling to be spawned from its own egg, and from random eggs
* wilderness.json is where your dino will be spawned in natural settings. Forests should stay safe.
* zinosaur.json  adds the zombified version to zombie spawn lists

monsters folder:

* dinosaur.json is where you finally create the dino itself. copy-from can be a good move to keep things tidy if there is already a similar dino
* hatchling.json is where freshly hatched dinos go. Tiny dinos grow to adults directly, but larger ones (15 kg or greater) grow into...
* juvenile.json is where juveniles go. They're five times bigger but still pretty tiny by dino standards. They grow to be adults in adult weight in kg divided by six days or one year, whichever is shorter.  Sauropods are they exception, they should be 1000 kg and L so they can defend themselves some.
* zed-dinosaur.json is where new zombified dinos go. copy-from can be very helpful here.

# DDA and BN
As much as possible, there should be content and feature parity between both (all?) Cataclysm variants for the best player experience and easiest mod maintenance. As of October 2022 the known code differences between the two for DinoMod are:
* Monstergroups folder - BN groups need  "default" and "cost_multiplier" to be defined, "weight should be "freq" instead
* Monsters folder:
    - DDA supports dissect and melee_damage, while BN supports melee_cut but not dissect or melee damage
    - the DDA death_guilt system is handled by the GUILT flag in BN
    - the DDA "bleeds" and "bleed_rate" fields are handled by BLEEDS flag in BN
    - bullet armor is "armor_bullet" in BN and so on for other armor types
    - DDA's petfood entry is handled by DOGFOOD, CATFOOD, and CATTLEFODDER flags in BN.
    - Death functions for ACID and FIREBALL and SMOKEBURST have different formatting also.
    - RANGED_ATTACKER and WATER_CAMOUFLAGE and CAN_BE_CULLED are flags in DDA but not BN.
    - smash special attack in DDA is SMASH in BN.
    - no EATS flag in BN
    - no SMALL_HIDER flag in BN
    - no CORNERED_FIGHTER flag in BN
    - no COMBAT_MOUNT flag in BN
    - no vertebrate parts to drop in BN.
    - PATH_AVOID_DANGER flag in DDA is PATH_AVOID_DANGER_1 in BN
    - no bleed_rate field in BN
    - no stomach_size field in BN
    - no families field in BN
    - no biosignature field in BN
    - no aggro_character field in BN
    - no weakpoint_sets field in BN
    - for reproduction no baby_type field is needed or understood in BN
    - no EAT_CARRION special attack in BN
    - no BROWSE or GRAZE special attacks in BN
* Mapgen folder - lots of content missing or different in BN, science basements still need to be reconstructed from BN originals, t_soil isn't a valid terrain in BN, spawn_data": "patrol" doesn't exist in BN. Dozens of nests had to be removed because mapgen could not see the monsters, I'm guessing copy strangeness. Added portal_location to DinoMod since portal map extras do not exist in BN
* Items folder - BN port removes pocket_data and amm0_to_fire and longest_side lines and changes damage_type bullet to stab and removes armor entry. No copy-from or event entries in item groups in BN. No dry catfood or dogfood to copy from in BN
* egg items - BN port removes FREEZERBURN flag
* monster_factions - copy-from doesn't work for changing vanilla monster factions in BN
* Overmap folder - CLASSIC and MAN_MADE flags and min_max_zlevel and terrain not supported in BN
* Recipe folder - activity_level and proficiencies not supported in BN. chain mail recipes must use chainmail_vest item id in BN. Cutting 2 doesn't exist in BN so everything needs to be changed to cutting 1. blacksmithing_standard crafting requirement specifies anvil quality 1, supplemented by blacksmithing_intermediate and blacksmithing_advanced requirements bundling higher anvil quality plus chisel quality, then swage and die set.
* Requirements folder - extend not supported in BN. Bronze armor copies from a different item ID in BN
* Harvest file - no marrow in BN, faulty bionics use the "fault_bionic_nonsterile" fault instead of flags, use of different bionic_group itemgroups  for power storage and military dino bionics (which add a chance to generate nothing or non-CBM components, in return for dissection code having a higher success rate in BN)
* monster_attacks file - no BN support for these fields: range, no_adjacent, throw_strength, throw_msg_u, throw_msg_npc
