#pragma once

#include "calendar.h"
#include "catalua_luna.h"
#include "mission.h"
#include "type_id.h"
#include "concepts_utility.h"

enum Attitude : int;
enum body_part : int;
enum character_movemode : int;
enum class add_type : int;
enum color_id : int;
enum damage_type : int;
enum game_message_type : int;
enum m_flag : int;
enum creature_size : int;
enum mf_attitude : int;
enum monster_attitude : int;
enum npc_attitude : int;
enum npc_need : int;
enum mission_origin : int;
enum mission_goal : int;
enum art_charge : int;
enum art_charge_req : int;
enum art_effect_active : int;
enum art_effect_passive : int;
enum vitamin_type : int;
enum class ot_match_type : int;
enum moon_phase : int;

namespace sfx
{
enum class channel : int;
} // namespace sfx

class avatar;
class Character;
class character_id;
class Creature;
struct power_stat;
class distribution_grid;
class distribution_grid_tracker;
class effect;
class overmapbuffer;
class effect_type;
class item;
class item_stack;
class ma_technique;
class ma_buff;
class map;
class map_stack;
class material_type;
class mission;
class monster;
class npc;
class player;
class query_popup;
class recipe;
class Skill;
class SkillLevel;
class SkillLevelMap;
class spell_type;
class spell;
class string_input_popup;
class time_duration;
class time_point;
class tinymap;
class uilist;
class relic;
struct body_part_type;
struct damage_instance;
struct damage_unit;
struct dealt_damage_instance;
struct fake_spell;
struct field_type;
struct mutation_branch;
struct mission_type;
struct npc_opinion;
struct npc_personality;
struct omt_find_params;
struct oter_t;
struct point;
struct species_type;
struct tripoint;
struct trap;
struct MonsterGroup;
struct uilist_entry;
struct book_recipe;
class ammunition_type;
struct ammo_effect;
class martialart;
struct common_ranged_data;
struct MOD_INFORMATION;
class weapon_category;
class emit;
class fault;
struct quality;
struct resistances;
struct armor_portion_data;
class vitamin;
struct explosion_data;
struct requirement_data;
class inventory;
class known_magic;

namespace units
{
template<Arithmetic V, typename U>
class quantity;

class angle_in_radians_tag;
using angle = quantity<double, angle_in_radians_tag>;

class energy_in_joule_tag;
using energy = quantity<int, energy_in_joule_tag>;

class mass_in_milligram_tag;
using mass = quantity<std::int64_t, mass_in_milligram_tag>;

class volume_in_milliliter_tag;
using volume = quantity<int, volume_in_milliliter_tag>;
} // namespace units

struct islot_container;
struct islot_tool;
struct islot_comestible;
struct islot_brewable;
struct islot_armor;
struct islot_pet_armor;
struct islot_book;
struct islot_mod;
struct islot_engine;
struct islot_wheel;
struct islot_fuel;
struct islot_gun;
struct islot_gunmod;
struct islot_magazine;
struct islot_battery;
struct islot_bionic;
struct islot_ammo;
struct islot_artifact;
struct islot_seed;
class islot_milling;

// These definitions help the doc generator
LUNA_DOC( bool, "bool" );
LUNA_DOC( std::int16_t, "int" );
LUNA_DOC( std::uint16_t, "int" );
LUNA_DOC( std::int32_t, "int" );
LUNA_DOC( std::uint32_t, "int" );
LUNA_DOC( std::int64_t, "int" );
LUNA_DOC( std::uint64_t, "int" );
LUNA_DOC( float, "double" );
LUNA_DOC( double, "double" );
LUNA_DOC( void, "nil" );
LUNA_DOC( char, "char" );
LUNA_DOC( signed char, "char" );
LUNA_DOC( unsigned char, "char" );
LUNA_DOC( const char *, "string" );
LUNA_DOC( std::string, "string" );
LUNA_DOC( std::string_view, "string" );
LUNA_DOC( sol::lua_nil_t, "nil" );
LUNA_DOC( sol::variadic_args, "..." );
LUNA_DOC( sol::this_state, "<this_state>" );
LUNA_DOC( sol::protected_function, "function" );
LUNA_DOC( sol::table, "table" );


