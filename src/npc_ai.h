#pragma once

#include "activity_type.h"
#include "effect.h"
#include "npc.h"
#include "type_id.h"

#include <string>

enum npc_action : int {
    npc_undecided = 0,
    npc_pause,
    npc_reload,
    npc_sleep,
    npc_pickup,
    npc_heal,
    npc_use_painkiller,
    npc_drop_items,
    npc_flee,
    npc_melee,
    npc_shoot,
    npc_look_for_player,
    npc_heal_player,
    npc_follow_player,
    npc_follow_embarked,
    npc_talk_to_player,
    npc_mug_player,
    npc_goto_to_this_pos,
    npc_goto_destination,
    npc_avoid_friendly_fire,
    npc_escape_explosion,
    npc_noop,
    npc_reach_attack,
    npc_aim,
    npc_investigate_sound,
    npc_return_to_guard_pos,
    npc_player_activity,
    num_npc_actions
};

inline const activity_id ACT_PULP{"ACT_PULP"};

inline const skill_id skill_firstaid{"firstaid"};

inline const bionic_id bio_ads{"bio_ads"};
inline const bionic_id bio_advreactor{"bio_advreactor"};
inline const bionic_id bio_faraday{"bio_faraday"};
inline const bionic_id bio_furnace{"bio_furnace"};
inline const bionic_id bio_heat_absorb{"bio_heat_absorb"};
inline const bionic_id bio_heatsink{"bio_heatsink"};
inline const bionic_id bio_hydraulics{"bio_hydraulics"};
inline const bionic_id bio_leukocyte{"bio_leukocyte"};
inline const bionic_id bio_nanobots{"bio_nanobots"};
inline const bionic_id bio_painkiller{"bio_painkiller"};
inline const bionic_id bio_plutfilter{"bio_plutfilter"};
inline const bionic_id bio_radscrubber{"bio_radscrubber"};
inline const bionic_id bio_reactor{"bio_reactor"};
inline const bionic_id bio_shock{"bio_shock"};
inline const bionic_id bio_soporific{"bio_soporific"};

inline const efftype_id effect_asthma{"asthma"};
inline const efftype_id effect_bandaged{"bandaged"};
inline const efftype_id effect_bite{"bite"};
inline const efftype_id effect_bleed{"bleed"};
inline const efftype_id effect_bouldering{"bouldering"};
inline const efftype_id effect_catch_up{"catch_up"};
inline const efftype_id effect_disinfected{"disinfected"};
inline const efftype_id effect_hallu{"hallu"};
inline const efftype_id effect_hit_by_player{"hit_by_player"};
inline const efftype_id effect_infected{"infected"};
inline const efftype_id effect_lying_down{"lying_down"};
inline const efftype_id effect_no_sight{"no_sight"};
inline const efftype_id effect_npc_fire_bad{"npc_fire_bad"};
inline const efftype_id effect_npc_flee_player{"npc_flee_player"};
inline const efftype_id effect_npc_player_looking{"npc_player_still_looking"};
inline const efftype_id effect_npc_run_away{"npc_run_away"};
inline const efftype_id effect_onfire{"onfire"};
inline const efftype_id effect_stunned{"stunned"};
inline const efftype_id effect_attention{"attention"};
inline const efftype_id effect_feral_infighting_punishment{"feral_infighting_punishment"};

inline const trait_id trait_ANIMALDISCORD{"ANIMALDISCORD"};
inline const trait_id trait_ANIMALDISCORD2{"ANIMALDISCORD2"};
inline const trait_id trait_ANIMALEMPATH{"ANIMALEMPATH"};
inline const trait_id trait_ANIMALEMPATH2{"ANIMALEMPATH2"};
inline const trait_id trait_BEE{"BEE"};
inline const trait_id trait_FLOWERS{"FLOWERS"};
inline const trait_id trait_MYCUS_FRIEND{"MYCUS_FRIEND"};
inline const trait_id trait_PHEROMONE_INSECT{"PHEROMONE_INSECT"};
inline const trait_id trait_PHEROMONE_MAMMAL{"PHEROMONE_MAMMAL"};
inline const trait_id trait_PROF_FERAL{"PROF_FERAL"};
inline const trait_id trait_TERRIFYING{"TERRIFYING"};
inline const trait_id trait_THRESH_MYCUS{"THRESH_MYCUS"};

inline const itype_id itype_battery{"battery"};
inline const itype_id itype_chem_ethanol{"chem_ethanol"};
inline const itype_id itype_chem_methanol{"chem_methanol"};
inline const itype_id itype_denat_alcohol{"denat_alcohol"};
inline const itype_id itype_inhaler{"inhaler"};
inline const itype_id itype_lsd{"lsd"};
inline const itype_id itype_smoxygen_tank{"smoxygen_tank"};
inline const itype_id itype_thorazine{"thorazine"};
inline const itype_id itype_oxygen_tank{"oxygen_tank"};

inline const itype_id fuel_wind{"wind"};
inline const itype_id fuel_sunlight{"sunlight"};

inline constexpr float NPC_DANGER_VERY_LOW = 5.0f;
inline constexpr float NPC_DANGER_MAX = 150.0f;
inline constexpr float MAX_FLOAT = 5000000000.0f;

inline constexpr int avoidance_vehicles_radius = 5;

namespace npc_ai {
/// AI decision seam: each free function takes the npc it decides for.
auto assess_danger(npc& who) -> void;
auto regen_ai_cache(npc& who) -> void;
auto method_of_fleeing(npc& who) -> npc_action;
auto method_of_attack(npc& who) -> npc_action;
auto address_needs(npc& who) -> npc_action;
auto address_needs(npc& who, float danger) -> npc_action;
auto address_player(npc& who) -> npc_action;
auto long_term_goal_action(npc& who) -> npc_action;
} // namespace npc_ai
