#include "flag.h"

#include <unordered_map>

#include "debug.h"
#include "json.h"
#include "type_id.h"
#include "generic_factory.h"

const flag_id flag_NULL = flag_id( "null" ); // intentionally invalid flag

const flag_id flag_ACID( "ACID" );
const flag_id flag_ACID_IMMUNE( "ACID_IMMUNE" );
const flag_id flag_ACTIVE_CLOAKING( "ACTIVE_CLOAKING" );
const flag_id flag_ACT_IN_FIRE( "ACT_IN_FIRE" );
const flag_id flag_ACT_ON_RANGED_HIT( "ACT_ON_RANGED_HIT" );
const flag_id flag_ADD_UPS_TOGGLE( "ADD_UPS_TOGGLE" );
const flag_id flag_ALARMCLOCK( "ALARMCLOCK" );
const flag_id flag_ALLERGEN_EGG( "ALLERGEN_EGG" );
const flag_id flag_ALLERGEN_FRUIT( "ALLERGEN_FRUIT" );
const flag_id flag_ALLERGEN_JUNK( "ALLERGEN_JUNK" );
const flag_id flag_ALLERGEN_MEAT( "ALLERGEN_MEAT" );
const flag_id flag_ALLERGEN_MILK( "ALLERGEN_MILK" );
const flag_id flag_ALLERGEN_NUT( "ALLERGEN_NUT" );
const flag_id flag_ALLERGEN_VEGGY( "ALLERGEN_VEGGY" );
const flag_id flag_ALLERGEN_WHEAT( "ALLERGEN_WHEAT" );
const flag_id flag_ALLERGEN_WOOL( "ALLERGEN_WOOL" );
const flag_id flag_ALLOWS_NATURAL_ATTACKS( "ALLOWS_NATURAL_ATTACKS" );
const flag_id flag_ALLOWS_REMOTE_USE( "ALLOWS_REMOTE_USE" );
const flag_id flag_ALWAYS_TWOHAND( "ALWAYS_TWOHAND" );
const flag_id flag_AURA( "AURA" );
const flag_id flag_BAROMETER( "BAROMETER" );
const flag_id flag_BASH_IMMUNE( "BASH_IMMUNE" );
const flag_id flag_BELTED( "BELTED" );
const flag_id flag_BELT_CLIP( "BELT_CLIP" );
const flag_id flag_BIONIC_ARMOR_INTERFACE( "BIONIC_ARMOR_INTERFACE" );
const flag_id flag_BIONIC_FAULTY( "BIONIC_FAULTY" );
const flag_id flag_BIONIC_INSTALLATION_DATA( "BIONIC_INSTALLATION_DATA" );
const flag_id flag_BIO_IMMUNE( "BIO_IMMUNE" );
const flag_id flag_BIPOD( "BIPOD" );
const flag_id flag_BIRD( "BIRD" );
const flag_id flag_BLED( "BLED" );
const flag_id flag_BLIND( "BLIND" );
const flag_id flag_BLOCK_WHILE_WORN( "BLOCK_WHILE_WORN" );
const flag_id flag_BOMB( "BOMB" );
const flag_id flag_BOOK_CANNIBAL( "BOOK_CANNIBAL" );
const flag_id flag_BRASS_CATCHER( "BRASS_CATCHER" );
const flag_id flag_BULLET_IMMUNE( "BULLET_IMMUNE" );
const flag_id flag_BUTCHER_RACK( "BUTCHER_RACK" );
const flag_id flag_BYPRODUCT( "BYPRODUCT" );
const flag_id flag_CABLE_SPOOL( "CABLE_SPOOL" );
const flag_id flag_CAMERA_PRO( "CAMERA_PRO" );
const flag_id flag_CAN_PLANT_UNDERGROUND( "CAN_PLANT_UNDERGROUND" );
const flag_id flag_CANNIBAL( "CANNIBAL" );
const flag_id flag_CANT_HEAL_EVERYONE( "CANT_HEAL_EVERYONE" );
const flag_id flag_CARNIVORE_OK( "CARNIVORE_OK" );
const flag_id flag_CASING( "CASING" );
const flag_id flag_CATTLE( "CATTLE" );
const flag_id flag_CBM_SCANNED( "CBM_SCANNED" );
const flag_id flag_CHALLENGE( "CHALLENGE" );
const flag_id flag_CHARGEDIM( "CHARGEDIM" );
const flag_id flag_CITY_START( "CITY_START" );
const flag_id flag_CLIMATE_CONTROL( "CLIMATE_CONTROL" );
const flag_id flag_CLOSES_PORTAL( "CLOSES_PORTAL" );
const flag_id flag_COLD( "COLD" );
const flag_id flag_COLD_IMMUNE( "COLD_IMMUNE" );
const flag_id flag_COLLAPSIBLE_STOCK( "COLLAPSIBLE_STOCK" );
const flag_id flag_COLLAR( "COLLAR" );
const flag_id flag_COMBAT_NPC_USE( "COMBAT_NPC_USE" );
const flag_id flag_COMBAT_NPC_ON( "COMBAT_NPC_ON" );
const flag_id flag_COMPACT( "COMPACT" );
const flag_id flag_CONDUCTIVE( "CONDUCTIVE" );
const flag_id flag_CONSUMABLE( "CONSUMABLE" );
const flag_id flag_COOKED( "COOKED" );
const flag_id flag_CORPSE( "CORPSE" );
const flag_id flag_CUSTOM_EXPLOSION( "CUSTOM_EXPLOSION" );
const flag_id flag_CUT_IMMUNE( "CUT_IMMUNE" );
const flag_id flag_DANGEROUS( "DANGEROUS" );
const flag_id flag_DARK_IMMUNE( "DARK_IMMUNE" );
const flag_id flag_DEAF( "DEAF" );
const flag_id flag_DESTROY_ON_DECHARGE( "DESTROY_ON_DECHARGE" );
const flag_id flag_DIAMOND( "DIAMOND" );
const flag_id flag_DIG_TOOL( "DIG_TOOL" );
const flag_id flag_DIMENSIONAL_ANCHOR( "DIMENSIONAL_ANCHOR" );
const flag_id flag_DIRTY( "DIRTY" );
const flag_id flag_DISABLE_SIGHTS( "DISABLE_SIGHTS" );
const flag_id flag_DROP_ACTION_ONLY_IF_LIQUID( "DROP_ACTION_ONLY_IF_LIQUID" );
const flag_id flag_DURABLE_MELEE( "DURABLE_MELEE" );
const flag_id flag_EATEN_COLD( "EATEN_COLD" );
const flag_id flag_EATEN_HOT( "EATEN_HOT" );
const flag_id flag_EFFECT_ACID_IMMUNE( "EFFECT_ACID_IMMUNE" );
const flag_id flag_EFFECT_BASH_IMMUNE( "EFFECT_BASH_IMMUNE" );
const flag_id flag_EFFECT_BIO_IMMUNE( "EFFECT_BIO_IMMUNE" );
const flag_id flag_EFFECT_BULLET_IMMUNE( "EFFECT_BULLET_IMMUNE" );
const flag_id flag_EFFECT_CLAIRVOYANCE( "EFFECT_CLAIRVOYANCE" );
const flag_id flag_EFFECT_CLAIRVOYANCE_PLUS( "EFFECT_CLAIRVOYANCE_PLUS" );
const flag_id flag_EFFECT_COLD_IMMUNE( "EFFECT_COLD_IMMUNE" );
const flag_id flag_EFFECT_CUT_IMMUNE( "EFFECT_CUT_IMMUNE" );
const flag_id flag_EFFECT_DARK_IMMUNE( "EFFECT_DARK_IMMUNE" );
const flag_id flag_EFFECT_ELECTRIC_IMMUNE( "EFFECT_ELECTRIC_IMMUNE" );
const flag_id flag_EFFECT_FEATHER_FALL( "EFFECT_FEATHER_FALL" );
const flag_id flag_EFFECT_HEAT_IMMUNE( "EFFECT_HEAT_IMMUNE" );
const flag_id flag_EFFECT_IMPEDING( "EFFECT_IMPEDING" );
const flag_id flag_EFFECT_INVISIBLE( "EFFECT_INVISIBLE" );
const flag_id flag_EFFECT_LIGHT_IMMUNE( "EFFECT_LIGHT_IMMUNE" );
const flag_id flag_EFFECT_LUA_ON_ADDED( "EFFECT_LUA_ON_ADDED" );
const flag_id flag_EFFECT_LUA_ON_TICK( "EFFECT_LUA_ON_TICK" );
const flag_id flag_EFFECT_LUA_ON_REMOVED( "EFFECT_LUA_ON_REMOVED" );
const flag_id flag_EFFECT_NIGHT_VISION( "EFFECT_NIGHT_VISION" );
const flag_id flag_EFFECT_PSI_IMMUNE( "EFFECT_PSI_IMMUNE" );
const flag_id flag_EFFECT_STAB_IMMUNE( "EFFECT_STAB_IMMUNE" );
const flag_id flag_EFFECT_SUPER_CLAIRVOYANCE( "EFFECT_SUPER_CLAIRVOYANCE" );
const flag_id flag_ELECTRIC_IMMUNE( "ELECTRIC_IMMUNE" );
const flag_id flag_ETHEREAL_ITEM( "ETHEREAL_ITEM" );
const flag_id flag_EXPLOSION_SMASHED( "EXPLOSION_SMASHED" );
const flag_id flag_EXPLOSION_PROPELLED( "EXPLOSION_PROPELLED" );
const flag_id flag_FAKE_MILL( "FAKE_MILL" );
const flag_id flag_FAKE_CLONING_VAT( "FAKE_CLONING_VAT" );
const flag_id flag_FAKE_SMOKE( "FAKE_SMOKE" );
const flag_id flag_FANCY( "FANCY" );
const flag_id flag_FELINE( "FELINE" );
const flag_id flag_FERTILIZER( "FERTILIZER" );
const flag_id flag_FIELD_DRESS( "FIELD_DRESS" );
const flag_id flag_FIELD_DRESS_FAILED( "FIELD_DRESS_FAILED" );
const flag_id flag_FIN( "FIN" );
const flag_id flag_FIRE( "FIRE" );
const flag_id flag_FIRESTARTER( "FIRESTARTER" );
const flag_id flag_FIREWOOD( "FIREWOOD" );
const flag_id flag_FIRE_100( "FIRE_100" );
const flag_id flag_FIRE_20( "FIRE_20" );
const flag_id flag_FIRE_50( "FIRE_50" );
const flag_id flag_HEAVY_WEAPON_SUPPORT( "HEAVY_WEAPON_SUPPORT" );
const flag_id flag_FIRE_TWOHAND( "FIRE_TWOHAND" );
const flag_id flag_FISH_GOOD( "FISH_GOOD" );
const flag_id flag_FISH_POOR( "FISH_POOR" );
const flag_id flag_FIT( "FIT" );
const flag_id flag_FIX_FARSIGHT( "FIX_FARSIGHT" );
const flag_id flag_FIX_NEARSIGHT( "FIX_NEARSIGHT" );
const flag_id flag_FLAMING( "FLAMING" );
const flag_id flag_FLASH_PROTECTION( "FLASH_PROTECTION" );
const flag_id flag_FLAT_SURFACE( "FLAT_SURFACE" );
const flag_id flag_FLOTATION( "FLOTATION" );
const flag_id flag_FORAGE_HALLU( "FORAGE_HALLU" );
const flag_id flag_FORAGE_POISON( "FORAGE_POISON" );
const flag_id flag_FRAGILE( "FRAGILE" );
const flag_id flag_FRAGILE_MELEE( "FRAGILE_MELEE" );
const flag_id flag_FROZEN( "FROZEN" );
const flag_id flag_FUNGAL_VECTOR( "FUNGAL_VECTOR" );
const flag_id flag_GAS_DISCOUNT( "GAS_DISCOUNT" );
const flag_id flag_GAS_PROOF( "GAS_PROOF" );
const flag_id flag_GIBBED( "GIBBED" );
const flag_id flag_GNV_EFFECT( "GNV_EFFECT" );
const flag_id flag_HEATS_FOOD( "HEATS_FOOD" );
const flag_id flag_HEATS_FOOD_IS_CONSUMED( "HEATS_FOOD_IS_CONSUMED" );
const flag_id flag_HEATS_FOOD_USING_CHARGES( "HEATS_FOOD_USING_CHARGES" );
const flag_id flag_HEATS_FOOD_USING_FIRE( "HEATS_FOOD_USING_FIRE" );
const flag_id flag_HEAT_IMMUNE( "HEAT_IMMUNE" );
const flag_id flag_HELMET_COMPAT( "HELMET_COMPAT" );
const flag_id flag_HIDDEN_HALLU( "HIDDEN_HALLU" );
const flag_id flag_HIDDEN_ITEM( "HIDDEN_ITEM" );
const flag_id flag_HIDDEN_POISON( "HIDDEN_POISON" );
const flag_id flag_HOOD( "HOOD" );
const flag_id flag_HOT( "HOT" );
const flag_id flag_HURT_WHEN_WIELDED( "HURT_WHEN_WIELDED" );
const flag_id flag_HYGROMETER( "HYGROMETER" );
const flag_id flag_INEDIBLE( "INEDIBLE" );
const flag_id flag_INITIAL_PART( "INITIAL_PART" );
const flag_id flag_INSPIRATIONAL( "INSPIRATIONAL" );
const flag_id flag_INSTALL_DIFFICULT( "INSTALL_DIFFICULT" );
const flag_id flag_IN_CBM( "IN_CBM" );
const flag_id flag_IRREMOVABLE( "IRREMOVABLE" );
const flag_id flag_IR_EFFECT( "IR_EFFECT" );
const flag_id flag_IS_ARMOR( "IS_ARMOR" );
const flag_id flag_IS_BLOOD( "IS_BLOOD" );
const flag_id flag_IS_EXPLOSION_PROPELLED( "IS_EXPLOSION_PROPELLED" );
const flag_id flag_IS_PET_ARMOR( "IS_PET_ARMOR" );
const flag_id flag_IS_UPS( "IS_UPS" );
const flag_id flag_LEAK_ALWAYS( "LEAK_ALWAYS" );
const flag_id flag_LEAK_DAM( "LEAK_DAM" );
const flag_id flag_LIGHT_IMMUNE( "LIGHT_IMMUNE" );
const flag_id flag_LITCIG( "LITCIG" );
const flag_id flag_LUPINE( "LUPINE" );
const flag_id flag_MAGIC_FOCUS( "MAGIC_FOCUS" );
const flag_id flag_MAG_BELT( "MAG_BELT" );
const flag_id flag_MAG_BULKY( "MAG_BULKY" );
const flag_id flag_MAG_DESTROY( "MAG_DESTROY" );
const flag_id flag_MAG_EJECT( "MAG_EJECT" );
const flag_id flag_MC_ENCRYPTED( "MC_ENCRYPTED" );
const flag_id flag_MC_HAS_DATA( "MC_HAS_DATA" );
const flag_id flag_MC_MAY_BE_ENCRYPTED( "MC_MAY_BE_ENCRYPTED" );
const flag_id flag_MC_MOBILE( "MC_MOBILE" );
const flag_id flag_MC_RANDOM_STUFF( "MC_RANDOM_STUFF" );
const flag_id flag_MC_SCIENCE_STUFF( "MC_SCIENCE_STUFF" );
const flag_id flag_MC_TURN_USED( "MC_TURN_USED" );
const flag_id flag_MC_USED( "MC_USED" );
const flag_id flag_MECH_BAT( "MECH_BAT" );
const flag_id flag_MELEE_GUNMOD( "MELEE_GUNMOD" );
const flag_id flag_MELTS( "MELTS" );
const flag_id flag_MESSY( "MESSY" );
const flag_id flag_MISSION_ITEM( "MISSION_ITEM" );
const flag_id flag_MORBID( "MORBID" );
const flag_id flag_MOUNTED_GUN( "MOUNTED_GUN" );
const flag_id flag_MYCUS_OK( "MYCUS_OK" );
const flag_id flag_NANOFAB_TEMPLATE( "NANOFAB_TEMPLATE" );
const flag_id flag_NAT_UPS( "NAT_UPS" );
const flag_id flag_NEEDS_NO_LUBE( "NEEDS_NO_LUBE" );
const flag_id flag_NEEDS_UNFOLD( "NEEDS_UNFOLD" );
const flag_id flag_NEGATIVE_MONOTONY_OK( "NEGATIVE_MONOTONY_OK" );
const flag_id flag_NEVER_JAMS( "NEVER_JAMS" );
const flag_id flag_NONCONDUCTIVE( "NONCONDUCTIVE" );
const flag_id flag_NON_FOULING( "NON_FOULING" );
const flag_id flag_NO_BLOAT( "NO_BLOAT" );
const flag_id flag_NO_CVD( "NO_CVD" );
const flag_id flag_NO_DAMAGE( "NO_DAMAGE" );
const flag_id flag_NO_DISPLAY( "NO_DISPLAY" );
const flag_id flag_NO_DROP( "NO_DROP" );
const flag_id flag_NO_INGEST( "NO_INGEST" );
const flag_id flag_NO_PACKED( "NO_PACKED" );
const flag_id flag_NO_PARASITES( "NO_PARASITES" );
const flag_id flag_NO_QUICKDRAW( "NO_QUICKDRAW" );
const flag_id flag_NO_RECOVER_AMMO( "NO_RECOVER_AMMO" );
const flag_id flag_NO_RELOAD( "NO_RELOAD" );
const flag_id flag_NO_REPAIR( "NO_REPAIR" );
const flag_id flag_NO_SALVAGE( "NO_SALVAGE" );
const flag_id flag_NO_STERILE( "NO_STERILE" );
const flag_id flag_NO_TAKEOFF( "NO_TAKEOFF" );
const flag_id flag_NO_UNLOAD( "NO_UNLOAD" );
const flag_id flag_NO_UNWIELD( "NO_UNWIELD" );
const flag_id flag_NPC_ACTIVATE( "NPC_ACTIVATE" );
const flag_id flag_NPC_ALT_ATTACK( "NPC_ALT_ATTACK" );
const flag_id flag_NPC_SAFE( "NPC_SAFE" );
const flag_id flag_NPC_THROWN( "NPC_THROWN" );
const flag_id flag_NPC_THROW_NOW( "NPC_THROW_NOW" );
const flag_id flag_NUTRIENT_OVERRIDE( "NUTRIENT_OVERRIDE" );
const flag_id flag_ONLY_ONE( "ONLY_ONE" );
const flag_id flag_EXOSUIT( "EXOSUIT" );
const flag_id flag_ORGANIC( "ORGANIC" );
const flag_id flag_OUTER( "OUTER" );
const flag_id flag_OVERSIZE( "OVERSIZE" );
const flag_id flag_PARTIAL_DEAF( "PARTIAL_DEAF" );
const flag_id flag_PERFECT_LOCKPICK( "PERFECT_LOCKPICK" );
const flag_id flag_PERPETUAL( "PERPETUAL" );
const flag_id flag_PERSONAL( "PERSONAL" );
const flag_id flag_PLACE_RANDOMLY( "PLACE_RANDOMLY" );
const flag_id flag_POCKETS( "POCKETS" );
const flag_id flag_POLEARM( "POLEARM" );
const flag_id flag_POWERARMOR_COMPATIBLE( "POWERARMOR_COMPATIBLE" );
const flag_id flag_POWERARMOR_EXO( "POWERARMOR_EXO" );
const flag_id flag_POWERARMOR_EXTERNAL( "POWERARMOR_EXTERNAL" );
const flag_id flag_POWERARMOR_MOD( "POWERARMOR_MOD" );
const flag_id flag_POWERED( "POWERED" );
const flag_id flag_PRIMITIVE_RANGED_WEAPON( "PRIMITIVE_RANGED_WEAPON" );
const flag_id flag_PROCESSING( "PROCESSING" );
const flag_id flag_PROCESSING_RESULT( "PROCESSING_RESULT" );
const flag_id flag_PSEUDO( "PSEUDO" );
const flag_id flag_PSI_IMMUNE( "PSI_IMMUNE" );
const flag_id flag_PSYSHIELD_PARTIAL( "PSYSHIELD_PARTIAL" );
const flag_id flag_PULPED( "PULPED" );
const flag_id flag_PUMP_ACTION( "PUMP_ACTION" );
const flag_id flag_PUMP_RAIL_COMPATIBLE( "PUMP_RAIL_COMPATIBLE" );
const flag_id flag_PYROMANIAC_WEAPON( "PYROMANIAC_WEAPON" );
const flag_id flag_QUARTERED( "QUARTERED" );
const flag_id flag_RADIOACTIVE( "RADIOACTIVE" );
const flag_id flag_RADIOCAR( "RADIOCAR" );
const flag_id flag_RADIOCARITEM( "RADIOCARITEM" );
const flag_id flag_RADIOSIGNAL_1( "RADIOSIGNAL_1" );
const flag_id flag_RADIOSIGNAL_2( "RADIOSIGNAL_2" );
const flag_id flag_RADIOSIGNAL_3( "RADIOSIGNAL_3" );
const flag_id flag_RADIO_ACTIVATION( "RADIO_ACTIVATION" );
const flag_id flag_RADIO_CONTROLLED( "RADIO_CONTROLLED" );
const flag_id flag_RADIO_INVOKE_PROC( "RADIO_INVOKE_PROC" );
const flag_id flag_RADIO_MOD( "RADIO_MOD" );
const flag_id flag_RADIO_MODABLE( "RADIO_MODABLE" );
const flag_id flag_RAD_PROOF( "RAD_PROOF" );
const flag_id flag_RAD_RESIST( "RAD_RESIST" );
const flag_id flag_RAINPROOF( "RAINPROOF" );
const flag_id flag_RAIN_PROTECT( "RAIN_PROTECT" );
const flag_id flag_RAW( "RAW" );
const flag_id flag_REACH( "REACH" );
const flag_id flag_REACH3( "REACH3" );
const flag_id flag_REACH_ATTACK( "REACH_ATTACK" );
const flag_id flag_REBREATHER( "REBREATHER" );
const flag_id flag_RECHARGE( "RECHARGE" );
const flag_id flag_REDUCED_BASHING( "REDUCED_BASHING" );
const flag_id flag_REDUCED_WEIGHT( "REDUCED_WEIGHT" );
const flag_id flag_RELOAD_AND_SHOOT( "RELOAD_AND_SHOOT" );
const flag_id flag_RELOAD_EJECT( "RELOAD_EJECT" );
const flag_id flag_RELOAD_ONE( "RELOAD_ONE" );
const flag_id flag_resized_large( "resized_large" );
const flag_id flag_resized_small( "resized_small" );
const flag_id flag_REQUIRES_BALANCE( "REQUIRES_BALANCE" );
const flag_id flag_REQUIRES_TINDER( "REQUIRES_TINDER" );
const flag_id flag_RESTRICT_HANDS( "RESTRICT_HANDS" );
const flag_id flag_REVIVE_SPECIAL( "REVIVE_SPECIAL" );
const flag_id flag_ROLLER_INLINE( "ROLLER_INLINE" );
const flag_id flag_ROLLER_ONE( "ROLLER_ONE" );
const flag_id flag_ROLLER_QUAD( "ROLLER_QUAD" );
const flag_id flag_SAFECRACK( "SAFECRACK" );
const flag_id flag_SEMITANGIBLE( "SEMITANGIBLE" );
const flag_id flag_SHATTERS( "SHATTERS" );
const flag_id flag_SHOCKING( "SHOCKING" );
const flag_id flag_ACIDIC( "ACIDIC" );
const flag_id flag_SILENT( "SILENT" );
const flag_id flag_SKINNED( "SKINNED" );
const flag_id flag_SKINTIGHT( "SKINTIGHT" );
const flag_id flag_SLEEP_AID( "SLEEP_AID" );
const flag_id flag_SLEEP_IGNORE( "SLEEP_IGNORE" );
const flag_id flag_SLOWS_MOVEMENT( "SLOWS_MOVEMENT" );
const flag_id flag_SLOWS_THIRST( "SLOWS_THIRST" );
const flag_id flag_SLOW_WIELD( "SLOW_WIELD" );
const flag_id flag_SMOKABLE( "SMOKABLE" );
const flag_id flag_SMOKED( "SMOKED" );
const flag_id flag_SNIPPET_NEEDS_LITERACY( "SNIPPET_NEEDS_LITERACY" );
const flag_id flag_SOLARPACK( "SOLARPACK" );
const flag_id flag_SOLARPACK_ON( "SOLARPACK_ON" );
const flag_id flag_SPAWN_FRIENDLY( "SPAWN_FRIENDLY" );
const flag_id flag_SPEAR( "SPEAR" );
const flag_id flag_SPEEDLOADER( "SPEEDLOADER" );
const flag_id flag_SPLINT( "SPLINT" );
const flag_id flag_STAB( "STAB" );
const flag_id flag_STAB_IMMUNE( "STAB_IMMUNE" );
const flag_id flag_STR_DRAW( "STR_DRAW" );
const flag_id flag_STR_RELOAD( "STR_RELOAD" );
const flag_id flag_STURDY( "STURDY" );
const flag_id flag_SUN_GLASSES( "SUN_GLASSES" );
const flag_id flag_SUPER_FANCY( "SUPER_FANCY" );
const flag_id flag_SWIM_GOGGLES( "SWIM_GOGGLES" );
const flag_id flag_TACK( "TACK" );
const flag_id flag_TANGLE( "TANGLE" );
const flag_id flag_TARDIS( "TARDIS" );
const flag_id flag_THERMOMETER( "THERMOMETER" );
const flag_id flag_TIE_UP( "TIE_UP" );
const flag_id flag_TINDER( "TINDER" );
const flag_id flag_TOBACCO( "TOBACCO" );
const flag_id flag_TOURNIQUET( "TOURNIQUET" );
const flag_id flag_TOW_CABLE( "TOW_CABLE" );
const flag_id flag_TRADER_AVOID( "TRADER_AVOID" );
const flag_id flag_TRADER_KEEP( "TRADER_KEEP" );
const flag_id flag_TRADER_KEEP_EQUIPPED( "TRADER_KEEP_EQUIPPED" );
const flag_id flag_TWO_WAY_RADIO( "TWO_WAY_RADIO" );
const flag_id flag_UNARMED_WEAPON( "UNARMED_WEAPON" );
const flag_id flag_UNBREAKABLE( "UNBREAKABLE" );
const flag_id flag_UNBREAKABLE_MELEE( "UNBREAKABLE_MELEE" );
const flag_id flag_UNDERSIZE( "UNDERSIZE" );
const flag_id flag_UNDERWATER_GUN( "UNDERWATER_GUN" );
const flag_id flag_UNRECOVERABLE( "UNRECOVERABLE" );
const flag_id flag_UNSAFE_CONSUME( "UNSAFE_CONSUME" );
const flag_id flag_URSINE_HONEY( "URSINE_HONEY" );
const flag_id flag_USES_BIONIC_POWER( "USES_BIONIC_POWER" );
const flag_id flag_USES_GRID_POWER( "USES_GRID_POWER" );
const flag_id flag_USE_EAT_VERB( "USE_EAT_VERB" );
const flag_id flag_USE_PARENT_GUN( "USE_PARENT_GUN" );
const flag_id flag_USE_PLAYER_ENERGY( "USE_PLAYER_ENERGY" );
const flag_id flag_USE_UPS( "USE_UPS" );
const flag_id flag_VARSIZE( "VARSIZE" );
const flag_id flag_VEHICLE( "VEHICLE" );
const flag_id flag_VERY_COLD( "VERY_COLD" );
const flag_id flag_WAIST( "WAIST" );
const flag_id flag_WATCH( "WATCH" );
const flag_id flag_WATERPROOF( "WATERPROOF" );
const flag_id flag_WATERPROOF_GUN( "WATERPROOF_GUN" );
const flag_id flag_WATER_EXTINGUISH( "WATER_EXTINGUISH" );
const flag_id flag_WATER_FRIENDLY( "WATER_FRIENDLY" );
const flag_id flag_WATER_DISABLE( "WATER_DISABLE" );
const flag_id flag_WET( "WET" );
const flag_id flag_WHIP( "WHIP" );
const flag_id flag_WIND_EXTINGUISH( "WIND_EXTINGUISH" );
const flag_id flag_WRITE_MESSAGE( "WRITE_MESSAGE" );
const flag_id flag_ZERO_WEIGHT( "ZERO_WEIGHT" );
const flag_id flag_ZOOM( "ZOOM" );
const flag_id flag_wooled( "wooled" );
const flag_id flag_MUTE( "MUTE" );
const flag_id flag_NOT_FOOTWEAR( "NOT_FOOTWEAR" );
const flag_id flag_WEATHER_FORECAST( "WEATHER_FORECAST" );
const flag_id flag_WINDMETER( "WINDMETER" );
const flag_id flag_INITIALLY_ACTIVATE( "INITIALLY_ACTIVATE" );
const flag_id flag_MULTIINSTALL( "MULTIINSTALL" );
const flag_id flag_BIONIC_TOOLS( "BIONIC_TOOLS" );
const flag_id flag_TEMPORARY_ITEM( "TEMPORARY_ITEM" );