// These definitions are for the bindings generator
LUNA_VAL( avatar, "Avatar" );
LUNA_VAL( Character, "Character" );
LUNA_VAL( character_id, "CharacterId" );
LUNA_VAL( color_id, "Color" );
LUNA_VAL( Creature, "Creature" );
LUNA_VAL( damage_instance, "DamageInstance" );
LUNA_VAL( damage_unit, "DamageUnit" );
LUNA_VAL( dealt_damage_instance, "DealtDamageInstance" );
LUNA_VAL( distribution_grid, "DistributionGrid" );
LUNA_VAL( power_stat, "PowerStat" );
LUNA_VAL( distribution_grid_tracker, "DistributionGridTracker" );
LUNA_PTR_VAL( item, "Item" );
LUNA_VAL( item_stack, "ItemStack" );
LUNA_VAL( map, "Map" );
LUNA_VAL( map_stack, "MapStack" );
LUNA_VAL( mission, "Mission" );
LUNA_VAL( mission_type, "MissionType" );
LUNA_VAL( monster, "Monster" );
LUNA_VAL( npc, "Npc" );
LUNA_VAL( npc_opinion, "NpcOpinion" );
LUNA_VAL( npc_personality, "NpcPersonality" );
LUNA_VAL( omt_find_params, "OmtFindParams" );
LUNA_VAL( overmapbuffer, "OvermapBuffer" );
LUNA_VAL( player, "Player" );
LUNA_VAL( point, "Point" );
LUNA_VAL( string_input_popup, "PopupInputStr" );
LUNA_VAL( query_popup, "QueryPopup" );
LUNA_VAL( SkillLevelMap, "SkillLevelMap" );
LUNA_VAL( SkillLevel, "SkillLevel" );
LUNA_VAL( fake_spell, "SpellSimple" )
LUNA_VAL( spell, "Spell" )
LUNA_VAL( known_magic, "KnownMagic" )
LUNA_VAL( time_duration, "TimeDuration" );
LUNA_VAL( time_point, "TimePoint" );
LUNA_VAL( tinymap, "Tinymap" );
LUNA_VAL( tripoint, "Tripoint" );
LUNA_VAL( uilist, "UiList" );
LUNA_VAL( uilist_entry, "UiListEntry" );
LUNA_VAL( units::angle, "Angle" );
LUNA_VAL( units::energy, "Energy" );
LUNA_VAL( units::mass, "Mass" );
LUNA_VAL( units::volume, "Volume" );
LUNA_VAL( relic, "Relic" )
LUNA_VAL( book_recipe, "BookRecipe" );
LUNA_VAL( common_ranged_data, "RangedData" );
LUNA_VAL( resistances, "Resistances" );
LUNA_VAL( armor_portion_data, "ArmorPortionData" );
LUNA_VAL( effect, "Effect" );
LUNA_VAL( explosion_data, "ExplosionData" );
LUNA_VAL( requirement_data, "RequirementData" );
LUNA_VAL( inventory, "Inventory" );

// Ids for in-game objects
LUNA_ID( ammunition_type, "AmmunitionType" )
LUNA_ID( ammo_effect, "AmmunitionEffect" )
LUNA_ID( activity_type, "ActivityType" )
LUNA_ID( bionic_data, "BionicData" )
LUNA_ID( body_part_type, "BodyPartType" )
LUNA_ID( disease_type, "DiseaseType" )
LUNA_ID( effect_type, "EffectType" )
LUNA_ID( faction, "Faction" )
LUNA_ID( field_type, "FieldType" )
LUNA_ID( furn_t, "Furn" )
LUNA_ID( itype, "Itype" )
LUNA_ID( MOD_INFORMATION, "ModInfo" )
LUNA_ID( json_flag, "JsonFlag" )
LUNA_ID( json_trait_flag, "JsonTraitFlag" )
LUNA_ID( ma_buff, "MartialArtsBuff" )
LUNA_ID( ma_technique, "MartialArtsTechnique" )
LUNA_ID( martialart, "MartialArts" )
LUNA_ID( material_type, "MaterialType" )
LUNA_ID( monfaction, "MonsterFaction" )
LUNA_ID( morale_type_data, "MoraleTypeData" )
LUNA_ID( mission_type_id, "MissionTypeId" )
LUNA_ID( mtype, "MonsterType" )
LUNA_ID( mutation_branch, "MutationBranch" )
LUNA_ID( mutation_category_trait, "MutationCategoryTrait" )
LUNA_ID( oter_t, "Oter" )
LUNA_ID( recipe, "Recipe" )
LUNA_ID( Skill, "Skill" )
LUNA_ID( species_type, "SpeciesType" )
LUNA_ID( spell_type, "SpellType" )
LUNA_ID( ter_t, "Ter" )
LUNA_ID( trap, "Trap" )
LUNA_ID( MonsterGroup, "MonsterGroup" )
LUNA_ID( weapon_category, "WeaponCategory" )
LUNA_ID( emit, "FieldEmit" )
LUNA_ID( fault, "Fault" )
LUNA_ID( quality, "Quality" )
LUNA_ID( vitamin, "Vitamin" )

// Enums
LUNA_ENUM( add_type, "AddictionType" )
LUNA_ENUM( Attitude, "Attitude" )
LUNA_ENUM( body_part, "BodyPart" )
LUNA_ENUM( character_movemode, "CharacterMoveMode" )
LUNA_ENUM( damage_type, "DamageType" )
LUNA_ENUM( game_message_type, "MsgType" )
LUNA_ENUM( mf_attitude, "MonsterFactionAttitude" )
LUNA_ENUM( m_flag, "MonsterFlag" )
LUNA_ENUM( monster_attitude, "MonsterAttitude" )
LUNA_ENUM( creature_size, "MonsterSize" )
LUNA_ENUM( npc_attitude, "NpcAttitude" )
LUNA_ENUM( npc_need, "NpcNeed" )
LUNA_ENUM( ot_match_type, "OtMatchType" )
LUNA_ENUM( sfx::channel, "SfxChannel" )
LUNA_ENUM( mission_origin, "MissionOrigin" )
LUNA_ENUM( mission_goal, "MissionGoal" )
LUNA_ENUM( art_charge, "ArtifactCharge" )
LUNA_ENUM( art_charge_req, "ArtifactChargeReq" )
LUNA_ENUM( art_effect_active, "ArtifactEffectPassive" )
LUNA_ENUM( art_effect_passive, "ArtifactEffectActive" )
LUNA_ENUM( phase_id, "Phase" )
LUNA_ENUM( vitamin_type, "VitaminType" )
LUNA_ENUM( moon_phase, "MoonPhase" );

// ISlot
LUNA_VAL( islot_container, "IslotContainer" );
LUNA_VAL( islot_tool, "IslotTool" );
LUNA_VAL( islot_comestible, "IslotComestible" );
LUNA_VAL( islot_brewable, "IslotBrewable" );
LUNA_VAL( islot_armor, "IslotArmor" );
LUNA_VAL( islot_pet_armor, "IslotPetArmor" );
LUNA_VAL( islot_book, "IslotBook" );
LUNA_VAL( islot_mod, "IslotMod" );
LUNA_VAL( islot_engine, "IslotEngine" );
LUNA_VAL( islot_wheel, "IslotWheel" );
LUNA_VAL( islot_fuel, "IslotFuel" );
LUNA_VAL( islot_gun, "IslotGun" );
LUNA_VAL( islot_gunmod, "IslotGunmod" );
LUNA_VAL( islot_magazine, "IslotMagazine" );
LUNA_VAL( islot_battery, "IslotBattery" );
LUNA_VAL( islot_bionic, "IslotBionic" );
LUNA_VAL( islot_ammo, "IslotAmmo" );
LUNA_VAL( islot_artifact, "IslotArtifact" );
LUNA_VAL( islot_milling, "IslotMilling" );
LUNA_VAL( islot_seed, "IslotSeed" );