namespace
{
generic_factory<json_flag> json_flags_all( "json_flags" );
} // namespace

/** @relates string_id */
template<>
bool flag_id ::is_valid() const
{
    return json_flags_all.is_valid( *this );
}

/** @relates string_id */
template<>
const json_flag &flag_id::obj() const
{
    return json_flags_all.obj( *this );
}

json_flag::operator bool() const
{
    return id.is_valid();
}

const json_flag &json_flag::get( const std::string &id )
{
    static const json_flag null_value = json_flag();
    const flag_id f_id( id );
    return f_id.is_valid() ? *f_id : null_value;
}

void json_flag::load( const JsonObject &jo, const std::string & )
{
    // TODO: mark fields as mandatory where appropriate
    optional( jo, was_loaded, "info", info_ );
    optional( jo, was_loaded, "conflicts", conflicts_ );
    optional( jo, was_loaded, "inherit", inherit_, true );
    optional( jo, was_loaded, "craft_inherit", craft_inherit_, false );
    optional( jo, was_loaded, "requires_flag", requires_flag_ );
    optional( jo, was_loaded, "taste_mod", taste_mod_ );
    optional( jo, was_loaded, "restriction", restriction_ );

    // FIXME: most flags have a "context" field that isn't used for anything
    // Test for it here to avoid errors about unvisited members
    jo.get_member( "context" );
}

void json_flag::check_consistency()
{
    json_flags_all.check();
}

void json_flag::reset()
{
    json_flags_all.reset();
}

void json_flag::load_all( const JsonObject &jo, const std::string &src )
{
    json_flags_all.load( jo, src );
}

void json_flag::check() const
{
    for( const auto &conflicting : conflicts_ ) {
        if( !flag_id( conflicting ).is_valid() ) {
            debugmsg( "flag definition %s specifies unknown conflicting field %s", id.str(),
                      conflicting );
        }
    }
}

void json_flag::finalize_all()
{
    json_flags_all.finalize();
}

bool json_flag::is_ready()
{
    return !json_flags_all.empty();
}

const std::vector<json_flag> &json_flag::get_all()
{
    return json_flags_all.get_all();
}
