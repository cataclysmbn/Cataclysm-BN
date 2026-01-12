#include "iuse.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "action.h"
#include "activity_actor.h"
#include "activity_actor_definitions.h"
#include "active_tile_data_def.h"
#include "animation.h"
#include "artifact.h"
#include "avatar.h"
#include "avatar_action.h"
#include "avatar_functions.h"
#include "bionics.h"
#include "bodypart.h"
#include "calendar.h"
#include "cata_utility.h"
#include "character.h"
#include "character_functions.h"
#include "character_martial_arts.h"
#include "color.h"
#include "coordinate_conversions.h"
#include "crafting.h"
#include "creature.h"
#include "damage.h"
#include "debug.h"
#include "distribution_grid.h"
#include "effect.h" // for weed_msg
#include "enums.h"
#include "event.h"
#include "event_bus.h"
#include "explosion.h"
#include "field.h"
#include "field_type.h"
#include "flag.h"
#include "fstream_utils.h"
#include "flat_set.h"
#include "fungal_effects.h"
#include "game.h"
#include "game_constants.h"
#include "game_inventory.h"
#include "handle_liquid.h"
#include "harvest.h"
#include "iexamine.h"
#include "int_id.h"
#include "inventory.h"
#include "item.h"
#include "item_contents.h"
#include "iteminfo_query.h"
#include "itype.h"
#include "iuse_actor.h" // For firestarter
#include "json.h"
#include "line.h"
#include "locations.h"
#include "map.h"
#include "map_iterator.h"
#include "map_selector.h"
#include "mapdata.h"
#include "martialarts.h"
#include "memorial_logger.h"
#include "memory_fast.h"
#include "messages.h"
#include "monattack.h"
#include "mongroup.h"
#include "monster.h"
#include "morale_types.h"
#include "mtype.h"
#include "mutation.h"
#include "npc.h"
#include "omdata.h"
#include "options.h"
#include "output.h"
#include "overmap.h"
#include "overmapbuffer.h"
#include "pimpl.h"
#include "player.h"
#include "player_activity.h"
#include "pldata.h"
#include "point.h"
#include "recipe.h"
#include "recipe_dictionary.h"
#include "requirements.h"
#include "ret_val.h"
#include "rng.h"
#include "sounds.h"
#include "speech.h"
#include "string_formatter.h"
#include "string_id.h"
#include "string_input_popup.h"
#include "string_utils.h"
#include "teleport.h"
#include "text_snippets.h"
#include "timed_event.h"
#include "translations.h"
#include "trap.h"
#include "type_id.h"
#include "ui.h"
#include "units_utility.h"
#include "value_ptr.h"
#include "veh_type.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vehicle_selector.h"
#include "visitable.h"
#include "vpart_position.h"
#include "vpart_range.h"
#include "weather.h"
#include "weather_gen.h"

static const activity_id ACT_BURROW( "ACT_BURROW" );
static const activity_id ACT_CHOP_LOGS( "ACT_CHOP_LOGS" );
static const activity_id ACT_CHOP_PLANKS( "ACT_CHOP_PLANKS" );
static const activity_id ACT_CHOP_TREE( "ACT_CHOP_TREE" );
static const activity_id ACT_CHURN( "ACT_CHURN" );
static const activity_id ACT_CLEAR_RUBBLE( "ACT_CLEAR_RUBBLE" );
static const activity_id ACT_CRAFT( "ACT_CRAFT" );
static const activity_id ACT_FILL_PIT( "ACT_FILL_PIT" );
static const activity_id ACT_FISH( "ACT_FISH" );
static const activity_id ACT_GAME( "ACT_GAME" );
static const activity_id ACT_GENERIC_GAME( "ACT_GENERIC_GAME" );
static const activity_id ACT_HAIRCUT( "ACT_HAIRCUT" );
static const activity_id ACT_HAND_CRANK( "ACT_HAND_CRANK" );
static const activity_id ACT_JACKHAMMER( "ACT_JACKHAMMER" );
static const activity_id ACT_MEDITATE( "ACT_MEDITATE" );
static const activity_id ACT_MIND_SPLICER( "ACT_MIND_SPLICER" );
static const activity_id ACT_PICKAXE( "ACT_PICKAXE" );
static const activity_id ACT_PRY_NAILS( "ACT_PRY_NAILS" );
static const activity_id ACT_ROBOT_CONTROL( "ACT_ROBOT_CONTROL" );
static const activity_id ACT_SHAVE( "ACT_SHAVE" );
static const activity_id ACT_VIBE( "ACT_VIBE" );

static const efftype_id effect_adrenaline( "adrenaline" );
static const efftype_id effect_antibiotic( "antibiotic" );
static const efftype_id effect_asthma( "asthma" );
static const efftype_id effect_attention( "attention" );
static const efftype_id effect_beartrap( "beartrap" );
static const efftype_id effect_bleed( "bleed" );
static const efftype_id effect_blind( "blind" );
static const efftype_id effect_bloodworms( "bloodworms" );
static const efftype_id effect_boomered( "boomered" );
static const efftype_id effect_bouldering( "bouldering" );
static const efftype_id effect_brainworms( "brainworms" );
static const efftype_id effect_cig( "cig" );
static const efftype_id effect_contacts( "contacts" );
static const efftype_id effect_corroding( "corroding" );
static const efftype_id effect_crushed( "crushed" );
static const efftype_id effect_datura( "datura" );
static const efftype_id effect_dazed( "dazed" );
static const efftype_id effect_well_fed( "well_fed" );
static const efftype_id effect_dermatik( "dermatik" );
static const efftype_id effect_docile( "docile" );
static const efftype_id effect_downed( "downed" );
static const efftype_id effect_drunk( "drunk" );
static const efftype_id effect_earphones( "earphones" );
static const efftype_id effect_foodpoison( "foodpoison" );
static const efftype_id effect_formication( "formication" );
static const efftype_id effect_fungus( "fungus" );
static const efftype_id effect_glowing( "glowing" );
static const efftype_id effect_glowy_led( "glowy_led" );
static const efftype_id effect_hallu( "hallu" );
static const efftype_id effect_happy( "happy" );
static const efftype_id effect_harnessed( "harnessed" );
static const efftype_id effect_has_bag( "has_bag" );
static const efftype_id effect_haslight( "haslight" );
static const efftype_id effect_in_pit( "in_pit" );
static const efftype_id effect_infected( "infected" );
static const efftype_id effect_jetinjector( "jetinjector" );
static const efftype_id effect_lack_sleep( "lack_sleep" );
static const efftype_id effect_laserlocked( "laserlocked" );
static const efftype_id effect_lying_down( "lying_down" );
static const efftype_id effect_melatonin_supplements( "melatonin" );
static const efftype_id effect_meth( "meth" );
static const efftype_id effect_monster_armor( "monster_armor" );
static const efftype_id effect_music( "music" );
static const efftype_id effect_onfire( "onfire" );
static const efftype_id effect_paincysts( "paincysts" );
static const efftype_id effect_pet( "pet" );
static const efftype_id effect_poison( "poison" );
static const efftype_id effect_ridden( "ridden" );
static const efftype_id effect_riding( "riding" );
static const efftype_id effect_run( "run" );
static const efftype_id effect_sad( "sad" );
static const efftype_id effect_saddled( "monster_saddled" );
static const efftype_id effect_sap( "sap" );
static const efftype_id effect_shakes( "shakes" );
static const efftype_id effect_sleep( "sleep" );
static const efftype_id effect_slimed( "slimed" );
static const efftype_id effect_smoke( "smoke" );
static const efftype_id effect_spores( "spores" );
static const efftype_id effect_stimpack( "stimpack" );
static const efftype_id effect_strong_antibiotic( "strong_antibiotic" );
static const efftype_id effect_stunned( "stunned" );
static const efftype_id effect_tapeworm( "tapeworm" );
static const efftype_id effect_teargas( "teargas" );
static const efftype_id effect_teleglow( "teleglow" );
static const efftype_id effect_tied( "tied" );
static const efftype_id effect_took_antiasthmatic( "took_antiasthmatic" );
static const efftype_id effect_took_flumed( "took_flumed" );
static const efftype_id effect_took_prozac( "took_prozac" );
static const efftype_id effect_took_prozac_bad( "took_prozac_bad" );
static const efftype_id effect_took_thorazine( "took_thorazine" );
static const efftype_id effect_took_xanax( "took_xanax" );
static const efftype_id effect_valium( "valium" );
static const efftype_id effect_visuals( "visuals" );
static const efftype_id effect_weak_antibiotic( "weak_antibiotic" );
static const efftype_id effect_webbed( "webbed" );
static const efftype_id effect_weed_high( "weed_high" );

static const itype_id itype_advanced_ecig( "advanced_ecig" );
static const itype_id itype_afs_atomic_smartphone( "afs_atomic_smartphone" );
static const itype_id itype_afs_atomic_smartphone_music( "afs_atomic_smartphone_music" );
static const itype_id itype_afs_wraitheon_smartphone( "afs_wraitheon_smartphone" );
static const itype_id itype_afs_atomic_wraitheon_music( "afs_atomic_wraitheon_music" );
static const itype_id itype_apparatus( "apparatus" );
static const itype_id itype_arrow_flamming( "arrow_flamming" );
static const itype_id itype_battery( "battery" );
static const itype_id itype_barometer( "barometer" );
static const itype_id itype_c4armed( "c4armed" );
static const itype_id itype_canister_empty( "canister_empty" );
static const itype_id itype_cig( "cig" );
static const itype_id itype_cigar( "cigar" );
static const itype_id itype_cow_bell( "cow_bell" );
static const itype_id itype_data_card( "data_card" );
static const itype_id itype_e_handcuffs( "e_handcuffs" );
static const itype_id itype_ecig( "ecig" );
static const itype_id itype_fire( "fire" );
static const itype_id itype_firecracker_act( "firecracker_act" );
static const itype_id itype_firecracker_pack_act( "firecracker_pack_act" );
static const itype_id itype_geiger_off( "geiger_off" );
static const itype_id itype_geiger_on( "geiger_on" );
static const itype_id itype_debug_grenade_act( "debug_grenade_act" );
static const itype_id itype_handrolled_cig( "handrolled_cig" );
static const itype_id itype_hygrometer( "hygrometer" );
static const itype_id itype_joint( "joint" );
static const itype_id itype_log( "log" );
static const itype_id itype_mask_h20survivor_on( "mask_h20survivor_on" );
static const itype_id itype_mininuke_act( "mininuke_act" );
static const itype_id itype_mobile_memory_card( "mobile_memory_card" );
static const itype_id itype_mobile_memory_card_used( "mobile_memory_card_used" );
static const itype_id itype_mp3( "mp3" );
static const itype_id itype_mp3_on( "mp3_on" );
static const itype_id itype_multi_cooker( "multi_cooker" );
static const itype_id itype_multi_cooker_filled( "multi_cooker_filled" );
static const itype_id itype_nicotine_liquid( "nicotine_liquid" );
static const itype_id itype_noise_emitter( "noise_emitter" );
static const itype_id itype_noise_emitter_on( "noise_emitter_on" );
static const itype_id itype_radio( "radio" );
static const itype_id itype_radio_car( "radio_car" );
static const itype_id itype_radio_car_on( "radio_car_on" );
static const itype_id itype_radio_on( "radio_on" );
static const itype_id itype_rebreather_on( "rebreather_on" );
static const itype_id itype_rebreather_xl_on( "rebreather_xl_on" );
static const itype_id itype_rmi2_corpse( "rmi2_corpse" );
static const itype_id itype_smart_phone( "smart_phone" );
static const itype_id itype_smartphone_music( "smartphone_music" );
static const itype_id itype_soldering_iron( "soldering_iron" );
static const itype_id itype_spiral_stone( "spiral_stone" );
static const itype_id itype_thermometer( "thermometer" );
static const itype_id itype_towel( "towel" );
static const itype_id itype_towel_soiled( "towel_soiled" );
static const itype_id itype_towel_wet( "towel_wet" );
static const itype_id itype_UPS( "UPS" );
static const itype_id itype_water( "water" );
static const itype_id itype_water_clean( "water_clean" );
static const itype_id itype_wax( "wax" );
static const itype_id itype_weather_reader( "weather_reader" );

static const skill_id skill_computer( "computer" );
static const skill_id skill_cooking( "cooking" );
static const skill_id skill_electronics( "electronics" );
static const skill_id skill_fabrication( "fabrication" );
static const skill_id skill_firstaid( "firstaid" );
static const skill_id skill_mechanics( "mechanics" );
static const skill_id skill_melee( "melee" );
static const skill_id skill_survival( "survival" );

static const trait_id trait_ACIDBLOOD( "ACIDBLOOD" );
static const trait_id trait_ACIDPROOF( "ACIDPROOF" );
static const trait_id trait_ALCMET( "ALCMET" );
static const trait_id trait_CHLOROMORPH( "CHLOROMORPH" );
static const trait_id trait_EATDEAD( "EATDEAD" );
static const trait_id trait_GILLS( "GILLS" );
static const trait_id trait_GILLS_CEPH( "GILLS_CEPH" );
static const trait_id trait_HYPEROPIC( "HYPEROPIC" );
static const trait_id trait_ILLITERATE( "ILLITERATE" );
static const trait_id trait_LIGHTWEIGHT( "LIGHTWEIGHT" );
static const trait_id trait_M_DEPENDENT( "M_DEPENDENT" );
static const trait_id trait_MARLOSS( "MARLOSS" );
static const trait_id trait_MARLOSS_AVOID( "MARLOSS_AVOID" );
static const trait_id trait_MARLOSS_BLUE( "MARLOSS_BLUE" );
static const trait_id trait_MARLOSS_YELLOW( "MARLOSS_YELLOW" );
static const trait_id trait_MYOPIC( "MYOPIC" );
static const trait_id trait_NOPAIN( "NOPAIN" );
static const trait_id trait_POISRESIST( "POISRESIST" );
static const trait_id trait_PROF_FERAL( "PROF_FERAL" );
static const trait_id trait_PSYCHOPATH( "PSYCHOPATH" );
static const trait_id trait_PYROMANIA( "PYROMANIA" );
static const trait_id trait_SAPROVORE( "SAPROVORE" );
static const trait_id trait_SPIRITUAL( "SPIRITUAL" );
static const trait_id trait_THRESH_MARLOSS( "THRESH_MARLOSS" );
static const trait_id trait_THRESH_MYCUS( "THRESH_MYCUS" );
static const trait_id trait_THRESH_PLANT( "THRESH_PLANT" );
static const trait_id trait_TOLERANCE( "TOLERANCE" );
static const trait_id trait_URSINE_EYE( "URSINE_EYE" );
static const trait_id trait_WAYFARER( "WAYFARER" );

static const quality_id qual_AXE( "AXE" );
static const quality_id qual_DIG( "DIG" );
static const quality_id qual_LOCKPICK( "LOCKPICK" );

static const requirement_id requirement_add_grid_connection =
    requirement_id( "add_grid_connection" );

static const species_id FUNGUS( "FUNGUS" );
static const species_id HALLUCINATION( "HALLUCINATION" );
static const species_id INSECT( "INSECT" );
static const species_id ZOMBIE( "ZOMBIE" );

static const mongroup_id GROUP_FISH( "GROUP_FISH" );

static const mtype_id mon_bee( "mon_bee" );
static const mtype_id mon_blob( "mon_blob" );
static const mtype_id mon_dog_thing( "mon_dog_thing" );
static const mtype_id mon_fly( "mon_fly" );
static const mtype_id mon_hologram( "mon_hologram" );
static const mtype_id mon_shadow( "mon_shadow" );
static const mtype_id mon_spore( "mon_spore" );
static const mtype_id mon_vortex( "mon_vortex" );
static const mtype_id mon_wasp( "mon_wasp" );

static const bionic_id bio_digestion( "bio_digestion" );
static const bionic_id bio_eye_optic( "bio_eye_optic" );
static const bionic_id bio_shock( "bio_shock" );

// terrain/furn flags
static const std::string flag_CURRENT( "CURRENT" );
static const std::string flag_FISHABLE( "FISHABLE" );
static const std::string flag_PLANT( "PLANT" );
static const std::string flag_PLOWABLE( "PLOWABLE" );

// how many characters per turn of radio
static constexpr int RADIO_PER_TURN = 25;

#include "iuse_software.h"


struct object_names_collection;

struct extended_photo_def : public JsonDeserializer, public JsonSerializer {
    int quality;
    std::string name;
    std::string description;

    extended_photo_def() = default;
    void deserialize( JsonIn &jsin ) override {
        JsonObject obj = jsin.get_object();
        quality = obj.get_int( "quality" );
        name = obj.get_string( "name" );
        description = obj.get_string( "description" );
    }

    void serialize( JsonOut &jsout ) const override {
        jsout.start_object();
        jsout.member( "quality", quality );
        jsout.member( "name", name );
        jsout.member( "description", description );
        jsout.end_object();
    }
};

static void item_save_monsters( player &p, item &it, const std::vector<monster *> &monster_vec,
                                int photo_quality );
static bool show_photo_selection( player &p, item &it, const std::string &var_name );

static bool item_read_extended_photos( item &, std::vector<extended_photo_def> &,
                                       const std::string &,
                                       bool = false );
static void item_write_extended_photos( item &, const std::vector<extended_photo_def> &,
                                        const std::string & );

static std::string format_object_pair( const std::pair<std::string, int> &pair,
                                       const std::string &article );
static std::string format_object_pair_article( const std::pair<std::string, int> &pair );
static std::string format_object_pair_no_article( const std::pair<std::string, int> &pair );

static std::string colorized_field_description_at( const tripoint &point );
static std::string colorized_trap_name_at( const tripoint &point );
static std::string colorized_ter_name_flags_at( const tripoint &point,
        const std::vector<std::string> &flags = {}, const std::vector<ter_str_id> &ter_whitelist = {} );
static std::string colorized_feature_description_at( const tripoint &center_point, bool &item_found,
        const units::volume &min_visible_volume );

static std::string colorized_item_name( const item &item );
static std::string colorized_item_description( const item &item );
static const item &get_top_item_at_point( const tripoint &point,
        const units::volume &min_visible_volume );

static std::string effects_description_for_creature( Creature *creature, std::string &pose,
        const std::string &pronoun_sex );

static object_names_collection enumerate_objects_around_point( const tripoint &point,
        int radius, const tripoint &bounds_center_point, int bounds_radius,
        const tripoint &camera_pos, const units::volume &min_visible_volume, bool create_figure_desc,
        std::unordered_set<tripoint> &ignored_points,
        std::unordered_set<const vehicle *> &vehicles_recorded );
static extended_photo_def photo_def_for_camera_point( const tripoint &aim_point,
        const tripoint &camera_pos,
        std::vector<monster *> &monster_vec, std::vector<player *> &player_vec );

static const std::vector<std::string> camera_ter_whitelist_flags = {
    "HIDE_PLACE", "FUNGUS", "TREE", "PERMEABLE", "SHRUB",
    "PLACE_ITEM", "GROWTH_HARVEST", "GROWTH_MATURE", "GOES_UP",
    "GOES_DOWN", "RAMP", "SHARP", "SIGN", "CLIMBABLE"
};
static const std::vector<ter_str_id> camera_ter_whitelist_types = {
    ter_str_id( "t_pit_covered" ), ter_str_id( "t_grave_new" ), ter_str_id( "t_grave" ), ter_str_id( "t_pit" ),
    ter_str_id( "t_pit_shallow" ), ter_str_id( "t_pit_corpsed" ), ter_str_id( "t_pit_spiked" ),
    ter_str_id( "t_pit_spiked_covered" ), ter_str_id( "t_pit_glass" ), ter_str_id( "t_pit_glass" ), ter_str_id( "t_utility_light" )
};

void remove_radio_mod( item &it, player &p )
{
    if( !it.has_flag( flag_RADIO_MOD ) ) {
        return;
    }
    p.add_msg_if_player( _( "You remove the radio modification from your %s!" ), it.tname() );
    p.i_add_or_drop( item::spawn( "radio_mod" ) );
    it.unset_flag( flag_RADIO_ACTIVATION );
    it.unset_flag( flag_RADIO_MOD );
    it.unset_flag( flag_RADIOSIGNAL_1 );
    it.unset_flag( flag_RADIOSIGNAL_2 );
    it.unset_flag( flag_RADIOSIGNAL_3 );
    it.unset_flag( flag_RADIOCARITEM );
}

/* iuse methods return the number of charges expended, which is usually it->charges_to_use().
 * Some items that don't normally use charges return 1 to indicate they're used up.
 * Regardless, returning 0 indicates the item has not been used up,
 * though it may have been successfully activated.
 */
int iuse::sewage( player *p, item *it, bool, const tripoint & )
{
    if( !p->query_yn( _( "Are you sure you want to drink… this?" ) ) ) {
        return 0;
    }

    g->events().send<event_type::eats_sewage>();
    p->vomit();
    if( one_in( 4 ) ) {
        p->mutate();
    }
    return it->type->charges_to_use();
}

int iuse::honeycomb( player *p, item *it, bool, const tripoint & )
{
    g->m.spawn_item( p->pos(), itype_wax, 2 );
    return it->type->charges_to_use();
}

int iuse::xanax( player *p, item *it, bool, const tripoint & )
{
    p->add_msg_if_player( _( "You take some %s." ), it->tname() );
    p->add_effect( effect_took_xanax, 90_minutes );
    return it->type->charges_to_use();
}

static constexpr time_duration alc_strength( const int strength, const time_duration &weak,
        const time_duration &medium, const time_duration &strong )
{
    return strength == 0 ? weak : strength == 1 ? medium : strong;
}

static int alcohol( player &p, const item &it, const int strength )
{
    // Weaker characters are cheap drunks
    /** @EFFECT_STR_MAX reduces drunkenness duration */
    time_duration duration = alc_strength( strength, 22_minutes, 34_minutes,
                                           45_minutes ) - ( alc_strength( strength, 36_seconds, 1_minutes, 72_seconds ) * p.str_max );
    if( p.has_trait( trait_ALCMET ) ) {
        duration = alc_strength( strength, 6_minutes, 14_minutes, 18_minutes ) - ( alc_strength( strength,
                   36_seconds, 1_minutes, 1_minutes ) * p.str_max );
        // Metabolizing the booze improves the nutritional value;
        // might not be healthy, and still causes Thirst problems, though
        p.mod_stored_kcal( 10 * ( std::abs( it.get_comestible() ? it.type->comestible->stim : 0 ) ) );
        // Metabolizing it cancels out the depressant
        p.mod_stim( std::abs( it.get_comestible() ? it.get_comestible()->stim : 0 ) );
    } else if( p.has_trait( trait_TOLERANCE ) ) {
        duration -= alc_strength( strength, 9_minutes, 16_minutes, 24_minutes );
    } else if( p.has_trait( trait_LIGHTWEIGHT ) ) {
        duration += alc_strength( strength, 9_minutes, 16_minutes, 24_minutes );
    }
    p.add_effect( effect_drunk, duration );
    return it.type->charges_to_use();
}

int iuse::alcohol_weak( player *p, item *it, bool, const tripoint & )
{
    return alcohol( *p, *it, 0 );
}

int iuse::alcohol_medium( player *p, item *it, bool, const tripoint & )
{
    return alcohol( *p, *it, 1 );
}

int iuse::alcohol_strong( player *p, item *it, bool, const tripoint & )
{
    return alcohol( *p, *it, 2 );
}

int iuse::antibiotic( player *p, item *it, bool, const tripoint & )
{
    p->add_msg_player_or_npc( m_neutral,
                              _( "You take some antibiotics." ),
                              _( "<npcname> takes some antibiotics." ) );
    if( p->has_effect( effect_infected ) && !p->has_effect( effect_antibiotic ) ) {
        p->add_msg_if_player( m_good,
                              _( "Maybe just placebo effect, but you feel a little better as the dose settles in." ) );
    }
    p->add_effect( effect_antibiotic, 12_hours );
    return it->type->charges_to_use();
}

int iuse::eyedrops( player *p, item *it, bool, const tripoint & )
{
    if( p->is_underwater() ) {
        p->add_msg_if_player( m_info, _( "You can't do that while underwater." ) );
        return 0;
    }
    if( it->charges < it->type->charges_to_use() ) {
        p->add_msg_if_player( _( "You're out of %s." ), it->tname() );
        return 0;
    }
    p->add_msg_if_player( _( "You use your %s." ), it->tname() );
    p->moves -= to_moves<int>( 10_seconds );
    if( p->has_effect( effect_boomered ) ) {
        p->remove_effect( effect_boomered );
        p->add_msg_if_player( m_good, _( "You wash the slime from your eyes." ) );
    }
    return it->type->charges_to_use();
}

int iuse::fungicide( player *p, item *it, bool, const tripoint & )
{
    if( p->is_underwater() ) {
        p->add_msg_if_player( m_info, _( "You can't do that while underwater." ) );
        return 0;
    }

    const bool has_fungus = p->has_effect( effect_fungus );
    const bool has_spores = p->has_effect( effect_spores );

    if( p->is_npc() && !has_fungus && !has_spores ) {
        return 0;
    }

    p->add_msg_player_or_npc( _( "You use your fungicide." ), _( "<npcname> uses some fungicide" ) );
    if( has_fungus && ( one_in( 3 ) ) ) {
        p->remove_effect( effect_fungus );
        p->add_msg_if_player( m_warning,
                              _( "You feel a burning sensation under your skin that quickly fades away." ) );
    }
    if( has_spores && ( one_in( 2 ) ) ) {
        if( !p->has_effect( effect_fungus ) ) {
            p->add_msg_if_player( m_warning, _( "Your skin grows warm for a moment." ) );
        }
        p->remove_effect( effect_spores );
        int spore_count = rng( 1, 6 );
        for( const tripoint &dest : g->m.points_in_radius( p->pos(), 1 ) ) {
            if( spore_count == 0 ) {
                break;
            }
            if( dest == p->pos() ) {
                continue;
            }
            if( g->m.passable( dest ) && !get_map().obstructed_by_vehicle_rotation( p->pos(), dest ) &&
                x_in_y( spore_count, 8 ) ) {
                if( monster *const mon_ptr = g->critter_at<monster>( dest ) ) {
                    monster &critter = *mon_ptr;
                    if( g->u.sees( dest ) &&
                        !critter.type->in_species( FUNGUS ) ) {
                        add_msg( m_warning, _( "The %s is covered in tiny spores!" ),
                                 critter.name() );
                    }
                    if( !critter.make_fungus() ) {
                        critter.die( p ); // counts as kill by player
                    }
                } else {
                    g->place_critter_at( mon_spore, dest );
                }
                spore_count--;
            }
        }
    }
    return it->type->charges_to_use();
}

int iuse::antifungal( player *p, item *it, bool, const tripoint & )
{
    if( p->is_underwater() ) {
        p->add_msg_if_player( m_info, _( "You can't do that while underwater." ) );
        return 0;
    }
    p->add_msg_if_player( _( "You take some antifungal medication." ) );
    if( p->has_effect( effect_fungus ) ) {
        p->remove_effect( effect_fungus );
        p->add_msg_if_player( m_warning,
                              _( "You feel a burning sensation under your skin that quickly fades away." ) );
    }
    if( p->has_effect( effect_spores ) ) {
        if( !p->has_effect( effect_fungus ) ) {
            p->add_msg_if_player( m_warning, _( "Your skin grows warm for a moment." ) );
        }
    }
    return it->type->charges_to_use();
}

int iuse::antiparasitic( player *p, item *it, bool, const tripoint & )
{
    if( p->is_underwater() ) {
        p->add_msg_if_player( m_info, _( "You can't do that while underwater." ) );
        return 0;
    }
    p->add_msg_if_player( _( "You take some antiparasitic medication." ) );
    if( p->has_effect( effect_dermatik ) ) {
        p->remove_effect( effect_dermatik );
        p->add_msg_if_player( m_good, _( "The itching sensation under your skin fades away." ) );
    }
    if( p->has_effect( effect_tapeworm ) ) {
        p->remove_effect( effect_tapeworm );
        if( p->has_trait( trait_NOPAIN ) ) {
            p->add_msg_if_player( m_good, _( "Your bowels clench as something inside them dies." ) );
        } else {
            p->add_msg_if_player( m_mixed, _( "Your bowels spasm painfully as something inside them dies." ) );
            p->mod_pain( rng( 8, 24 ) );
        }
    }
    if( p->has_effect( effect_bloodworms ) ) {
        p->remove_effect( effect_bloodworms );
        p->add_msg_if_player( _( "Your skin prickles and your veins itch for a few moments." ) );
    }
    if( p->has_effect( effect_brainworms ) ) {
        p->remove_effect( effect_brainworms );
        if( p->has_trait( trait_NOPAIN ) ) {
            p->add_msg_if_player( m_good, _( "The pressure inside your head feels better already." ) );
        } else {
            p->add_msg_if_player( m_mixed,
                                  _( "Your head pounds like a sore tooth as something inside of it dies." ) );
            p->mod_pain( rng( 8, 24 ) );
        }
    }
    if( p->has_effect( effect_paincysts ) ) {
        p->remove_effect( effect_paincysts );
        if( p->has_trait( trait_NOPAIN ) ) {
            p->add_msg_if_player( m_good, _( "The stiffness in your joints goes away." ) );
        } else {
            p->add_msg_if_player( m_good, _( "The pain in your joints goes away." ) );
        }
    }
    return it->type->charges_to_use();
}

int iuse::anticonvulsant( player *p, item *it, bool, const tripoint & )
{
    p->add_msg_if_player( _( "You take some anticonvulsant medication." ) );
    /** @EFFECT_STR reduces duration of anticonvulsant medication */
    time_duration duration = 8_hours - p->str_cur * rng( 0_turns, 10_minutes );
    if( p->has_trait( trait_TOLERANCE ) ) {
        duration -= 1_hours;
    }
    if( p->has_trait( trait_LIGHTWEIGHT ) ) {
        duration += 2_hours;
    }
    p->add_effect( effect_valium, duration );
    if( p->has_effect( effect_shakes ) ) {
        p->remove_effect( effect_shakes );
        p->add_msg_if_player( m_good, _( "You stop shaking." ) );
    }
    return it->type->charges_to_use();
}

int iuse::meth( player *p, item *it, bool, const tripoint & )
{
    /** @EFFECT_STR reduces duration of meth */
    time_duration duration = 1_minutes * ( 60 - p->str_cur );
    if( p->has_amount( itype_apparatus, 1 ) && p->use_charges_if_avail( itype_fire, 1 ) ) {
        p->add_msg_if_player( m_neutral, _( "You smoke your meth." ) );
        p->add_msg_if_player( m_good, _( "The world seems to sharpen." ) );
        p->mod_fatigue( -375 );
        if( p->has_trait( trait_TOLERANCE ) ) {
            duration *= 1.2;
        } else {
            duration *= ( p->has_trait( trait_LIGHTWEIGHT ) ? 1.8 : 1.5 );
        }
        // breathe out some smoke
        for( int i = 0; i < 3; i++ ) {
            g->m.add_field( {p->posx() + rng( -2, 2 ), p->posy() + rng( -2, 2 ), p->posz()},
                            fd_methsmoke, 2 );
        }
    } else {
        p->add_msg_if_player( _( "You snort some crystal meth." ) );
        p->mod_fatigue( -300 );
    }
    if( !p->has_effect( effect_meth ) ) {
        duration += 1_hours;
    }
    if( duration > 0_turns ) {
        // meth actually inhibits hunger, weaker characters benefit more
        /** @EFFECT_STR_MAX >4 experiences less hunger benefit from meth */
        int hungerpen = ( p->str_max < 5 ? 35 : 40 - ( 2 * p->str_max ) );
        if( hungerpen > 0 ) {
            p->mod_stored_kcal( 10 * hungerpen );
        }
        p->add_effect( effect_meth, duration );
    }
    return it->type->charges_to_use();
}

int iuse::vaccine( player *p, item *it, bool, const tripoint & )
{
    p->add_msg_if_player( _( "You inject the vaccine." ) );
    p->add_msg_if_player( m_good, _( "You feel tough." ) );
    p->mod_healthy_mod( 200, 200 );
    p->mod_pain( 3 );
    p->i_add( item::spawn( "syringe", it->birthday() ) );
    return it->type->charges_to_use();
}

int iuse::antiasthmatic( player *p, item *it, bool, const tripoint & )
{
    p->add_msg_if_player( m_good,
                          _( "You no longer need to worry about asthma attacks, at least for a while." ) );
    p->add_effect( effect_took_antiasthmatic, 1_days, bodypart_str_id::NULL_ID() );
    return it->type->charges_to_use();
}

int iuse::poison( player *p, item *it, bool, const tripoint & )
{
    if( ( p->has_trait( trait_EATDEAD ) ) ) {
        return it->type->charges_to_use();
    }

    // NPCs have a magical sense of what is inedible
    // Players can abuse the crafting menu instead...
    if( !it->has_flag( flag_HIDDEN_POISON ) &&
        ( p->is_npc() ||
          !p->query_yn( _( "Are you sure you want to eat this?  It looks poisonous…" ) ) ) ) {
        return 0;
    }
    if( !p->has_trait( trait_POISRESIST ) ) {
        p->add_effect( effect_poison, 15_minutes );
    }

    p->add_effect( effect_foodpoison, 3_hours );
    return it->type->charges_to_use();
}

int iuse::meditate( player *p, item *it, bool t, const tripoint & )
{
    if( !p || t ) {
        return 0;
    }
    if( p->is_mounted() ) {
        p->add_msg_if_player( m_info, _( "You cannot do that while mounted." ) );
        return 0;
    }
    if( p->has_trait( trait_SPIRITUAL ) ) {
        const int moves = to_moves<int>( 20_minutes );
        p->assign_activity( ACT_MEDITATE, moves );
    } else {
        p->add_msg_if_player( _( "This %s probably meant a lot to someone at one time." ),
                              it->tname() );
    }
    return it->type->charges_to_use();
}

int iuse::thorazine( player *p, item *it, bool, const tripoint & )
{
    if( p->has_effect( effect_took_thorazine ) ) {
        p->remove_effect( effect_took_thorazine );
        p->mod_fatigue( 15 );
    }
    p->add_effect( effect_took_thorazine, 12_hours );
    p->mod_fatigue( 5 );
    p->remove_effect( effect_hallu );
    p->remove_effect( effect_visuals );
    if( !p->has_effect( effect_dermatik ) ) {
        p->remove_effect( effect_formication );
    }
    if( one_in( 50 ) ) { // adverse reaction
        p->add_msg_if_player( m_bad, _( "You feel completely exhausted." ) );
        p->mod_fatigue( 50 );
    } else {
        p->add_msg_if_player( m_warning, _( "You feel a bit wobbly." ) );
    }
    return it->type->charges_to_use();
}

int iuse::prozac( player *p, item *it, bool, const tripoint & )
{
    if( !p->has_effect( effect_took_prozac ) ) {
        p->add_effect( effect_took_prozac, 12_hours );
    } else {
        p->mod_stim( 3 );
    }
    if( one_in( 16 ) ) { // adverse reaction, same duration as prozac effect.
        p->add_msg_if_player( m_warning, _( "You suddenly feel hollow inside." ) );
        p->add_effect( effect_took_prozac_bad, p->get_effect_dur( effect_took_prozac ) );
    }
    return it->type->charges_to_use();
}

int iuse::sleep( player *p, item *it, bool, const tripoint & )
{
    p->add_msg_if_player( m_warning, _( "You feel sleepy…" ) );
    return it->type->charges_to_use();
}

int iuse::datura( player *p, item *it, bool, const tripoint & )
{
    if( p->is_npc() ) {
        return 0;
    }

    p->add_effect( effect_datura, rng( 3_hours, 13_hours ) );
    p->add_msg_if_player( _( "You eat the datura seed." ) );
    if( p->has_trait( trait_SPIRITUAL ) ) {
        p->add_morale( MORALE_FOOD_GOOD, 36, 72, 2_hours, 1_hours, false, it->type );
    }
    return it->type->charges_to_use();
}

int iuse::flumed( player *p, item *it, bool, const tripoint & )
{
    p->add_effect( effect_took_flumed, 10_hours );
    p->add_msg_if_player( _( "You take some %s" ), it->tname() );
    return it->type->charges_to_use();
}

int iuse::flusleep( player *p, item *it, bool, const tripoint & )
{
    p->add_effect( effect_took_flumed, 12_hours );
    p->add_msg_if_player( _( "You take some %s" ), it->tname() );
    p->add_msg_if_player( m_warning, _( "You feel sleepy…" ) );
    return it->type->charges_to_use();
}

int iuse::inhaler( player *p, item *it, bool, const tripoint & )
{
    p->add_msg_player_or_npc( m_neutral, _( "You take a puff from your inhaler." ),
                              _( "<npcname> takes a puff from their inhaler." ) );
    if( !p->remove_effect( effect_asthma ) ) {
        p->mod_fatigue( -3 ); // if we don't have asthma can be used as stimulant
        if( one_in( 20 ) ) {   // with a small but significant risk of adverse reaction
            p->add_effect( effect_shakes, rng( 2_minutes, 5_minutes ) );
        }
    }
    p->add_effect( effect_took_antiasthmatic, rng( 1_hours, 2_hours ) );
    p->remove_effect( effect_smoke );
    return it->type->charges_to_use();
}

int iuse::oxygen_bottle( player *p, item *it, bool, const tripoint & )
{
    p->moves -= to_moves<int>( 10_seconds );
    p->add_msg_player_or_npc( m_neutral, string_format( _( "You breathe deeply from the %s" ),
                              it->tname() ),
                              string_format( _( "<npcname> breathes from the %s" ),
                                      it->tname() ) );
    if( p->has_effect( effect_smoke ) ) {
        p->remove_effect( effect_smoke );
    } else if( p->has_effect( effect_teargas ) ) {
        p->remove_effect( effect_teargas );
    } else if( p->has_effect( effect_asthma ) ) {
        p->remove_effect( effect_asthma );
    } else if( p->get_stim() < 16 ) {
        p->mod_stim( 8 );
        p->mod_painkiller( 2 );
    }
    p->mod_painkiller( 2 );
    return it->type->charges_to_use();
}

int iuse::blech( player *p, item *it, bool, const tripoint & )
{
    // TODO: Add more effects?
    if( it->made_of( LIQUID ) ) {
        if( !p->query_yn( _( "This looks unhealthy, sure you want to drink it?" ) ) ) {
            return 0;
        }
    } else { //Assume that if a blech consumable isn't a drink, it will be eaten.
        if( !p->query_yn( _( "This looks unhealthy, sure you want to eat it?" ) ) ) {
            return 0;
        }
    }

    if( it->has_flag( flag_ACID ) && ( p->has_trait( trait_ACIDPROOF ) ||
                                       p->has_trait( trait_ACIDBLOOD ) ) ) {
        p->add_msg_if_player( m_bad, _( "Blech, that tastes gross!" ) );
        //reverse the harmful values of drinking this acid.
        double multiplier = -1;
        p->mod_stored_kcal( 10 * p->nutrition_for( *it ) * multiplier );
        p->mod_thirst( -it->get_comestible()->quench * multiplier + 20 );
        p->mod_healthy_mod( it->get_comestible()->healthy * multiplier,
                            it->get_comestible()->healthy * multiplier );
        p->add_morale( MORALE_FOOD_BAD, it->get_comestible_fun() * multiplier, 60, 1_hours, 30_minutes,
                       false, it->type );
    } else {
        p->add_msg_if_player( m_bad, _( "Blech, that burns your throat!" ) );
        p->mod_pain( rng( 32, 64 ) );
        p->add_effect( effect_poison, 15_minutes );
        p->apply_damage( nullptr, bodypart_id( "torso" ), rng( 4, 12 ) );
        p->vomit();
    }
    return it->type->charges_to_use();
}

int iuse::blech_because_unclean( player *p, item *it, bool, const tripoint & )
{
    if( !p->is_npc()  && !p->has_bionic( bio_digestion ) ) {
        if( it->made_of( LIQUID ) ) {
            if( !p->query_yn( _( "This looks unclean, sure you want to drink it?" ) ) ) {
                return 0;
            }
        } else { //Assume that if a blech consumable isn't a drink, it will be eaten.
            if( !p->query_yn( _( "This looks unclean, sure you want to eat it?" ) ) ) {
                return 0;
            }
        }
    }
    return it->type->charges_to_use();
}

int iuse::plantblech( player *p, item *it, bool, const tripoint &pos )
{
    if( p->has_trait( trait_THRESH_PLANT ) ) {
        double multiplier = -1;
        if( p->has_trait( trait_CHLOROMORPH ) ) {
            multiplier = -3;
            p->add_msg_if_player( m_good, _( "The meal is revitalizing." ) );
        } else {
            p->add_msg_if_player( m_good, _( "Oddly enough, this doesn't taste so bad." ) );
        }

        //reverses the harmful values of drinking fertilizer
        p->mod_stored_kcal( -10 * p->nutrition_for( *it ) * multiplier );
        p->mod_thirst( -it->get_comestible()->quench * multiplier );
        p->mod_healthy_mod( it->get_comestible()->healthy * multiplier,
                            it->get_comestible()->healthy * multiplier );
        p->add_morale( MORALE_FOOD_GOOD, -10 * multiplier, 60, 1_hours, 30_minutes, false, it->type );
        return it->type->charges_to_use();
    } else {
        return blech( p, it, true, pos );
    }
}

// Helper to handle the logic of removing some random mutations.
static void do_purify( player &p )
{
    std::vector<trait_id> valid; // Which flags the player has
    // We should use the actual thresh category if present, but old characters might not have it
    // So if they don't have it then we default to old behavior of highest category
    mutation_category_id thresh = p.thresh_category != mutation_category_id::NULL_ID() ?
                                  p.thresh_category : p.get_highest_category();
    for( auto &traits_iter : mutation_branch::get_all() ) {
        if( p.has_trait( traits_iter.id ) && !p.has_base_trait( traits_iter.id ) ) {
            //Looks for active mutation
            bool threshlocked = false;
            for( auto cat : traits_iter.category ) {
                if( ( cat == thresh ) && p.crossed_threshold() && ( p.thresh_tier > traits_iter.threshold_tier ) ) {
                    // We shouldn't be able to get rid of mutations that we have a threshold from
                    threshlocked = true;
                    break;
                }
            }
            if( !threshlocked ) {
                valid.push_back( traits_iter.id );
            }
        }
    }
    if( valid.empty() ) {
        p.add_msg_if_player( _( "You feel cleansed." ) );
        return;
    }
    int num_cured = rng( 1, valid.size() );
    num_cured = std::min( 4, num_cured );
    for( int i = 0; i < num_cured && !valid.empty(); i++ ) {
        const trait_id id = random_entry_removed( valid );
        if( id->purifiable ) {
            p.remove_mutation( id );
        } else {
            p.add_msg_if_player( m_warning, _( "You feel a slight itching inside, but it passes." ) );
        }
    }
}

int iuse::purifier( player *p, item *it, bool, const tripoint & )
{
    mutagen_attempt checks =
        mutagen_common_checks( *p, *it, false, mutagen_technique::consumed_purifier );
    if( !checks.allowed ) {
        return checks.charges_used;
    }

    do_purify( *p );
    return it->type->charges_to_use();
}

int iuse::purify_iv( player *p, item *it, bool, const tripoint & )
{
    mutagen_attempt checks =
        mutagen_common_checks( *p, *it, false, mutagen_technique::injected_purifier );
    if( !checks.allowed ) {
        return checks.charges_used;
    }
    // We should use the actual thresh category if present, but old characters might not have it
    // So if they don't have it then we default to old behavior of highest category
    mutation_category_id thresh = p->thresh_category != mutation_category_id::NULL_ID() ?
                                  p->thresh_category : p->get_highest_category();
    std::vector<trait_id> valid; // Which flags the player has
    for( auto &traits_iter : mutation_branch::get_all() ) {
        if( p->has_trait( traits_iter.id ) && !p->has_base_trait( traits_iter.id ) ) {
            //Looks for active mutation
            bool threshlocked = false;
            for( auto cat : traits_iter.category ) {
                if( ( cat == thresh ) && p->crossed_threshold() &&
                    ( p->thresh_tier > traits_iter.threshold_tier ) ) {
                    // We shouldn't be able to get rid of mutations that we have a threshold from
                    threshlocked = true;
                    break;
                }
            }
            if( !threshlocked ) {
                valid.push_back( traits_iter.id );
            }
        }
    }
    if( valid.empty() ) {
        p->add_msg_if_player( _( "You feel cleansed." ) );
        return it->type->charges_to_use();
    }
    int num_cured = rng( 4,
                         valid.size() ); //Essentially a double-strength purifier, but guaranteed at least 4.  Double-edged and all
    if( num_cured > 8 ) {
        num_cured = 8;
    }
    for( int i = 0; i < num_cured && !valid.empty(); i++ ) {
        const trait_id id = random_entry_removed( valid );
        if( id->purifiable ) {
            p->remove_mutation( id );
        } else {
            p->add_msg_if_player( m_warning, _( "You feel a distinct burning inside, but it passes." ) );
        }
        if( !( p->has_trait( trait_NOPAIN ) ) ) {
            p->mod_pain( 2 * num_cured ); //Hurts worse as it fixes more
            p->add_msg_if_player( m_warning, _( "Feels like you're on fire, but you're OK." ) );
        }
        p->mod_stored_nutr( 2 * num_cured );
        p->mod_thirst( 2 * num_cured );
        p->mod_fatigue( 2 * num_cured );
    }
    return it->type->charges_to_use();
}

int iuse::purify_smart( player *p, item *it, bool, const tripoint & )
{
    mutagen_attempt checks =
        mutagen_common_checks( *p, *it, false, mutagen_technique::injected_smart_purifier );
    if( !checks.allowed ) {
        return checks.charges_used;
    }

    // We should use the actual thresh category if present, but old characters might not have it
    // So if they don't have it then we default to old behavior of highest category
    mutation_category_id thresh = p->thresh_category != mutation_category_id::NULL_ID() ?
                                  p->thresh_category : p->get_highest_category();
    std::vector<trait_id> valid; // Which flags the player has
    std::vector<std::string> valid_names; // Which flags the player has
    for( auto &traits_iter : mutation_branch::get_all() ) {
        if( p->has_trait( traits_iter.id ) &&
            !p->has_base_trait( traits_iter.id ) &&
            traits_iter.id->purifiable ) {
            //Looks for active mutation
            bool threshlocked = false;
            for( auto cat : traits_iter.category ) {
                if( ( cat == thresh ) && p->crossed_threshold() &&
                    ( p->thresh_tier > traits_iter.threshold_tier ) ) {
                    // We shouldn't be able to get rid of mutations that we have a threshold from
                    threshlocked = true;
                    break;
                }
            }
            if( !threshlocked ) {
                valid.push_back( traits_iter.id );
                valid_names.push_back( traits_iter.id->name() );
            }
        }
    }
    if( valid.empty() ) {
        p->add_msg_if_player( _( "You don't have any mutations to purify." ) );
        return 0;
    }

    int mutation_index = uilist( _( "Choose a mutation to purify" ), valid_names );
    if( mutation_index < 0 ) {
        return 0;
    }

    p->add_msg_if_player(
        _( "You inject the purifier.  The liquid thrashes inside the tube and goes down reluctantly." ) );

    p->remove_mutation( valid[mutation_index] );
    valid.erase( valid.begin() + mutation_index );

    p->mod_pain( 3 );

    p->i_add( item::spawn( "syringe", it->birthday() ) );
    return it->type->charges_to_use();
}

static void spawn_spores( const player &p )
{
    int spores_spawned = 0;
    map &here = get_map();
    fungal_effects fe( *g, here );
    for( const tripoint &dest : closest_points_first( p.pos(), 4 ) ) {
        if( here.impassable( dest ) ) {
            continue;
        }
        float dist = rl_dist( dest, p.pos() );
        if( x_in_y( 1, dist ) ) {
            fe.marlossify( dest );
        }
        if( g->critter_at( dest ) != nullptr ) {
            continue;
        }
        if( one_in( 10 + 5 * dist ) && one_in( spores_spawned * 2 ) ) {
            if( monster *const spore = g->place_critter_at( mon_spore, dest ) ) {
                spore->friendly = -1;
                spores_spawned++;
            }
        }
    }
}

static void marloss_common( player &p, item &it, const trait_id &current_color )
{
    static const std::map<trait_id, add_type> mycus_colors = {{
            { trait_MARLOSS_BLUE, add_type::MARLOSS_B }, { trait_MARLOSS_YELLOW, add_type::MARLOSS_Y }, { trait_MARLOSS, add_type::MARLOSS_R }
        }
    };

    if( p.has_trait( current_color ) || p.has_trait( trait_THRESH_MARLOSS ) ) {
        p.add_msg_if_player( m_good,
                             _( "As you eat the %s, you have a near-religious experience, feeling at one with your surroundings…" ),
                             it.tname() );
        p.add_morale( MORALE_MARLOSS, 100, 1000 );
        for( const std::pair<const trait_id, add_type> &pr : mycus_colors ) {
            if( pr.first != current_color ) {
                p.add_addiction( pr.second, 50 );
            }
        }

        p.set_stored_kcal( p.max_stored_kcal() );
        spawn_spores( p );
        return;
    }

    int marloss_count = std::count_if( mycus_colors.begin(), mycus_colors.end(),
    [&p]( const std::pair<trait_id, add_type> &pr ) {
        return p.has_trait( pr.first );
    } );

    /* If we're not already carriers of current type of Marloss, roll for a random effect:
     * 1 - Mutate
     * 2 - Mutate
     * 3 - Mutate
     * 4 - Purify
     * 5 - Purify
     * 6 - Cleanse radiation + Purify
     * 7 - Fully satiate
     * 8 - Vomit
     * 9-12 - Give Marloss mutation
     */
    int effect = rng( 1, 12 );
    if( effect <= 3 ) {
        p.add_msg_if_player( _( "It tastes extremely strange!" ) );
        p.mutate();
        // Gruss dich, mutation drain, missed you!
        p.mod_pain( 2 * rng( 1, 5 ) );
        p.mod_stored_nutr( 10 );
        p.mod_thirst( 10 );
        p.mod_fatigue( 5 );
    } else if( effect <= 6 ) { // Radiation cleanse is below
        p.add_msg_if_player( m_good, _( "You feel better all over." ) );
        p.mod_painkiller( 30 );
        iuse::purifier( &p, &it, false, p.pos() );
        if( effect == 6 ) {
            p.set_rad( 0 );
        }
    } else if( effect == 7 ) {
        p.add_msg_if_player( m_good, _( "It is delicious, and very filling!" ) );
        p.set_stored_kcal( p.max_stored_kcal() );
    } else if( effect == 8 ) {
        p.add_msg_if_player( m_bad, _( "You take one bite, and immediately vomit!" ) );
        p.vomit();
    } else if( p.crossed_threshold() || p.has_trait( trait_PROF_FERAL ) ) {
        // Mycus Rejection.  Goo already present fights off the fungus.
        p.add_msg_if_player( m_bad,
                             _( "You feel a familiar warmth, but suddenly it surges into an excruciating burn as you convulse, vomiting, and black out…" ) );
        if( p.is_avatar() ) {
            g->memorial().add(
                pgettext( "memorial_male", "Suffered Marloss Rejection." ),
                pgettext( "memorial_female", "Suffered Marloss Rejection." ) );
        }
        p.vomit();
        p.mod_pain( 90 );
        p.hurtall( rng( 40, 65 ), nullptr ); // No good way to say "lose half your current HP"
        /** @EFFECT_INT slightly reduces sleep duration when eating mycus+goo */
        p.fall_asleep( 10_hours - p.int_cur *
                       1_minutes ); // Hope you were eating someplace safe.  Mycus v. Goo in your guts is no joke.
        for( const std::pair<const trait_id, add_type> &pr : mycus_colors ) {
            p.unset_mutation( pr.first );
            p.rem_addiction( pr.second );
        }
        p.set_mutation(
            trait_MARLOSS_AVOID ); // And if you survive it's etched in your RNA, so you're unlikely to repeat the experiment.
    } else if( marloss_count >= 2 ) {
        p.add_msg_if_player( m_bad,
                             _( "You feel a familiar warmth, but suddenly it surges into painful burning as you convulse and collapse to the ground…" ) );
        /** @EFFECT_INT reduces sleep duration when eating wrong color marloss */
        p.fall_asleep( 40_minutes - 1_minutes * p.int_cur / 2 );
        for( const std::pair<const trait_id, add_type> &pr : mycus_colors ) {
            p.unset_mutation( pr.first );
            p.rem_addiction( pr.second );
        }

        p.set_mutation( trait_THRESH_MARLOSS );
        g->m.ter_set( p.pos(), t_marloss );
        g->events().send<event_type::crosses_marloss_threshold>( p.getID() );
        p.add_msg_if_player( m_good,
                             _( "You wake up in a marloss bush.  Almost *cradled* in it, actually, as though it grew there for you." ) );
        p.add_msg_if_player( m_good,
                             //~ Beginning to hear the Mycus while conscious: that's it speaking
                             _( "unity.  together we have reached the door.  we provide the final key.  now to pass through…" ) );
    } else {
        p.add_msg_if_player( _( "You feel a strange warmth spreading throughout your body…" ) );
        p.set_mutation( current_color );
        // Give us addictions to the other two colors, but cure one for current color
        for( const std::pair<const trait_id, add_type> &pr : mycus_colors ) {
            if( pr.first == current_color ) {
                p.rem_addiction( pr.second );
            } else {
                p.add_addiction( pr.second, 60 );
            }
        }
    }
}

static bool marloss_prevented( const player &p )
{
    if( p.is_npc() ) {
        return true;
    }
    if( p.has_trait( trait_MARLOSS_AVOID ) ) {
        p.add_msg_if_player( m_warning,
                             //~ "Uh-uh" is a sound used for "nope", "no", etc.
                             _( "After what happened that last time?  uh-uh.  You're not eating that alien poison." ) );
        return true;
    }
    if( p.has_trait( trait_THRESH_MYCUS ) ) {
        p.add_msg_if_player( m_info,
                             _( "We no longer require this scaffolding.  We reserve it for other uses." ) );
        return true;
    }

    return false;
}

int iuse::marloss( player *p, item *it, bool, const tripoint & )
{
    if( marloss_prevented( *p ) ) {
        return 0;
    }

    g->events().send<event_type::consumes_marloss_item>( p->getID(), it->typeId() );

    marloss_common( *p, *it, trait_MARLOSS );
    return it->type->charges_to_use();
}

int iuse::marloss_seed( player *p, item *it, bool, const tripoint & )
{
    if( !query_yn( _( "Sure you want to eat the %s?  You could plant it in a mound of dirt." ),
                   colorize( it->tname(), it->color_in_inventory() ) ) ) {
        return 0; // Save the seed for later!
    }

    if( marloss_prevented( *p ) ) {
        return 0;
    }

    g->events().send<event_type::consumes_marloss_item>( p->getID(), it->typeId() );

    marloss_common( *p, *it, trait_MARLOSS_BLUE );
    return it->type->charges_to_use();
}

int iuse::marloss_gel( player *p, item *it, bool, const tripoint & )
{
    if( marloss_prevented( *p ) ) {
        return 0;
    }

    g->events().send<event_type::consumes_marloss_item>( p->getID(), it->typeId() );

    marloss_common( *p, *it, trait_MARLOSS_YELLOW );
    return it->type->charges_to_use();
}

int iuse::mycus( player *p, item *it, bool t, const tripoint &pos )
{
    if( p->is_npc() ) {
        return it->type->charges_to_use();
    }
    // Welcome our guide.  Welcome.  To. The Mycus.
    if( p->has_trait( trait_THRESH_MARLOSS ) ) {
        g->events().send<event_type::crosses_mycus_threshold>( p->getID() );
        p->add_msg_if_player( m_neutral,
                              _( "It tastes amazing, and you finish it quickly." ) );
        p->add_msg_if_player( m_good, _( "You feel better all over." ) );
        p->mod_painkiller( 30 );
        purifier( p, it, t, pos ); // Clear out some of that goo you may have floating around
        p->set_rad( 0 );
        p->healall( 4 ); // Can't make you a whole new person, but not for lack of trying
        p->add_msg_if_player( m_good,
                              _( "As it settles in, you feel ecstasy radiating through every part of your body…" ) );
        p->add_morale( MORALE_MARLOSS, 1000, 1000 ); // Last time you'll ever have it this good.  So enjoy.
        p->add_msg_if_player( m_good,
                              _( "Your eyes roll back in your head.  Everything dissolves into a blissful haze…" ) );
        /** @EFFECT_INT slightly reduces sleep duration when eating mycus */
        p->fall_asleep( 5_hours - p->int_cur * 1_minutes );
        p->unset_mutation( trait_THRESH_MARLOSS );
        p->set_mutation( trait_THRESH_MYCUS );
        g->invalidate_main_ui_adaptor();
        //~ The Mycus does not use the term (or encourage the concept of) "you".  The PC is a local/native organism, but is now the Mycus.
        //~ It still understands the concept, but uninitelligent fungaloids and mind-bent symbiotes should not need it.
        //~ We are the Mycus.
        popup( _( "We welcome into us.  We have endured long in this forbidding world." ) );
        p->add_msg_if_player( " " );
        p->add_msg_if_player( m_good,
                              _( "A sea of white caps, waving gently.  A haze of spores wafting silently over a forest." ) );
        g->invalidate_main_ui_adaptor();
        popup( _( "The natives have a saying: \"E Pluribus Unum.\"  Out of many, one." ) );
        p->add_msg_if_player( " " );
        p->add_msg_if_player( m_good,
                              _( "The blazing pink redness of the berry.  The juices spreading across your tongue, the warmth draping over us like a lover's embrace." ) );
        g->invalidate_main_ui_adaptor();
        popup( _( "We welcome the union of our lines in our local guide.  We will prosper, and unite this world.  Even now, our fruits adapt to better serve local physiology." ) );
        p->add_msg_if_player( " " );
        p->add_msg_if_player( m_good,
                              _( "The sky-blue of the seed.  The nutty, creamy flavors intermingling with the berry, a memory that will never leave us." ) );
        g->invalidate_main_ui_adaptor();
        popup( _( "As, in time, shall we adapt to better welcome those who have not received us." ) );
        p->add_msg_if_player( " " );
        p->add_msg_if_player( m_good,
                              _( "The amber-yellow of the sap.  Feel it flowing through our veins, taking the place of the strange, thin red gruel called \"blood.\"" ) );
        g->invalidate_main_ui_adaptor();
        popup( _( "We are the Mycus." ) );
        /*p->add_msg_if_player( m_good,
                              _( "We welcome into us.  We have endured long in this forbidding world." ) );
        p->add_msg_if_player( m_good,
                              _( "The natives have a saying: \"E Pluribus Unum\"  Out of many, one." ) );
        p->add_msg_if_player( m_good,
                              _( "We welcome the union of our lines in our local guide.  We will prosper, and unite this world." ) );
        p->add_msg_if_player( m_good, _( "Even now, our fruits adapt to better serve local physiology." ) );
        p->add_msg_if_player( m_good,
                              _( "As, in time, shall we adapt to better welcome those who have not received us." ) );*/
        fungal_effects fe( *g, g->m );
        for( const tripoint &nearby_pos : g->m.points_in_radius( p->pos(), 3 ) ) {
            fe.marlossify( nearby_pos );
        }
        p->rem_addiction( add_type::MARLOSS_R );
        p->rem_addiction( add_type::MARLOSS_B );
        p->rem_addiction( add_type::MARLOSS_Y );
    } else if( p->has_trait( trait_THRESH_MYCUS ) &&
               !p->has_trait( trait_M_DEPENDENT ) ) { // OK, now set the hook.
        if( !one_in( 3 ) ) {
            p->mutate_category( mutation_category_id( "MYCUS" ) );
            p->mod_stored_nutr( 10 );
            p->mod_thirst( 10 );
            p->mod_fatigue( 5 );
            p->add_morale( MORALE_MARLOSS, 25, 200 ); // still covers up mutation pain
        }
    } else if( p->has_trait( trait_THRESH_MYCUS ) ) {
        p->mod_painkiller( 5 );
        p->mod_stim( 5 );
    } else { // In case someone gets one without having been adapted first.
        // Marloss is the Mycus' method of co-opting humans.  Mycus fruit is for symbiotes' maintenance and development.
        p->add_msg_if_player(
            _( "This tastes really weird!  You're not sure it's good for you…" ) );
        p->mutate();
        p->mod_pain( 2 * rng( 1, 5 ) );
        p->mod_stored_nutr( 10 );
        p->mod_thirst( 10 );
        p->mod_fatigue( 5 );
        p->vomit(); // no hunger/quench benefit for you
        p->mod_healthy_mod( -8, -50 );
    }
    return it->type->charges_to_use();
}

int iuse::petfood( player *p, item *it, bool, const tripoint & )
{
    if( !it->is_comestible() ) {
        p->add_msg_if_player( _( "You doubt someone would want to eat % 1$s." ), it->tname() );
        return 0;
    }

    const std::optional<tripoint> pnt_ = choose_adjacent( string_format(
            _( "Tame which animal with the %s?" ),
            it->tname() ) );
    if( !pnt_ ) {
        return 0;
    }
    const tripoint pnt = *pnt_;
    p->moves -= to_moves<int>( 1_seconds );

    // First a check to see if we are trying to feed a NPC dog food.
    if( g->critter_at<npc>( pnt ) != nullptr ) {
        if( npc *const person_ = g->critter_at<npc>( pnt ) ) {
            npc &person = *person_;
            if( query_yn( _( "Are you sure you want to feed a person the pet food?" ) ) ) {
                p->add_msg_if_player( _( "You put your %1$s into %2$s's mouth!" ), it->tname(),
                                      person.name );
                if( person.is_ally( *p ) || x_in_y( 9, 10 ) ) {
                    person.say(
                        _( "Okay, but please, don't give me this again.  I don't want to eat pet food in the cataclysm all day." ) );
                    p->consume_charges( *it, 1 );
                    return 0;
                } else {
                    p->add_msg_if_player( _( "%s knocks it out from your hand!" ), person.name );
                    person.make_angry();
                    p->consume_charges( *it, 1 );
                    return 0;
                }
            } else {
                p->add_msg_if_player( _( "Never mind." ) );
                return 0;
            }
        }

        // Then monsters.
    } else if( monster *const mon_ptr = g->critter_at<monster>( pnt, true ) ) {
        monster &mon = *mon_ptr;

        if( mon.is_hallucination() ) {
            p->add_msg_if_player( _( "You try to feed the %s some %s, but it vanishes!" ),
                                  mon.type->nname(), it->tname() );
            mon.die( nullptr );
            return 0;
        }

        // Feral survivors don't get to tame normal critters.
        if( p->has_trait( trait_PROF_FERAL ) ) {
            // TODO: Allow player ferals to tame zombie animals, but make sure non-feral players
            // can't tame them, and for flavor possibly only allow taming with meat-based items.
            p->add_msg_if_player( _( "You reach for the %s, but it recoils away from you!" ),
                                  mon.type->nname() );
            return 0;
        }

        //check to see if the item has a petfood data entry deterimine if the item can be fed to a bet
        bool can_feed = false;
        const pet_food_data &petfood = mon.type->petfood;
        const std::set<std::string> &itemfood = it->get_comestible()->petfood;
        if( !petfood.food.empty() ) {
            for( const std::string &food : petfood.food ) {
                if( itemfood.contains( food ) ) {
                    can_feed = true;
                    break;
                }
            }
        }

        //if the item cannot be fed, give a message to the player and return
        if( !can_feed ) {
            p->add_msg_if_player( _( "The %s doesn't want that kind of food." ),
                                  mon.type->nname() );
            return 0;
        }

        if( !petfood.tamer_traits.empty() ) {
            for( const TraitSet &trait_set : petfood.tamer_traits ) {
                if( !p->has_one_of_traits( trait_set ) ) {
                    can_feed = false;
                } else {
                    can_feed = true;
                }
            }
            if( !can_feed ) {
                p->add_msg_if_player( _( "The %s does not trust your kind." ),
                                      mon.type->nname() );
                return 0;
            }
        }

        if( mon.type->id == mon_dog_thing ) {
            p->deal_damage( &mon, bodypart_id( "hand_r" ), damage_instance( DT_CUT, rng( 1, 10 ) ) );
            p->add_msg_if_player( m_bad, _( "You want to feed it the pet food, but it bites your fingers!" ) );
            if( one_in( 5 ) ) {
                p->add_msg_if_player(
                    _( "Apparently it's more interested in your flesh than the pet food in your hand!" ) );
                p->consume_charges( *it, 1 );
                return 0;
            }
        }

        p->add_msg_if_player( _( "You feed your %1$s to the %2$s." ), it->tname(), mon.get_name() );

        if( petfood.feed.empty() ) {
            p->add_msg_if_player( _( "The %1$s is your pet now!" ), mon.get_name() );
        } else {
            p->add_msg_if_player( _( petfood.feed ), mon.get_name() );
        }

        mon.make_pet();

        // Apply well_fed effect to improve monster productivity
        // This effect increases reproduction rate, milk production, growth speed, and HP recovery
        // Duration: 24 hours (one full day cycle)
        const time_duration well_fed_duration = 24_hours;

        if( mon.has_effect( effect_well_fed ) ) {
            // Refresh duration if already well-fed
            mon.add_effect( effect_well_fed, well_fed_duration );
        } else {
            // Apply new well-fed effect
            mon.add_effect( effect_well_fed, well_fed_duration );
            p->add_msg_if_player( m_good,
                                  _( "The %s looks healthier and more productive." ),
                                  mon.get_name() );
        }

        p->consume_charges( *it, 1 );
        return 0;
    }

    p->add_msg_if_player( _( "There is nothing to be fed here." ) );
    return 0;

}

int iuse::radio_mod( player *p, item *, bool, const tripoint & )
{
    if( p->is_npc() ) {
        // Now THAT would be kinda cruel
        return 0;
    }

    auto filter = []( const item & itm ) {
        return itm.has_flag( flag_RADIO_MODABLE );
    };

    // note: if !p->is_npc() then p is avatar
    item *loc = game_menus::inv::titled_filter_menu(
                    filter, *p->as_avatar(), _( "Modify what?" ) );

    if( !loc ) {
        p->add_msg_if_player( _( "You do not have that item!" ) );
        return 0;
    }
    item &modded = *loc;

    int choice = uilist( _( "Which signal should activate the item?" ), {
        _( "\"Red\"" ), _( "\"Blue\"" ), _( "\"Green\"" )
    } );

    flag_id newtag;
    std::string colorname;
    switch( choice ) {
        case 0:
            newtag = flag_RADIOSIGNAL_1;
            colorname = _( "\"Red\"" );
            break;
        case 1:
            newtag = flag_RADIOSIGNAL_2;
            colorname = _( "\"Blue\"" );
            break;
        case 2:
            newtag = flag_RADIOSIGNAL_3;
            colorname = _( "\"Green\"" );
            break;
        default:
            return 0;
    }

    if( modded.has_flag( flag_RADIO_MOD ) && modded.has_flag( newtag ) ) {
        p->add_msg_if_player( _( "This item has been modified this way already." ) );
        return 0;
    }

    remove_radio_mod( modded, *p );

    p->add_msg_if_player(
        _( "You modify your %1$s to listen for %2$s activation signal on the radio." ),
        modded.tname(), colorname );
    modded.set_flag( flag_RADIO_ACTIVATION );
    modded.set_flag( flag_RADIOCARITEM );
    modded.set_flag( flag_RADIO_MOD );
    modded.set_flag( newtag );
    return 1;
}

int iuse::remove_all_mods( player *p, item *, bool, const tripoint & )
{
    if( !p ) {
        return 0;
    }

    item *loc = g->inv_map_splice( []( const item & e ) {
        for( const item *it : e.toolmods() ) {
            if( !it->is_irremovable() ) {
                return true;
            }
        }
        if( e.has_flag( flag_RADIO_MOD ) ) {
            return true;
        }
        return false;
    },
    _( "Remove mods from tool?" ), 1,
    _( "You don't have any modified tools." ) );

    if( !loc ) {
        add_msg( m_info, _( "Never mind." ) );
        return 0;
    }

    if( !loc->ammo_remaining() || avatar_funcs::unload_item( *p->as_avatar(), *loc ) ) {
        bool done = false;
        loc->contents.remove_top_items_with(
        [&p, &done]( detached_ptr<item> &&e ) {
            if( !done && e->is_toolmod() && !e->is_irremovable() ) {
                done = true;
                add_msg( m_info, _( "You remove the %s from the tool." ), e->tname() );
                return p->i_add_or_drop( std::move( e ) );
            }
            return std::move( e );
        } );

        remove_radio_mod( *loc, *p );
    }
    return 0;
}
// Returns 0-5 based on how good the fishing spot is, 5 being "Middle of the
// ocean" and 0 being "no"
int iuse::good_fishing_spot( tripoint pos )
{
    int fishable_locations = g->get_fishable_locations( 60, pos ).size();
    map &here = get_map();
    const oter_id &cur_omt =
        overmap_buffer.ter( tripoint_abs_omt( ms_to_omt_copy( here.getabs( pos ) ) ) );
    std::string om_id = cur_omt.id().c_str();
    if( fishable_locations < 100 && !g->m.has_flag( "CURRENT", pos ) &&
        om_id.find( "river_" ) == std::string::npos && !cur_omt->is_lake() &&
        !cur_omt->is_lake_shore() ) {
        return 0;
    }
    // I hate I cant use a switch for this.
    // Tiles in range is 144k, so knock off 14k to be nice for max fishing e-'fish'-iency.
    if( fishable_locations >= 10400 ) {
        return 5;
    } else if( fishable_locations < 10400 && fishable_locations >= 7800 ) {
        return 4;
    } else if( fishable_locations < 7800 && fishable_locations >= 5200 ) {
        return 3;
    } else if( fishable_locations < 5200 && fishable_locations >= 2600 ) {
        return 2;//If you cant amass a 10x10 for fishing womp womp.
    } else if( fishable_locations < 2600 && fishable_locations >= 100 ) {
        return 1;
    }
    g->u.add_msg_if_player(
        m_info, _( "You doubt you will catch anything here, best look elsewhere" ) );
    return 0;
}

int iuse::fishing_rod( player *p, item *it, bool, const tripoint & )
{
    if( p->is_npc() ) {
        // Long actions - NPCs don't like those yet.
        return 0;
    }
    if( p->is_mounted() ) {
        p->add_msg_if_player( m_info, _( "You cannot do that while mounted." ) );
        return 0;
    }
    std::optional<tripoint> found;
    for( const tripoint &pnt : g->m.points_in_radius( p->pos(), 1 ) ) {
        if( g->m.has_flag( flag_FISHABLE, pnt ) && iuse::good_fishing_spot( pnt ) != 0 ) {
            found = pnt;
            break;
        }
    }
    if( !found ) {
        p->add_msg_if_player( m_info, _( "You can't fish there!" ) );
        return 0;
    }
    switch( iuse::good_fishing_spot( *found ) ) {
        case 1: {
            p->add_msg_if_player( m_info,
                                  _( "You doubt you will catch too much here, but surely theres some" ) );
            break;
        }
        case 2: {
            p->add_msg_if_player(
                m_info, _( "You think there might be a few things to catch here" ) );
            break;
        }
        case 3: {
            p->add_msg_if_player(
                m_info, _( "You see at least one catch, should be a decent spot" ) );
            break;
        }
        case 4: {
            p->add_msg_if_player(
                m_info, _( "You see several catches already, this should be a great spot" ) );
            break;
        }
        case 5: {
            p->add_msg_if_player(
                m_info, _( "You see a massive number of catches here, this is as good as it gets" ) );
            break;
        }
    }
    p->add_msg_if_player( _( "You cast your line and wait to hook something…" ) );
    p->assign_activity( ACT_FISH, to_moves<int>( 5_hours ), 0, 0, it->tname() );
    p->activity->tools.emplace_back( it );
    p->activity->placement = *found;
    p->activity->coord_set = g->get_fishable_locations( 60, *found );
    return 0;
}

int iuse::fish_trap( player *p, item *it, bool t, const tripoint &pos )
{
    if( !t ) {
        // Handle deploying fish trap.
        if( it->is_active() ) {
            it->deactivate();
            return 0;
        }

        if( it->charges < 0 ) {
            it->charges = 0;
            return 0;
        }
        if( p->is_mounted() ) {
            p->add_msg_if_player( m_info, _( "You cannot do that while mounted." ) );
            return 0;
        }
        if( p->is_underwater() ) {
            p->add_msg_if_player( m_info, _( "You can't do that while underwater." ) );
            return 0;
        }

        if( it->charges == 0 ) {
            p->add_msg_if_player( _( "Fish are not foolish enough to go in here without bait." ) );
            return 0;
        }

        const std::optional<tripoint> pnt_ = choose_adjacent( _( "Put fish trap where?" ) );
        if( !pnt_ ) {
            return 0;
        }
        const tripoint pnt = *pnt_;

        if( !g->m.has_flag( "FISHABLE", pnt ) ) {
            p->add_msg_if_player( m_info, _( "You can't fish there!" ) );
            return 0;
        }
        if( good_fishing_spot( pnt ) == 0 ) {
            return 0;
        }
        it->activate();
        it->set_age( 0_turns );
        g->m.add_item_or_charges( pnt, it->detach() );
        p->add_msg_if_player( m_info,
                              _( "You place the fish trap, in three hours or so you may catch some fish." ) );

        return 0;

    } else {
        // Handle processing fish trap over time.
        if( it->charges == 0 ) {
            it->deactivate();
            return 0;
        }
        if( it->age() > 3_hours ) {
            it->deactivate();

            if( !g->m.has_flag( "FISHABLE", pos ) ) {
                return 0;
            }
            int fish = good_fishing_spot( pos );
            int success = -250 + ( good_fishing_spot( pos ) * 15 );
            const int surv_mod = p->get_skill_level( skill_survival ) + ( fish );
            for( int i = 0; i < it->charges; i++ ) {
                success += surv_mod + fish;
            }

            it->charges = 0;

            int caught = 0;

            if( success >= 200 ) {
                caught = rng( 2, 4 );
            } else if( success < 200 && success >= 100 ) {
                caught = rng( 1, 2 );
            } else if( success < 100 && success >= 50 ) {
                caught = rng( 0, 2 );
            } else if( success < 50 && success >= 0 ) {
                caught = rng( 0, 1 );
            } else {
                caught = 0;
            }

            if( caught == 0 ) {
                p->practice( skill_survival, rng( 10, 25 ) );
                return 0;
            }
            for( int i = 0; i < caught; i++ ) {
                p->practice( skill_survival, rng( 4, 8 ) );
                const std::vector<mtype_id> fish_group = MonsterGroupManager::GetMonstersFromGroup(
                            GROUP_FISH );
                const mtype_id &fish_mon = random_entry_ref( fish_group );
                g->m.add_item_or_charges( pos, item::make_corpse( fish_mon, it->birthday() + rng( 0_turns,
                                          3_hours ) ) );
            }
        }
        return 0;
    }
}

int iuse::extinguisher( player *p, item *it, bool, const tripoint & )
{
    if( !it->ammo_sufficient() ) {
        return 0;
    }
    // If anyone other than the player wants to use one of these,
    // they're going to need to figure out how to aim it.
    const std::optional<tripoint> dest_ = choose_adjacent( _( "Spray where?" ) );
    if( !dest_ ) {
        return 0;
    }
    tripoint dest = *dest_;

    p->moves -= to_moves<int>( 2_seconds );

    // Reduce the strength of fire (if any) in the target tile.
    g->m.mod_field_intensity( dest, fd_fire, 0 - rng( 2, 3 ) );

    // Also spray monsters in that tile.
    if( monster *const mon_ptr = g->critter_at<monster>( dest, true ) ) {
        monster &critter = *mon_ptr;
        critter.moves -= to_moves<int>( 2_seconds );
        bool blind = false;
        if( one_in( 2 ) && critter.has_flag( MF_SEES ) ) {
            blind = true;
            critter.add_effect( effect_blind, rng( 1_minutes, 2_minutes ) );
        }
        if( g->u.sees( critter ) ) {
            p->add_msg_if_player( _( "The %s is sprayed!" ), critter.name() );
            if( blind ) {
                p->add_msg_if_player( _( "The %s looks blinded." ), critter.name() );
            }
        }
        if( critter.made_of( LIQUID ) ) {
            if( g->u.sees( critter ) ) {
                p->add_msg_if_player( _( "The %s is frozen!" ), critter.name() );
            }
            critter.apply_damage( p, bodypart_id( "torso" ), rng( 20, 60 ) );
            critter.set_speed_base( critter.get_speed_base() / 2 );
        }
    }

    // Whatever we sprayed, if present extinguish it too.
    if( Creature *target = g->critter_at( dest, true ) ) {
        if( target->has_effect( effect_onfire ) ) {
            target->remove_effect( effect_onfire );
        }
    }

    // Slightly reduce the strength of fire immediately behind the target tile.
    if( g->m.passable( dest ) ) {
        dest.x += ( dest.x - p->posx() );
        dest.y += ( dest.y - p->posy() );

        g->m.mod_field_intensity( dest, fd_fire, std::min( 0 - rng( 0, 1 ) + rng( 0, 1 ), 0 ) );
    }

    return it->type->charges_to_use();
}

int iuse::unpack_item( player *p, item *it, bool, const tripoint & )
{
    if( p->is_underwater() ) {
        p->add_msg_if_player( m_info, _( "You can't do that while underwater." ) );
        return 0;
    }
    std::string oname = it->typeId().str() + "_on";
    p->moves -= to_moves<int>( 10_seconds );
    p->add_msg_if_player( _( "You unpack your %s for use." ), it->tname() );
    it->convert( itype_id( oname ) );
    it->deactivate();
    return 0;
}

int iuse::pack_cbm( player *p, item *it, bool, const tripoint & )
{
    item *bionic = g->inv_map_splice( []( const item & e ) {
        return e.is_bionic() && e.has_flag( flag_NO_PACKED );
    }, _( "Choose CBM to pack" ), PICKUP_RANGE, _( "You don't have any CBMs." ) );

    if( !bionic ) {
        return 0;
    }
    if( !bionic->faults.empty() ) {
        if( p->query_yn( _( "This CBM is faulty.  You should mend it first.  Do you want to try?" ) ) ) {
            avatar_funcs::mend_item( *p->as_avatar(), *bionic );
        }
        return 0;
    }

    const int success = p->get_skill_level( skill_firstaid ) - rng( 0, 6 );
    if( success > 0 ) {
        p->add_msg_if_player( m_good, _( "You carefully prepare the CBM for sterilization." ) );
        bionic->unset_flag( flag_NO_PACKED );
    } else {
        p->add_msg_if_player( m_bad, _( "You fail to properly prepare the CBM." ) );
    }

    std::vector<item_comp> comps;
    comps.emplace_back( it->typeId(), 1 );
    p->consume_items( comps, 1, is_crafting_component );

    return 0;
}

int iuse::pack_item( player *p, item *it, bool t, const tripoint & )
{
    if( p->is_underwater() ) {
        p->add_msg_if_player( m_info, _( "You can't do that while underwater." ) );
        return 0;
    }
    if( t ) { // Normal use
        // Numbers below -1 are reserved for worn items
    } else if( p->get_item_position( it ) < -1 ) {
        p->add_msg_if_player( m_info, _( "You can't pack your %s until you take it off." ),
                              it->tname() );
        return 0;
    } else { // Turning it off
        std::string oname = it->typeId().str();
        if( oname.ends_with( "_on" ) ) {
            oname.erase( oname.length() - 3, 3 );
        } else {
            debugmsg( "no item type to turn it into (%s)!", oname );
            return 0;
        }
        p->moves -= to_moves<int>( 10_seconds );
        p->add_msg_if_player( _( "You pack your %s for storage." ), it->tname() );
        it->convert( itype_id( oname ) );
        it->deactivate();
    }
    return 0;
}

int iuse::water_purifier( player *p, item *it, bool, const tripoint & )
{
    constexpr auto purification_efficiency = 8; // one tablet purifies 250ml x 8 = 2L

    if( p->is_mounted() ) {
        p->add_msg_if_player( m_info, _( "You cannot do that while mounted." ) );
        return 0;
    }
    auto obj = g->inv_map_splice( []( const item & e ) {
        return !e.contents.empty() && e.contents.front().typeId() == itype_water;
    }, _( "Purify what?" ), 1, _( "You don't have water to purify." ) );

    if( !obj ) {
        p->add_msg_if_player( m_info, _( "You do not have that item!" ) );
        return 0;
    }

    item &liquid = obj->contents.front();
    const auto used_charges = std::max( liquid.charges / purification_efficiency, 1 );
    if( !it->units_sufficient( *p, used_charges ) ) {
        p->add_msg_if_player( m_info, _( "That volume of water is too large to purify." ) );
        return 0;
    }

    p->moves -= to_moves<int>( 2_seconds );

    liquid.convert( itype_water_clean );
    liquid.poison = 0;
    return used_charges;
}

int iuse::radio_off( player *p, item *it, bool, const tripoint & )
{
    if( !it->units_sufficient( *p ) ) {
        p->add_msg_if_player( _( "It's dead." ) );
    } else {
        p->add_msg_if_player( _( "You turn the radio on." ) );
        it->convert( itype_radio_on );
        it->activate();
    }
    return it->type->charges_to_use();
}

int iuse::directional_antenna( player *p, item *it, bool, const tripoint & )
{
    // Find out if we have an active radio
    auto radios = p->items_with( []( const item & it ) {
        return it.typeId() == itype_radio_on;
    } );
    // If we don't wield the radio, also check on the ground
    if( radios.empty() ) {
        map_stack items = get_map().i_at( p->pos() );
        for( item * const &an_item : items ) {
            if( an_item->typeId() == itype_radio_on ) {
                radios.push_back( an_item );
            }
        }
    }
    if( radios.empty() ) {
        add_msg( m_info, _( "Must have an active radio to check for signal direction." ) );
        return 0;
    }
    const item &radio = *radios.front();
    // Find the radio station its tuned to (if any)
    const auto tref = overmap_buffer.find_radio_station( radio.frequency );
    if( !tref ) {
        p->add_msg_if_player( m_info, _( "You can't find the direction if your radio isn't tuned." ) );
        return 0;
    }
    // Report direction.
    // TODO: fix point types
    const tripoint_abs_sm player_pos( p->global_sm_location() );
    direction angle = direction_from( player_pos.xy(), tref.abs_sm_pos );
    add_msg( _( "The signal seems strongest to the %s." ), direction_name( angle ) );
    return it->type->charges_to_use();
}

int iuse::radio_on( player *p, item *it, bool t, const tripoint &pos )
{
    if( t ) {
        // Normal use
        std::string message = _( "Radio: Kssssssssssssh." );
        const auto tref = overmap_buffer.find_radio_station( it->frequency );
        if( tref ) {
            const auto selected_tower = tref.tower;
            if( selected_tower->type == radio_type::MESSAGE_BROADCAST ) {
                message = selected_tower->message;
            } else if( selected_tower->type == radio_type::WEATHER_RADIO ) {
                message = weather_forecast( tref.abs_sm_pos );
            }

            message = obscure_message( message, [&]()->int {
                int signal_roll = dice( 10, tref.signal_strength * 3 );
                int static_roll = dice( 10, 100 );
                if( static_roll > signal_roll )
                {
                    if( static_roll < signal_roll * 1.1 && one_in( 4 ) ) {
                        return 0;
                    } else {
                        return '#';
                    }
                } else
                {
                    return -1;
                }
            } );

            std::vector<std::string> segments = foldstring( message, RADIO_PER_TURN );
            int index = to_turn<int>( calendar::turn ) % segments.size();
            message = string_format( _( "radio: %s" ), segments[index] );
        }
        sounds::ambient_sound( pos, 6, sounds::sound_t::electronic_speech, message );
        if( !sfx::is_channel_playing( sfx::channel::radio ) ) {
            if( one_in( 10 ) ) {
                sfx::play_ambient_variant_sound( "radio", "static", 100, sfx::channel::radio, 300, -1, 0 );
            } else if( one_in( 10 ) ) {
                sfx::play_ambient_variant_sound( "radio", "inaudible_chatter", 100, sfx::channel::radio, 300, -1,
                                                 0 );
            }
        }
    } else { // Activated
        int ch = 1;
        if( it->ammo_remaining() > 0 ) {
            ch = uilist( _( "Radio:" ), {
                _( "Scan" ), _( "Turn off" )
            } );
        }

        switch( ch ) {
            case 0: {
                const int old_frequency = it->frequency;
                const radio_tower *lowest_tower = nullptr;
                const radio_tower *lowest_larger_tower = nullptr;
                for( auto &tref : overmap_buffer.find_all_radio_stations() ) {
                    const auto new_frequency = tref.tower->frequency;
                    if( new_frequency == old_frequency ) {
                        continue;
                    }
                    if( new_frequency > old_frequency &&
                        ( lowest_larger_tower == nullptr || new_frequency < lowest_larger_tower->frequency ) ) {
                        lowest_larger_tower = tref.tower;
                    } else if( lowest_tower == nullptr || new_frequency < lowest_tower->frequency ) {
                        lowest_tower = tref.tower;
                    }
                }
                if( lowest_larger_tower != nullptr ) {
                    it->frequency = lowest_larger_tower->frequency;
                } else if( lowest_tower != nullptr ) {
                    it->frequency = lowest_tower->frequency;
                }
            }
            break;
            case 1:
                p->add_msg_if_player( _( "The radio dies." ) );
                it->convert( itype_radio );
                it->deactivate();
                sfx::fade_audio_channel( sfx::channel::radio, 300 );
                break;
            default:
                break;
        }
    }
    return it->type->charges_to_use();
}

int iuse::noise_emitter_off( player *p, item *it, bool, const tripoint & )
{
    if( !it->units_sufficient( *p ) ) {
        p->add_msg_if_player( _( "It's dead." ) );
    } else {
        p->add_msg_if_player( _( "You turn the noise emitter on." ) );
        it->convert( itype_noise_emitter_on );
        it->activate();
    }
    return it->type->charges_to_use();
}

int iuse::noise_emitter_on( player *p, item *it, bool t, const tripoint &pos )
{
    if( t ) { // Normal use
        //~ the sound of a noise emitter when turned on
        sounds::sound( pos, 30, sounds::sound_t::alarm, _( "KXSHHHHRRCRKLKKK!" ), true, "tool",
                       "noise_emitter" );
    } else { // Turning it off
        p->add_msg_if_player( _( "The infernal racket dies as the noise emitter turns off." ) );
        it->convert( itype_noise_emitter );
        it->deactivate();
    }
    return it->type->charges_to_use();
}

// Ugly and uses variables that shouldn't be public
int iuse::note_bionics( player *p, item *it, bool t, const tripoint &pos )
{
    const bool possess = p->has_item( *it );

    if( !t ) {
        it->revert( p, true );
        it->deactivate();
        return 0;
    }
    if( !p->is_avatar() ) {
        // Not supported at the moment
        return 0;
    }
    map &here = get_map();

    if( !p->has_enough_charges( *it, false ) ) {
        it->revert( p, true );
        it->deactivate();
        return 0;
    }

    // Try to minimize the use of has_enough_charges() because it's kind of expensive.
    bool no_charges = false;
    for( const tripoint &pt : here.points_in_radius( pos, PICKUP_RANGE ) ) {
        if( !here.has_items( pt ) || !p->sees( pt ) ) {
            continue;
        }
        for( item * const &corpse : here.i_at( pt ) ) {
            if( !corpse->is_corpse() ||
                corpse->get_var( "bionics_scanned_by", -1 ) == p->getID().get_value() ) {
                continue;
            }

            std::vector<const item *> cbms;
            for( const item * const &maybe_cbm : corpse->get_components() ) {
                if( maybe_cbm->is_bionic() ) {
                    cbms.push_back( maybe_cbm );
                }
            }

            int charges = static_cast<int>( cbms.size() );
            charges -= it->ammo_consume( charges, pos );
            if( possess && it->has_flag( flag_USE_UPS ) ) {
                if( p->use_charges_if_avail( itype_UPS, charges ) ) {
                    charges = 0;
                }
            }
            if( charges ) {
                p->add_msg_if_player( m_bad, "Your %s doesn't have enough power for the %s", it->tname(),
                                      corpse->display_name().c_str() );
                if( !p->has_enough_charges( *it, false ) ) {
                    no_charges = true;
                    break;
                } else {
                    continue;
                }
            }

            corpse->set_var( "bionics_scanned_by", p->getID().get_value() );
            if( !cbms.empty() ) {
                corpse->set_flag( flag_CBM_SCANNED );
                std::string bionics_string =
                    enumerate_as_string( cbms.begin(), cbms.end(),
                []( const item * entry ) -> std::string {
                    return entry->display_name();
                }, enumeration_conjunction::none );
                //~ %1 is corpse name, %2 is direction, %3 is bionic name
                p->add_msg_if_player( m_good, _( "A %1$s located %2$s contains %3$s." ),
                                      corpse->display_name().c_str(),
                                      direction_name( direction_from( p->pos(), pt ) ).c_str(),
                                      bionics_string.c_str()
                                    );
            }
        }
        if( no_charges ) {
            it->revert( p );
            it->deactivate();
            return 0;
        }
    }

    return 0;
}

int iuse::ma_manual( player *p, item *it, bool, const tripoint & )
{
    // [CR] - should NPCs just be allowed to learn this stuff? Just like that?

    const matype_id style_to_learn = martial_art_learned_from( *it->type );

    if( !style_to_learn.is_valid() ) {
        debugmsg( "ERROR: Invalid martial art" );
        return 0;
    }

    p->martial_arts_data->learn_style( style_to_learn, p->is_avatar() );

    return 1;
}

int iuse::hammer( player *p, item *it, bool, const tripoint & )
{
    if( p->is_mounted() ) {
        p->add_msg_if_player( m_info, _( "You cannot do that while mounted." ) );
        return 0;
    }

    const std::function<bool( const tripoint & )> f = []( const tripoint & pnt ) {
        if( pnt == g->u.pos() ) {
            return false;
        }
        const ter_id ter = g->m.ter( pnt );

        return ( ter->nail_pull_result != ter_str_id::NULL_ID() );
    };

    const std::optional<tripoint> pnt_ = choose_adjacent_highlight(
            _( "Pry where?" ), _( "There is nothing to pry nearby." ), f, false );
    if( !pnt_ ) {
        return 0;
    }
    const tripoint &pnt = *pnt_;
    if( !f( pnt ) ) {
        if( pnt == p->pos() ) {
            p->add_msg_if_player( _( "You try to hit yourself with the hammer." ) );
            p->add_msg_if_player( _( "But you can't touch this." ) );
        } else {
            p->add_msg_if_player( m_info, _( "You can't pry that." ) );
        }
        return 0;
    } else {
        // pry action
        std::unique_ptr<player_activity> act = std::make_unique<player_activity>( ACT_PRY_NAILS,
                                               to_moves<int>( 30_seconds ),
                                               -1 );
        act->placement = pnt;
        p->assign_activity( std::move( act ) );
        return it->type->charges_to_use();
    }
}

int iuse::crowbar( player *p, item *it, bool, const tripoint &pos )
{
    if( p->is_mounted() ) {
        p->add_msg_if_player( m_info, _( "You cannot do that while mounted." ) );
        return 0;
    }
    const pry_result *pry = nullptr;
    bool pry_furn;

    const std::function<bool( const tripoint & )> can_pry = [&p]( const tripoint & pnt ) {
        if( pnt == p->pos() ) {
            return false;
        }
        const ter_id ter = g->m.ter( pnt );
        const furn_id furn = g->m.furn( pnt );

        const bool is_allowed = ter->pry.pry_quality != -1 || furn->pry.pry_quality != -1;
        return is_allowed;
    };

    const std::optional<tripoint> pnt_ = ( pos != p->pos() ) ? pos : choose_adjacent_highlight(
            _( "Pry where?" ), _( "There is nothing to pry nearby." ), can_pry, false );
    if( !pnt_ ) {
        return 0;
    }
    const tripoint &pnt = *pnt_;
    const ter_id ter = g->m.ter( pnt );
    const furn_id furn = g->m.furn( pnt );

    if( !can_pry( pnt ) ) {
        if( pnt == p->pos() ) {
            p->add_msg_if_player( m_info, _( "You attempt to pry open your wallet "
                                             "but alas.  You are just too miserly." ) );
        } else if( !ter->has_flag( "LOCKED" ) && ter->open ) {
            p->add_msg_if_player( m_info, _( "You notice the door is unlocked, so you simply open it." ) );
            g->m.ter_set( pnt, ter->open );
        } else {
            p->add_msg_if_player( m_info, _( "You can't pry that." ) );
        }

        return 0;
    }

    if( furn->pry.pry_quality != -1 ) {
        pry_furn = true;
        pry = &furn->pry;
    } else {
        pry_furn = false;
        pry = &ter->pry;
    }

    // Doors need PRY 2 which is on a crowbar, crates need PRY 1 which is on a crowbar
    // & a claw hammer.
    // The iexamine function for crate supplies a hammer object.
    // So this stops the player (A)ctivating a Hammer with a Crowbar in their backpack
    // then managing to open a door.
    const int pry_level = it->get_quality( quality_id( "PRY" ) );

    if( pry_level < pry->pry_quality ) {
        p->add_msg_if_player( _( "You can't get sufficient leverage to open that with your %s." ),
                              it->tname() );
        p->mod_moves( 10 ); // spend a few moves trying it.
        return 0;
    }

    // For every level of PRY over the requirement, remove n from the difficulty.
    // Then multiply n by pry_bonus_mult. It's recommended that you don't allow
    // the result to be negative if you can help it.
    int diff = pry->difficulty;
    diff -= ( ( pry_level - pry->pry_quality ) * pry->pry_bonus_mult );

    /** @EFFECT_STR speeds up crowbar prying attempts */
    p->mod_moves( -std::max( 20, diff * 50 - p->str_cur * 10 ) );
    /** @EFFECT_STR increases chance of crowbar prying success */

    if( dice( 4, diff ) < dice( 4, p->str_cur ) ) {
        p->add_msg_if_player( m_good, pry->success_message );

        if( pry_furn ) {
            g->m.furn_set( pnt, pry->new_furn_type );
        } else {
            g->m.ter_set( pnt, pry->new_ter_type );
        }

        if( pry->noise > 0 ) {
            sounds::sound( pnt, pry->noise, sounds::sound_t::combat, pry->sound, true, "tool", "crowbar" );
        }
        g->m.spawn_items( pnt, item_group::items_from( pry->pry_items, calendar::turn ) );
        if( pry->alarm ) {
            g->events().send<event_type::triggers_alarm>( p->getID() );
            sounds::sound( p->pos(), 40, sounds::sound_t::alarm, _( "an alarm sound!" ), true, "environment",
                           "alarm" );
            if( !g->timed_events.queued( TIMED_EVENT_WANTED ) ) {
                g->timed_events.add( TIMED_EVENT_WANTED, calendar::turn + 30_minutes, 0,
                                     p->global_sm_location() );
            }
        }
    } else {
        if( pry->breakable ) {
            //chance of breaking the glass if pry attempt fails
            /** @EFFECT_STR reduces chance of breaking window with crowbar */

            /** @EFFECT_MECHANICS reduces chance of breaking window with crowbar */
            if( dice( 4, diff ) > ( dice( 2, p->get_skill_level( skill_mechanics ) ) + dice( 2,
                                    p->str_cur ) ) * pry_level ) {
                p->add_msg_if_player( m_mixed, pry->break_message );
                sounds::sound( pnt, pry->break_noise, sounds::sound_t::combat, pry->break_sound, true, "smash",
                               "door" );
                if( pry_furn ) {
                    g->m.furn_set( pnt, pry->break_furn_type );
                } else {
                    g->m.ter_set( pnt, pry->break_ter_type );
                }
                g->m.spawn_items( pnt, item_group::items_from( pry->break_items, calendar::turn ) );
                if( pry->alarm ) {
                    g->events().send<event_type::triggers_alarm>( p->getID() );
                    sounds::sound( p->pos(), 40, sounds::sound_t::alarm, _( "an alarm sound!" ), true, "environment",
                                   "alarm" );
                    if( !g->timed_events.queued( TIMED_EVENT_WANTED ) ) {
                        g->timed_events.add( TIMED_EVENT_WANTED, calendar::turn + 30_minutes, 0,
                                             p->global_sm_location() );
                    }
                }
                return it->type->charges_to_use();
            }
        }
        p->add_msg_if_player( pry->fail_message );
    }
    return it->type->charges_to_use();
}

int iuse::makemound( player *p, item *it, bool t, const tripoint & )
{
    if( !p || t ) {
        return 0;
    }
    if( p->is_mounted() ) {
        p->add_msg_if_player( m_info, _( "You cannot do that while mounted." ) );
        return 0;
    }
    const std::optional<tripoint> pnt_ = choose_adjacent( _( "Till soil where?" ) );
    if( !pnt_ ) {
        return 0;
    }
    const tripoint pnt = *pnt_;

    if( pnt == p->pos() ) {
        p->add_msg_if_player( m_info,
                              _( "You think about jumping on a shovel, but then change up your mind." ) );
        return 0;
    }

    if( g->m.has_flag( flag_PLOWABLE, pnt ) && !g->m.has_flag( flag_PLANT, pnt ) ) {
        p->add_msg_if_player( _( "You start churning up the earth here." ) );
        p->assign_activity( ACT_CHURN, 18000, -1, p->get_item_position( it ) );
        p->activity->placement = g->m.getabs( pnt );
        return it->type->charges_to_use();
    } else {
        p->add_msg_if_player( _( "You can't churn up this ground." ) );
        return 0;
    }
}

struct digging_moves_and_byproducts {
    int moves;
    std::string byproducts_item_group;
    ter_id result_terrain;
};

static digging_moves_and_byproducts dig_pit_moves_and_byproducts( player *p, item *it,
        const tripoint &pos, const bool channel )
{
    // Vastly simplified version of DDA's version, which had a 77-line-long explanation.
    //
    // Here, we simply set a target base time to dig, 60 minutes to dig a shallow pit,
    // 120 minutes for a deep pit. This is meant to be more in line with woodcutting,
    // mining, and other activities instead. Crafting quality is used to divide this like
    // we do with woodcutting, so deep pit is balanced around the minimum permitted quality
    // of 2 cutting that base time in half.
    //
    // We also must tone down the yield of dirt to avoid potential problems,
    // the old math was generating more than the tile volume limit.
    //
    // So to keep it simple, 200 liters for shallow pits, 400 for deep pit. We're basically
    // assuming that the first step is about one-third of the total work.

    // Get the dig quality of the tool.
    const int quality = it->get_quality( qual_DIG );

    ///\EFFECT_STR modifies dig rate
    // Adjust the dig rate if the player is above or below strength of 10.
    // Floor it at 1 so we don't divide by zero, of course!
    const double attr = 10.0 / std::max( 1, p->str_cur );

    // And now determine the moves...
    int dig_minutes = channel ? 60 : g->m.ter( pos )->digging_results.num_minutes;
    int moves = to_moves<int>( std::max( 10_minutes,
                                         time_duration::from_minutes( dig_minutes * attr ) / quality ) );
    // Channel can be assumed to always be moving water because it doesn't create magic terraforming in theory.
    ter_id result_terrain = channel ? ter_id( "t_water_moving_sh" ) : g->m.ter(
                                pos )->digging_results.result_ter;

    return { moves, g->m.ter( pos )->digging_results.result_items.str(), result_terrain };
}

int iuse::dig( player *p, item *it, bool t, const tripoint & )
{
    if( !p || t ) {
        return 0;
    }
    if( p->is_mounted() ) {
        p->add_msg_if_player( m_info, _( "You cannot do that while mounted." ) );
        return 0;
    }
    const tripoint dig_point = p->pos();

    const bool can_dig_here = g->m.ter( dig_point )->is_diggable() &&
                              !g->m.has_furn( dig_point ) &&
                              g->m.tr_at( dig_point ).is_null() &&
                              ( g->m.ter( dig_point ) == t_grave_new || g->m.i_at( dig_point ).empty() ) &&
                              !g->m.veh_at( dig_point );

    if( !can_dig_here ) {
        p->add_msg_if_player(
            _( "You can't dig a pit in this location.  Ensure it is clear diggable ground with no items or obstacles." ) );
        return 0;
    }
    const bool grave = g->m.ter( dig_point ) == t_grave;

    if( !( p->crafting_inventory().max_quality( qual_DIG ) >= g->m.ter(
               dig_point )->digging_results.dig_min ) ) {
        if( grave ) {
            p->add_msg_if_player( _( "You can't exhume a grave without a better digging tool." ) );
            return 0;
        } else {
            p->add_msg_if_player( _( "You don't have a good enough digging tool to dig there!" ) );
            return 0;
        }
    }

    const std::function<bool( const tripoint & )> f = []( const tripoint & pnt ) {
        return g->m.passable( pnt );
    };

    const std::optional<tripoint> pnt_ = choose_adjacent_highlight(
            _( "Deposit excavated materials where?" ),
            _( "There is nowhere to deposit the excavated materials." ), f, false );
    if( !pnt_ ) {
        return 0;
    }
    const tripoint deposit_point = *pnt_;

    if( !f( deposit_point ) ) {
        p->add_msg_if_player(
            _( "You can't deposit the excavated materials onto an impassable location." ) );
        return 0;
    }

    if( grave ) {
        if( g->u.has_trait( trait_SPIRITUAL ) && !g->u.has_trait( trait_PSYCHOPATH ) &&
            g->u.query_yn( _( "Would you really touch the sacred resting place of the dead?" ) ) ) {
            add_msg( m_info, _( "Exhuming a grave is really against your beliefs." ) );
            g->u.add_morale( MORALE_GRAVEDIGGER, -50, -100, 48_hours, 12_hours );
            if( one_in( 3 ) ) {
                g->u.vomit();
            }
        } else if( g->u.has_trait( trait_PSYCHOPATH ) ) {
            p->add_msg_if_player( m_good,
                                  _( "Exhuming a grave is fun now, where there is no one to object." ) );
            g->u.add_morale( MORALE_GRAVEDIGGER, 25, 50, 2_hours, 1_hours );
        } else if( !g->u.has_trait( trait_EATDEAD ) &&
                   !g->u.has_trait( trait_SAPROVORE ) ) {
            p->add_msg_if_player( m_bad, _( "Exhuming this grave is utterly disgusting!" ) );
            g->u.add_morale( MORALE_GRAVEDIGGER, -25, -50, 2_hours, 1_hours );
            if( one_in( 5 ) ) {
                p->vomit();
            }
        }
    }

    digging_moves_and_byproducts moves_and_byproducts = dig_pit_moves_and_byproducts( p, it, dig_point,
            false );

    const std::vector<npc *> helpers = character_funcs::get_crafting_helpers( *p, 3 );
    for( const npc *np : helpers ) {
        add_msg( m_info, _( "%s helps with this task…" ), np->name );
    }
    moves_and_byproducts.moves = moves_and_byproducts.moves * ( 10 - helpers.size() ) / 10;

    p->assign_activity( std::make_unique<player_activity>( std::make_unique<dig_activity_actor>(
                            moves_and_byproducts.moves,
                            dig_point,
                            moves_and_byproducts.result_terrain.id().str(),
                            deposit_point,
                            moves_and_byproducts.byproducts_item_group
                        ) ) );

    return it->type->charges_to_use();
}

int iuse::dig_channel( player *p, item *it, bool t, const tripoint & )
{
    if( !p || t ) {
        return 0;
    }
    if( p->is_mounted() ) {
        p->add_msg_if_player( m_info, _( "You cannot do that while mounted." ) );
        return 0;
    }
    const tripoint dig_point = p->pos();

    tripoint north = dig_point + point_north;
    tripoint south = dig_point + point_south;
    tripoint west = dig_point + point_west;
    tripoint east = dig_point + point_east;

    const bool can_dig_here = g->m.ter( dig_point )->is_diggable() &&
                              !g->m.has_furn( dig_point ) &&
                              g->m.tr_at( dig_point ).is_null() && g->m.i_at( dig_point ).empty() && !g->m.veh_at( dig_point ) &&
                              ( g->m.has_flag( flag_CURRENT, north ) ||  g->m.has_flag( flag_CURRENT, south ) ||
                                g->m.has_flag( flag_CURRENT, east ) ||  g->m.has_flag( flag_CURRENT, west ) );

    if( !can_dig_here ) {
        p->add_msg_if_player(
            _( "You can't dig a channel in this location.  Ensure it is clear diggable ground with no items or obstacles, adjacent to flowing water." ) );
        return 0;
    }

    const std::function<bool( const tripoint & )> f = []( const tripoint & pnt ) {
        return g->m.passable( pnt );
    };

    const std::optional<tripoint> pnt_ = choose_adjacent_highlight(
            _( "Deposit excavated materials where?" ),
            _( "There is nowhere to deposit the excavated materials." ), f, false );
    if( !pnt_ ) {
        return 0;
    }
    const tripoint deposit_point = *pnt_;

    if( !f( deposit_point ) ) {
        p->add_msg_if_player(
            _( "You can't deposit the excavated materials onto an impassable location." ) );
        return 0;
    }

    digging_moves_and_byproducts moves_and_byproducts = dig_pit_moves_and_byproducts( p, it, dig_point,
            true );

    const std::vector<npc *> helpers = character_funcs::get_crafting_helpers( *p, 3 );
    for( const npc *np : helpers ) {
        add_msg( m_info, _( "%s helps with this task…" ), np->name );
    }
    moves_and_byproducts.moves = moves_and_byproducts.moves * ( 10 - helpers.size() ) / 10;

    p->assign_activity( std::make_unique<player_activity>( std::make_unique<dig_channel_activity_actor>(
                            moves_and_byproducts.moves,
                            dig_point,
                            moves_and_byproducts.result_terrain.id().str(),
                            deposit_point,
                            moves_and_byproducts.byproducts_item_group
                        ) ) );
    return it->type->charges_to_use();
}

int iuse::fill_pit( player *p, item *it, bool t, const tripoint & )
{
    if( !p || t ) {
        return 0;
    }
    if( p->is_mounted() ) {
        p->add_msg_if_player( m_info, _( "You cannot do that while mounted." ) );
        return 0;
    }

    const std::function<bool( const tripoint & )> f = []( const tripoint & pnt ) {
        if( pnt == g->u.pos() ) {
            return false;
        }
        const ter_id type = g->m.ter( pnt );
        return ( type->fill_result != ter_str_id::NULL_ID() );
    };

    const std::optional<tripoint> pnt_ = choose_adjacent_highlight(
            _( "Fill which pit or mound?" ), _( "There is no pit or mound to fill nearby." ), f, false );
    if( !pnt_ ) {
        return 0;
    }
    const tripoint &pnt = *pnt_;
    const ter_id ter = g->m.ter( pnt );
    if( !f( pnt ) ) {
        if( pnt == p->pos() ) {
            p->add_msg_if_player( m_info, _( "You decide not to bury yourself that early." ) );
        } else {
            p->add_msg_if_player( m_info, _( "There is nothing to fill." ) );
        }
        return 0;
    }

    int moves = to_moves<int>( time_duration::from_minutes( ter->fill_minutes ) );

    const std::vector<npc *> helpers = character_funcs::get_crafting_helpers( *p, 3 );
    for( const npc *np : helpers ) {
        add_msg( m_info, _( "%s helps with this task…" ), np->name );
    }
    moves = moves * ( 10 - helpers.size() ) / 10;

    p->assign_activity( ACT_FILL_PIT, moves, -1, p->get_item_position( it ) );
    p->activity->placement = pnt;

    return it->type->charges_to_use();
}

/**
 * Explanation of ACT_CLEAR_RUBBLE activity values:
 *
 * coords[0]: Where the rubble is.
 * index: The bonus, for calculating hunger and thirst penalties.
 */

int iuse::clear_rubble( player *p, item *it, bool, const tripoint & )
{
    if( p->is_mounted() ) {
        p->add_msg_if_player( m_info, _( "You cannot do that while mounted." ) );
        return 0;
    }
    const std::function<bool( const tripoint & )> f = []( const tripoint & pnt ) {
        return g->m.has_flag( "RUBBLE", pnt );
    };

    const std::optional<tripoint> pnt_ = choose_adjacent_highlight(
            _( "Clear rubble where?" ), _( "There is no rubble to clear nearby." ), f, false );
    if( !pnt_ ) {
        return 0;
    }
    const tripoint &pnt = *pnt_;
    if( !f( pnt ) ) {
        p->add_msg_if_player( m_bad, _( "There's no rubble to clear." ) );
        return 0;
    }

    int moves = to_moves<int>( 30_seconds );
    int bonus = std::max( it->get_quality( quality_id( "DIG" ) ) - 1, 1 );

    const std::vector<npc *> helpers = character_funcs::get_crafting_helpers( *p, 3 );
    for( const npc *np : helpers ) {
        add_msg( m_info, _( "%s helps with this task…" ), np->name );
    }
    moves = moves * ( 10 - helpers.size() ) / 10;

    player_activity act( ACT_CLEAR_RUBBLE, moves / bonus, bonus );
    p->assign_activity( std::make_unique<player_activity>( ACT_CLEAR_RUBBLE, moves / bonus, bonus ) );
    p->activity->placement = pnt;
    return it->type->charges_to_use();
}

void act_vehicle_siphon( vehicle * ); // veh_interact.cpp

int iuse::siphon( player *p, item *it, bool, const tripoint & )
{
    if( p->is_mounted() ) {
        p->add_msg_if_player( m_info, _( "You cannot do that while mounted." ) );
        return 0;
    }
    const std::function<bool( const tripoint & )> f = []( const tripoint & pnt ) {
        const optional_vpart_position vp = g->m.veh_at( pnt );
        return !!vp;
    };

    vehicle *v = nullptr;
    bool found_more_than_one = false;
    for( const tripoint &pos : g->m.points_in_radius( g->u.pos(), 1 ) ) {
        const optional_vpart_position vp = g->m.veh_at( pos );
        if( !vp ) {
            continue;
        }
        vehicle *vfound = &vp->vehicle();
        if( v == nullptr ) {
            v = vfound;
        } else {
            //found more than one vehicle?
            if( v != vfound ) {
                v = nullptr;
                found_more_than_one = true;
                break;
            }
        }
    }
    if( found_more_than_one ) {
        std::optional<tripoint> pnt_ = choose_adjacent_highlight(
                                           _( "Siphon from where?" ), _( "There is nothing to siphon nearby." ), f, false );
        if( !pnt_ ) {
            return 0;
        }
        const optional_vpart_position vp = g->m.veh_at( *pnt_ );
        if( vp ) {
            v = &vp->vehicle();
        }
    }

    if( v == nullptr ) {
        p->add_msg_if_player( m_info, _( "There's no vehicle there." ) );
        return 0;
    }
    act_vehicle_siphon( v );
    return it->type->charges_to_use();
}

int iuse::jackhammer( player *p, item *it, bool, const tripoint &pos )
{
    // use has_enough_charges to check for UPS availability
    // p is assumed to exist for iuse cases
    if( !p->has_enough_charges( *it, false ) ) {
        return 0;
    }
    if( p->is_mounted() ) {
        p->add_msg_if_player( m_info, _( "You cannot do that while mounted." ) );
        return 0;
    }
    if( p->is_underwater() ) {
        p->add_msg_if_player( m_info, _( "You can't do that while underwater." ) );
        return 0;
    }

    tripoint pnt = pos;
    if( pos == p->pos() ) {
        const std::optional<tripoint> pnt_ = choose_adjacent( _( "Drill where?" ) );
        if( !pnt_ ) {
            return 0;
        }
        pnt = *pnt_;
    }

    if( !g->m.has_flag( "MINEABLE", pnt ) ) {
        p->add_msg_if_player( m_info, _( "You can't drill there." ) );
        return 0;
    }
    if( g->m.veh_at( pnt ) ) {
        p->add_msg_if_player( _( "There's a vehicle in the way!" ) );
        return 0;
    }

    // Base time of 30 minutes at 8 strength
    int moves = to_moves<int>( 10_minutes );
    moves += ( 24 - std::min( p->str_cur, 24 ) ) * to_moves<int>( 75_seconds );
    if( g->m.move_cost( pnt ) == 2 ) {
        // We're breaking up some flat surface like pavement, which is much easier
        moves /= 2;
    }

    const std::vector<npc *> helpers = character_funcs::get_crafting_helpers( *p, 3 );
    for( const npc *np : helpers ) {
        add_msg( m_info, _( "%s helps with this task…" ), np->name );
    }
    moves = moves * ( 10 - helpers.size() ) / 10;

    p->assign_activity( ACT_JACKHAMMER, moves );
    p->activity->tools.emplace_back( it );
    p->activity->placement = g->m.getabs( pnt );
    p->add_msg_if_player( _( "You start drilling into the %1$s with your %2$s." ),
                          g->m.tername( pnt ), it->tname() );

    return it->type->charges_to_use();
}

int iuse::pick_lock( player *p, item *it, bool, const tripoint &pos )
{
    if( p->is_npc() ) {
        return 0;
    }
    avatar &you = dynamic_cast<avatar &>( *p );

    std::optional<tripoint> target;
    // Prompt for a target lock to pick, or use the given tripoint
    if( pos == you.pos() ) {
        target = lockpick_activity_actor::select_location( you );
    } else {
        target = pos;
    }
    if( !target.has_value() ) {
        return 0;
    }

    int qual = it->get_quality( qual_LOCKPICK );

    /** @EFFECT_DEX speeds up door lock picking */
    /** @EFFECT_MECHANICS speeds up door lock picking */
    int duration;
    if( it->has_flag( flag_PERFECT_LOCKPICK ) ) {
        duration = to_moves<int>( 5_seconds );
    } else {
        duration = std::max( to_moves<int>( 10_seconds ),
                             to_moves<int>( 10_minutes - time_duration::from_minutes( qual ) ) -
                             ( you.dex_cur + you.get_skill_level( skill_mechanics ) ) * 2300 );
    }

    you.assign_activity( std::make_unique<player_activity>( lockpick_activity_actor::use_item( duration,
                         *it, g->m.getabs( *target ) ) ) );
    return it->type->charges_to_use();
}

int iuse::pickaxe( player *p, item *it, bool, const tripoint &pos )
{
    if( p->is_npc() ) {
        // Long action
        return 0;
    }
    if( p->is_mounted() ) {
        p->add_msg_if_player( m_info, _( "You cannot do that while mounted." ) );
        return 0;
    }
    if( p->is_underwater() ) {
        p->add_msg_if_player( m_info, _( "You can't do that while underwater." ) );
        return 0;
    }

    tripoint pnt = pos;
    if( pos == p->pos() ) {
        const std::optional<tripoint> pnt_ = choose_adjacent( _( "Mine where?" ) );
        if( !pnt_ ) {
            return 0;
        }
        pnt = *pnt_;
    }

    if( !g->m.has_flag( "MINEABLE", pnt ) ) {
        p->add_msg_if_player( m_info, _( "You can't mine there." ) );
        return 0;
    }
    if( g->m.veh_at( pnt ) ) {
        p->add_msg_if_player( _( "There's a vehicle in the way!" ) );
        return 0;
    }

    // Base time of 90 minutes at 8 strength
    int moves = to_moves<int>( 30_minutes );
    moves += ( 24 - std::min( p->str_cur, 24 ) ) * to_moves<int>( 225_seconds );
    if( g->m.move_cost( pnt ) == 2 ) {
        // We're breaking up some flat surface like pavement, which is much easier
        moves /= 2;
    }

    const std::vector<npc *> helpers = character_funcs::get_crafting_helpers( *p, 3 );
    for( const npc *np : helpers ) {
        add_msg( m_info, _( "%s helps with this task…" ), np->name );
    }
    moves = moves * ( 10 - helpers.size() ) / 10;

    p->assign_activity( ACT_PICKAXE, moves, -1 );
    p->activity->tools.emplace_back( it );
    p->activity->placement = g->m.getabs( pnt );
    p->add_msg_if_player( _( "You strike the %1$s with your %2$s." ),
                          g->m.tername( pnt ), it->tname() );
    return 0; // handled when the activity finishes
}

int iuse::burrow( player *p, item *it, bool, const tripoint &pos )
{
    if( p->is_npc() ) {
        // Long action
        return 0;
    }
    if( p->is_mounted() ) {
        p->add_msg_if_player( m_info, _( "You cannot do that while mounted." ) );
        return 0;
    }
    if( p->is_underwater() ) {
        p->add_msg_if_player( m_info, _( "You can't do that while underwater." ) );
        return 0;
    }

    tripoint pnt = pos;
    if( pos == p->pos() ) {
        const std::optional<tripoint> pnt_ = choose_adjacent( _( "Burrow where?" ) );
        if( !pnt_ ) {
            return 0;
        }
        pnt = *pnt_;
    }

    if( !g->m.has_flag( "MINEABLE", pnt ) ) {
        p->add_msg_if_player( m_info, _( "You can't burrow there." ) );
        return 0;
    }
    if( g->m.veh_at( pnt ) ) {
        p->add_msg_if_player( _( "There's a vehicle in the way!" ) );
        return 0;
    }

    // Base time of 60 minutes at 8 strength
    int moves = to_moves<int>( 20_minutes );
    moves += ( 24 - std::min( p->str_cur, 24 ) ) * to_moves<int>( 150_seconds );
    if( g->m.move_cost( pnt ) == 2 ) {
        // We're breaking up some flat surface like pavement, which is much easier
        moves /= 2;
    }

    // For consistency, makes as much sense as NPCs helping you mine faster when you're the only one with the tool
    const std::vector<npc *> helpers = character_funcs::get_crafting_helpers( *p, 3 );
    for( const npc *np : helpers ) {
        add_msg( m_info, _( "%s helps with this task…" ), np->name );
    }
    moves = moves * ( 10 - helpers.size() ) / 10;

    p->assign_activity( ACT_BURROW, moves, -1, 0 );
    p->activity->placement = pnt;
    p->add_msg_if_player( _( "You start tearing into the %1$s with your %2$s." ),
                          g->m.tername( pnt ), it->tname() );
    return 0; // handled when the activity finishes
}

int iuse::geiger( player *p, item *it, bool t, const tripoint &pos )
{
    if( t ) { // Every-turn use when it's on
        const int rads = g->m.get_radiation( pos );
        if( rads == 0 ) {
            return it->type->charges_to_use();
        }
        std::string description = rads > 50 ? _( "buzzing" ) :
                                  rads > 25 ? _( "rapid clicking" ) : _( "clicking" );
        std::string sound_var = rads > 50 ? _( "geiger_high" ) :
                                rads > 25 ? _( "geiger_medium" ) : _( "geiger_low" );

        sounds::sound( pos, 6, sounds::sound_t::alarm, description, true, "tool", sound_var );
        if( !p->can_hear( pos, 6 ) ) {
            // can not hear it, but may have alarmed other creatures
            return it->type->charges_to_use();
        }
        if( rads > 50 ) {
            add_msg( m_warning, _( "The geiger counter buzzes intensely." ) );
        } else if( rads > 35 ) {
            add_msg( m_warning, _( "The geiger counter clicks wildly." ) );
        } else if( rads > 25 ) {
            add_msg( m_warning, _( "The geiger counter clicks rapidly." ) );
        } else if( rads > 15 ) {
            add_msg( m_warning, _( "The geiger counter clicks steadily." ) );
        } else if( rads > 8 ) {
            add_msg( m_warning, _( "The geiger counter clicks slowly." ) );
        } else if( rads > 4 ) {
            add_msg( _( "The geiger counter clicks intermittently." ) );
        } else {
            add_msg( _( "The geiger counter clicks once." ) );
        }
        return it->type->charges_to_use();
    }
    // Otherwise, we're activating the geiger counter
    if( it->typeId() == itype_geiger_on ) {
        add_msg( _( "The geiger counter's SCANNING LED turns off." ) );
        it->convert( itype_geiger_off );
        it->deactivate();
        return 0;
    }

    int ch = uilist( _( "Geiger counter:" ), {
        _( "Scan yourself or other person" ), _( "Scan the ground" ), _( "Turn continuous scan on" )
    } );
    switch( ch ) {
        case 0: {
            const std::function<bool( const tripoint & )> f = [&]( const tripoint & pnt ) {
                return g->critter_at<npc>( pnt ) != nullptr || g->critter_at<player>( pnt ) != nullptr;
            };

            const std::optional<tripoint> pnt_ = choose_adjacent_highlight( _( "Scan whom?" ),
                                                 _( "There is no one to scan nearby." ), f, false );
            if( !pnt_ ) {
                return 0;
            }
            const tripoint &pnt = *pnt_;
            if( pnt == g->u.pos() ) {
                p->add_msg_if_player( m_info, _( "Your radiation level: %d mSv (%d mSv from items)" ), p->get_rad(),
                                      p->leak_level( flag_RADIOACTIVE ) );
                break;
            }
            if( npc *const person_ = g->critter_at<npc>( pnt ) ) {
                npc &person = *person_;
                p->add_msg_if_player( m_info, _( "%s's radiation level: %d mSv (%d mSv from items)" ),
                                      person.name, person.get_rad(),
                                      person.leak_level( flag_RADIOACTIVE ) );
            }
            break;
        }
        case 1:
            p->add_msg_if_player( m_info, _( "The ground's radiation level: %d mSv/h" ),
                                  g->m.get_radiation( p->pos() ) );
            break;
        case 2:
            p->add_msg_if_player( _( "The geiger counter's scan LED turns on." ) );
            it->convert( itype_geiger_on );
            it->activate();
            break;
        default:
            return 0;
    }
    p->mod_moves( -100 );

    return it->type->charges_to_use();
}

int iuse::teleport( player *p, item *it, bool, const tripoint & )
{
    if( p->is_npc() ) {
        // That would be evil
        return 0;
    }
    if( p->is_mounted() ) {
        p->add_msg_if_player( m_info, _( "You cannot do that while mounted." ) );
        return 0;
    }
    if( !it->ammo_sufficient() ) {
        return 0;
    }
    p->moves -= to_moves<int>( 1_seconds );
    teleport::teleport( *p );
    return it->type->charges_to_use();
}

int iuse::can_goo( player *p, item *it, bool, const tripoint & )
{
    int tries = 0;
    tripoint goop;
    goop.z = p->posz();
    do {
        goop.x = p->posx() + rng( -2, 2 );
        goop.y = p->posy() + rng( -2, 2 );
        tries++;
    } while( g->m.impassable( goop ) && tries < 10 );
    if( tries == 10 ) {
        add_msg( _( "Nothing happens." ) );
        return 0;
    }
    if( monster *const mon_ptr = g->critter_at<monster>( goop ) ) {
        monster &critter = *mon_ptr;
        if( g->u.sees( goop ) ) {
            add_msg( _( "Black goo emerges from the canister and envelopes a %s!" ),
                     critter.name() );
        }
        critter.poly( mon_blob );

        critter.set_speed_base( critter.get_speed_base() - rng( 5, 25 ) );
        critter.set_hp( critter.get_speed() );
    } else {
        if( g->u.sees( goop ) ) {
            add_msg( _( "Living black goo emerges from the canister!" ) );
        }
        if( monster *const goo = g->place_critter_at( mon_blob, goop ) ) {
            goo->friendly = -1;
        }
    }
    if( x_in_y( 3.0, 4.0 ) ) {
        tries = 0;
        bool found = false;
        do {
            goop.x = p->posx() + rng( -2, 2 );
            goop.y = p->posy() + rng( -2, 2 );
            tries++;
            found = g->m.passable( goop ) && g->m.tr_at( goop ).is_null();
        } while( !found && tries < 10 );
        if( found ) {
            if( g->u.sees( goop ) ) {
                add_msg( m_warning, _( "A nearby splatter of goo forms into a goo pit." ) );
            }
            g->m.trap_set( goop, tr_goo );
        }
    }
    if( it->charges <= it->type->charges_to_use() ) {
        it->charges = 0;
        it->convert( itype_canister_empty );
        return 0;
    }
    return it->type->charges_to_use();
}

int iuse::throwable_extinguisher_act( player *, item *it, bool, const tripoint &pos )
{
    if( pos.x == -999 || pos.y == -999 ) {
        return 0;
    }
    if( g->m.get_field( pos, fd_fire ) != nullptr ) {
        sounds::sound( pos, 50, sounds::sound_t::combat, _( "Bang!" ), false, "explosion", "small" );
        // Reduce the strength of fire (if any) in the target tile.
        g->m.mod_field_intensity( pos, fd_fire, 0 - 2 );
        // Slightly reduce the strength of fire around and in the target tile.
        for( const tripoint &dest : g->m.points_in_radius( pos, 1 ) ) {
            if( g->m.passable( dest ) && dest != pos ) {
                g->m.mod_field_intensity( dest, fd_fire, 0 - rng( 0, 2 ) );
            }
        }
        return 1;
    }
    it->deactivate();
    return 0;
}

int iuse::debug_grenade( player *p, item *it, bool, const tripoint & )
{
    p->add_msg_if_player( _( "You pull the pin on the %s." ), it->tname() );
    it->convert( itype_debug_grenade_act );
    it->charges = 5;
    it->activate();
    return it->type->charges_to_use();
}

int iuse::debug_grenade_act( player *p, item *it, bool t, const tripoint &pos )
{
    if( pos.x == -999 || pos.y == -999 ) {
        return 0;
    }
    if( t ) { // Simple timer effects
        // Vol 0 = only heard if you hold it
        sounds::sound( pos, 0, sounds::sound_t::electronic_speech, _( "Merged!" ),
                       true, "speech", it->typeId().str() );
    } else if( it->charges > 0 ) {
        p->add_msg_if_player( m_info, _( "You've already pulled the %s's pin, try throwing it instead." ),
                              it->tname() );
        return 0;
    }

    if( it->charges == 0 ) { // When that timer runs down...
        int explosion_radius = 3;
        int effect_roll = rng( 1, 5 );
        auto buff_stat = [&]( int &current_stat, int modify_by ) {
            auto modified_stat = current_stat + modify_by;
            current_stat = std::max( current_stat, std::min( 15, modified_stat ) );
        };
        switch( effect_roll ) {
            case 1:
                sounds::sound( pos, 100, sounds::sound_t::electronic_speech, _( "BUGFIXES!" ),
                               true, "speech", it->typeId().str() );
                explosion_handler::draw_explosion( pos, explosion_radius, c_light_cyan, "explosion" );
                for( const tripoint &dest : g->m.points_in_radius( pos, explosion_radius ) ) {
                    monster *const mon = g->critter_at<monster>( dest, true );
                    if( mon && ( mon->type->in_species( INSECT ) || mon->is_hallucination() ) ) {
                        mon->die_in_explosion( nullptr );
                    }
                }
                break;

            case 2:
                sounds::sound( pos, 100, sounds::sound_t::electronic_speech, _( "BUFFS!" ),
                               true, "speech", it->typeId().str() );
                explosion_handler::draw_explosion( pos, explosion_radius, c_green, "explosion" );
                for( const tripoint &dest : g->m.points_in_radius( pos, explosion_radius ) ) {
                    if( monster *const mon_ptr = g->critter_at<monster>( dest ) ) {
                        monster &critter = *mon_ptr;
                        critter.set_speed_base(
                            critter.get_speed_base() * rng_float( 1.1, 2.0 ) );
                        critter.set_hp( critter.get_hp() * rng_float( 1.1, 2.0 ) );
                    } else if( npc *const person = g->critter_at<npc>( dest ) ) {
                        /** @EFFECT_STR_MAX increases possible granade str buff for NPCs */
                        buff_stat( person->str_max, rng( 0, person->str_max / 2 ) );
                        /** @EFFECT_DEX_MAX increases possible granade dex buff for NPCs */
                        buff_stat( person->dex_max, rng( 0, person->dex_max / 2 ) );
                        /** @EFFECT_INT_MAX increases possible granade int buff for NPCs */
                        buff_stat( person->int_max, rng( 0, person->int_max / 2 ) );
                        /** @EFFECT_PER_MAX increases possible granade per buff for NPCs */
                        buff_stat( person->per_max, rng( 0, person->per_max / 2 ) );
                    } else if( g->u.pos() == dest ) {
                        /** @EFFECT_STR_MAX increases possible granade str buff */
                        buff_stat( g->u.str_max, rng( 0, g->u.str_max / 2 ) );
                        /** @EFFECT_DEX_MAX increases possible granade dex buff */
                        buff_stat( g->u.dex_max, rng( 0, g->u.dex_max / 2 ) );
                        /** @EFFECT_INT_MAX increases possible granade int buff */
                        buff_stat( g->u.int_max, rng( 0, g->u.int_max / 2 ) );
                        /** @EFFECT_PER_MAX increases possible granade per buff */
                        buff_stat( g->u.per_max, rng( 0, g->u.per_max / 2 ) );
                        g->u.recalc_hp();
                        for( const bodypart_id &bp : g->u.get_all_body_parts() ) {
                            g->u.set_part_hp_cur( bp, g->u.get_part_hp_cur( bp ) * rng_float( 1, 1.2 ) );
                            const int hp_max = g->u.get_part_hp_max( bp );
                            if( g->u.get_part_hp_cur( bp ) > hp_max ) {
                                g->u.set_part_hp_cur( bp, hp_max );
                            }
                        }
                    }
                }
                break;

            case 3:
                sounds::sound( pos, 100, sounds::sound_t::electronic_speech, _( "NERFS!" ),
                               true, "speech", it->typeId().str() );
                explosion_handler::draw_explosion( pos, explosion_radius, c_red, "explosion" );
                for( const tripoint &dest : g->m.points_in_radius( pos, explosion_radius ) ) {
                    if( monster *const mon_ptr = g->critter_at<monster>( dest ) ) {
                        monster &critter = *mon_ptr;
                        critter.set_speed_base(
                            rng( 0, critter.get_speed_base() ) );
                        critter.set_hp( rng( 1, critter.get_hp() ) );
                    } else if( npc *const person = g->critter_at<npc>( dest ) ) {
                        /** @EFFECT_STR_MAX increases possible granade str debuff for NPCs (NEGATIVE) */
                        person->str_max -= rng( 0, person->str_max / 2 );
                        /** @EFFECT_DEX_MAX increases possible granade dex debuff for NPCs (NEGATIVE) */
                        person->dex_max -= rng( 0, person->dex_max / 2 );
                        /** @EFFECT_INT_MAX increases possible granade int debuff for NPCs (NEGATIVE) */
                        person->int_max -= rng( 0, person->int_max / 2 );
                        /** @EFFECT_PER_MAX increases possible granade per debuff for NPCs (NEGATIVE) */
                        person->per_max -= rng( 0, person->per_max / 2 );
                    } else if( g->u.pos() == dest ) {
                        /** @EFFECT_STR_MAX increases possible granade str debuff (NEGATIVE) */
                        g->u.str_max -= rng( 0, g->u.str_max / 2 );
                        /** @EFFECT_DEX_MAX increases possible granade dex debuff (NEGATIVE) */
                        g->u.dex_max -= rng( 0, g->u.dex_max / 2 );
                        /** @EFFECT_INT_MAX increases possible granade int debuff (NEGATIVE) */
                        g->u.int_max -= rng( 0, g->u.int_max / 2 );
                        /** @EFFECT_PER_MAX increases possible granade per debuff (NEGATIVE) */
                        g->u.per_max -= rng( 0, g->u.per_max / 2 );
                        g->u.recalc_hp();
                        for( const bodypart_id &bp : g->u.get_all_body_parts() ) {
                            const int hp_cur = g->u.get_part_hp_cur( bp );
                            if( hp_cur > 0 ) {
                                g->u.set_part_hp_cur( bp, rng( 1, hp_cur ) );
                            }
                        }
                    }
                }
                break;

            case 4:
                sounds::sound( pos, 100, sounds::sound_t::electronic_speech, _( "REVERTS!" ),
                               true, "speech", it->typeId().str() );
                explosion_handler::draw_explosion( pos, explosion_radius, c_pink, "explosion" );
                for( const tripoint &dest : g->m.points_in_radius( pos, explosion_radius ) ) {
                    if( monster *const mon_ptr = g->critter_at<monster>( dest ) ) {
                        monster &critter = *mon_ptr;
                        critter.set_speed_base( critter.type->speed );
                        critter.set_hp( critter.get_hp_max() );
                        critter.clear_effects();
                    } else if( npc *const person = g->critter_at<npc>( dest ) ) {
                        person->environmental_revert_effect();
                    } else if( g->u.pos() == dest ) {
                        g->u.environmental_revert_effect();
                        do_purify( g->u );
                    }
                }
                break;
            case 5:
                sounds::sound( pos, 100, sounds::sound_t::electronic_speech, _( "BEES!" ),
                               true, "speech", it->typeId().str() );
                explosion_handler::draw_explosion( pos, explosion_radius, c_yellow, "explosion" );
                for( const tripoint &dest : g->m.points_in_radius( pos, explosion_radius ) ) {
                    if( one_in( 5 ) && !g->critter_at( dest ) ) {
                        g->m.add_field( dest, fd_bees, rng( 1, 3 ) );
                    }
                }
                break;
        }
    }
    return it->type->charges_to_use();
}

int iuse::c4( player *p, item *it, bool, const tripoint & )
{
    int time;
    bool got_value = query_int( time, _( "Set the timer to (0 to cancel)?" ) );
    if( !got_value || time <= 0 ) {
        p->add_msg_if_player( _( "Never mind." ) );
        return 0;
    }
    p->add_msg_if_player( _( "You set the timer to %d." ), time );
    it->convert( itype_c4armed );
    it->charges = time;
    it->activate();
    return it->type->charges_to_use();
}

int iuse::acidbomb_act( player *p, item *it, bool, const tripoint &pos )
{
    if( !p->has_item( *it ) ) {
        it->charges = -1;
        for( const tripoint &tmp : g->m.points_in_radius( pos.x == -999 ? p->pos() : pos, 1 ) ) {
            g->m.add_field( tmp, fd_acid, 3 );
        }
        return 1;
    }
    return 0;
}

int iuse::grenade_inc_act( player *p, item *it, bool t, const tripoint &pos )
{
    if( pos.x == -999 || pos.y == -999 ) {
        return 0;
    }


    if( t ) {
        // Simple timer effects
        // Vol 0 = only heard if you hold it
        sounds::sound( pos, 0, sounds::sound_t::alarm, _( "Tick!" ), true, "misc", "bomb_ticking" );
    } else if( it->charges > 0 ) {
        p->add_msg_if_player( m_info, _( "You've already released the handle, try throwing it instead." ) );
        return 0;
    }

    if( it->charges == 0 ) { // blow up
        int num_flames = rng( 3, 5 );
        for( int current_flame = 0; current_flame < num_flames; current_flame++ ) {
            tripoint dest( pos + point( rng( -5, 5 ), rng( -5, 5 ) ) );
            std::vector<tripoint> flames = line_to( pos, dest, 0, 0 );
            for( auto &flame : flames ) {
                g->m.add_field( flame, fd_fire, rng( 0, 2 ) );
            }
        }
        explosion_handler::explosion( pos, p, 8, 0.8, true );
        for( const tripoint &dest : g->m.points_in_radius( pos, 2 ) ) {
            g->m.add_field( dest, fd_incendiary, 3 );
        }

        if( p->has_trait( trait_PYROMANIA ) ) {
            p->add_morale( MORALE_PYROMANIA_STARTFIRE, 15, 15, 8_hours, 6_hours );
            p->rem_morale( MORALE_PYROMANIA_NOFIRE );
            p->add_msg_if_player( m_good, _( "Fire…  Good…" ) );
        }
        return 0;
    }
    return 0;
}

int iuse::arrow_flammable( player *p, item *it, bool, const tripoint & )
{
    if( p->is_underwater() ) {
        p->add_msg_if_player( m_info, _( "You can't do that while underwater." ) );
        return 0;
    }
    if( !p->use_charges_if_avail( itype_fire, 1 ) ) {
        p->add_msg_if_player( m_info, _( "You need a source of fire!" ) );
        return 0;
    }
    p->add_msg_if_player( _( "You light the arrow!" ) );
    p->moves -= to_moves<int>( 1_seconds );
    if( it->charges == 1 ) {
        it->convert( itype_arrow_flamming );
        return 0;
    }
    detached_ptr<item> lit_arrow = item::spawn( *it );
    lit_arrow->convert( itype_arrow_flamming );
    lit_arrow->charges = 1;
    p->i_add( std::move( lit_arrow ) );
    return 1;
}

int iuse::molotov_lit( player *p, item *it, bool t, const tripoint &pos )
{
    if( pos.x == -999 || pos.y == -999 ) {
        return 0;
    } else if( !t ) {
        if( p->has_item( *it ) ) {
            if( !query_yn( "Really smash it on yourself?" ) ) {
                p->add_msg_if_player( m_info, _( "You should probably throw it instead." ) );
                return 0;
            }
        }
        for( const tripoint &pt : g->m.points_in_radius( pos, 1, 0 ) ) {
            const int intensity = 1 + one_in( 3 ) + one_in( 5 );
            g->m.add_field( pt, fd_fire, intensity );
        }
        if( p->has_trait( trait_PYROMANIA ) ) {
            p->add_morale( MORALE_PYROMANIA_STARTFIRE, 15, 15, 8_hours, 6_hours );
            p->rem_morale( MORALE_PYROMANIA_NOFIRE );
            p->add_msg_if_player( m_good, _( "Fire…  Good…" ) );
        }
        // If you exploded it on yourself through activation.
        return 1;
    } else if( p->has_item( *it ) && it->charges == 0 ) {
        return 0;
    }
    return 0;
}

int iuse::firecracker_pack( player *p, item *it, bool, const tripoint & )
{
    if( p->is_underwater() ) {
        p->add_msg_if_player( m_info, _( "You can't do that while underwater." ) );
        return 0;
    }
    if( !p->has_charges( itype_fire, 1 ) ) {
        p->add_msg_if_player( m_info, _( "You need a source of fire!" ) );
        return 0;
    }
    p->add_msg_if_player( _( "You light the pack of firecrackers." ) );
    it->convert( itype_firecracker_pack_act );
    it->charges = 26;
    it->set_age( 0_turns );
    it->activate();
    return 0; // don't use any charges at all. it has became a new item
}

int iuse::firecracker_pack_act( player *, item *it, bool, const tripoint &pos )
{
    time_duration timer = it->age();
    if( timer < 2_turns ) {
        sounds::sound( pos, 0, sounds::sound_t::alarm, _( "ssss…" ), true, "misc", "lit_fuse" );
        it->inc_damage();
    } else if( it->charges > 0 ) {
        int ex = rng( 4, 6 );
        int i = 0;
        if( ex > it->charges ) {
            ex = it->charges;
        }
        for( i = 0; i < ex; i++ ) {
            sounds::sound( pos, 20, sounds::sound_t::combat, _( "Bang!" ), false, "explosion", "small" );
        }
        it->charges -= ex;
    }
    if( it->charges == 0 ) {
        it->charges = -1;
    }
    return 0;
}

int iuse::firecracker( player *p, item *it, bool, const tripoint & )
{
    if( p->is_underwater() ) {
        p->add_msg_if_player( m_info, _( "You can't do that while underwater." ) );
        return 0;
    }
    if( !p->use_charges_if_avail( itype_fire, 1 ) ) {
        p->add_msg_if_player( m_info, _( "You need a source of fire!" ) );
        return 0;
    }
    p->add_msg_if_player( _( "You light the firecracker." ) );
    it->convert( itype_firecracker_act );
    it->charges = 2;
    it->activate();
    return it->type->charges_to_use();
}

int iuse::firecracker_act( player *p, item *it, bool t, const tripoint &pos )
{
    if( pos.x == -999 || pos.y == -999 ) {
        return 0;
    }

    if( t ) { // Simple timer effects
        sounds::sound( pos, 0, sounds::sound_t::alarm, _( "ssss…" ), true, "misc", "lit_fuse" );
    } else if( it->charges > 0 ) {
        p->add_msg_if_player( m_info, _( "You've already lit the %s, try throwing it instead." ),
                              it->tname() );
        return 0;
    }

    if( it->charges == 0 ) { // When that timer runs down...
        sounds::sound( pos, 20, sounds::sound_t::combat, _( "Bang!" ), true, "explosion", "small" );
    }
    return 0;
}

int iuse::mininuke( player *p, item *it, bool, const tripoint & )
{
    int time;
    bool got_value = query_int( time, _( "Set the timer to ___ turns (0 to cancel)?" ) );
    if( !got_value || time <= 0 ) {
        p->add_msg_if_player( _( "Never mind." ) );
        return 0;
    }
    p->add_msg_if_player( _( "You set the timer to %s." ),
                          to_string( time_duration::from_turns( time ) ) );
    g->events().send<event_type::activates_mininuke>( p->getID() );
    it->convert( itype_mininuke_act );
    it->charges = time;
    it->activate();
    return it->type->charges_to_use();
}

int iuse::pheromone( player *p, item *it, bool, const tripoint &pos )
{
    if( !it->ammo_sufficient() ) {
        return 0;
    }
    if( p->is_underwater() ) {
        p->add_msg_if_player( m_info, _( "You can't do that while underwater." ) );
        return 0;
    }

    if( pos.x == -999 || pos.y == -999 ) {
        return 0;
    }

    p->add_msg_player_or_npc( _( "You squeeze the pheromone ball…" ),
                              _( "<npcname> squeezes the pheromone ball…" ) );

    p->moves -= 15;

    int converts = 0;
    for( const tripoint &dest : g->m.points_in_radius( pos, 4 ) ) {
        monster *const mon_ptr = g->critter_at<monster>( dest, true );
        if( !mon_ptr ) {
            continue;
        }
        monster &critter = *mon_ptr;
        if( critter.type->in_species( ZOMBIE ) && critter.friendly == 0 &&
            rng( 0, 500 ) > critter.get_hp() ) {
            converts++;
            critter.anger = 0;
        }
    }

    if( g->u.sees( *p ) ) {
        if( converts == 0 ) {
            add_msg( _( "…but nothing happens." ) );
        } else if( converts == 1 ) {
            add_msg( m_good, _( "…and a nearby zombie becomes passive!" ) );
        } else {
            add_msg( m_good, _( "…and several nearby zombies become passive!" ) );
        }
    }
    return it->type->charges_to_use();
}

int iuse::portal( player *p, item *it, bool, const tripoint & )
{
    if( !it->ammo_sufficient() ) {
        return 0;
    }
    if( p->is_mounted() ) {
        p->add_msg_if_player( m_info, _( "You cannot do that while mounted." ) );
        return 0;
    }
    tripoint t( p->posx() + rng( -2, 2 ), p->posy() + rng( -2, 2 ), p->posz() );
    g->m.trap_set( t, tr_portal );
    return it->type->charges_to_use();
}

int iuse::tazer( player *p, item *it, bool, const tripoint &pos )
{
    if( !it->units_sufficient( *p ) ) {
        return 0;
    }

    tripoint pnt = pos;
    if( pos == p->pos() ) {
        const std::optional<tripoint> pnt_ = choose_adjacent( _( "Shock where?" ) );
        if( !pnt_ ) {
            return 0;
        }
        pnt = *pnt_;
    }

    if( pnt == p->pos() ) {
        p->add_msg_if_player( m_info, _( "Umm.  No." ) );
        return 0;
    }

    Creature *target = g->critter_at( pnt, true );
    if( target == nullptr ) {
        p->add_msg_if_player( _( "There's nothing to zap there!" ) );
        return 0;
    }

    npc *foe = dynamic_cast<npc *>( target );
    if( foe != nullptr &&
        !foe->is_enemy() &&
        !p->query_yn( _( "Really shock %s?" ), target->disp_name() ) ) {
        return 0;
    }

    /** @EFFECT_DEX slightly increases chance of successfully using tazer */
    /** @EFFECT_MELEE increases chance of successfully using a tazer */
    int numdice = 3 + ( p->dex_cur / 2.5 ) + p->get_skill_level( skill_melee ) * 2;
    p->moves -= to_moves<int>( 1_seconds );

    /** @EFFECT_DODGE increases chance of dodging a tazer attack */
    const bool tazer_was_dodged = dice( numdice, 10 ) < dice( target->get_dodge(), 10 );
    if( tazer_was_dodged ) {
        p->add_msg_player_or_npc( _( "You attempt to shock %s, but miss." ),
                                  _( "<npcname> attempts to shock %s, but misses." ),
                                  target->disp_name() );
    } else {
        // TODO: Maybe - Execute an attack and maybe zap something other than torso
        // Maybe, because it's torso (heart) that fails when zapped with electricity
        int dam = target->deal_damage( p, bodypart_id( "torso" ), damage_instance( DT_ELECTRIC, rng( 5,
                                       25 ) ) ).total_damage();
        if( dam > 0 ) {
            p->add_msg_player_or_npc( m_good,
                                      _( "You shock %s!" ),
                                      _( "<npcname> shocks %s!" ),
                                      target->disp_name() );
        } else {
            p->add_msg_player_or_npc( m_warning,
                                      _( "You unsuccessfully attempt to shock %s!" ),
                                      _( "<npcname> unsuccessfully attempts to shock %s!" ),
                                      target->disp_name() );
        }
    }

    if( foe != nullptr ) {
        foe->on_attacked( *p );
    }

    return it->type->charges_to_use();
}

int iuse::tazer2( player *p, item *it, bool b, const tripoint &pos )
{
    if( it->ammo_remaining() >= 100 ) {
        // Instead of having a ctrl+c+v of the function above, spawn a fake tazer and use it
        // Ugly, but less so than copied blocks
        item *fake = item::spawn_temporary( "tazer", calendar::start_of_cataclysm );
        fake->charges = 100;
        return tazer( p, fake, b, pos );
    } else {
        p->add_msg_if_player( m_info, _( "Insufficient power" ) );
    }

    return 0;
}

static std::string get_music_description()
{
    const std::array<std::string, 5> descriptions = {{
            translate_marker( "a sweet guitar solo!" ),
            translate_marker( "a funky bassline." ),
            translate_marker( "some amazing vocals." ),
            translate_marker( "some pumping bass." ),
            translate_marker( "dramatic classical music." )
        }
    };

    if( one_in( 50 ) ) {
        return _( "some bass-heavy post-glam speed polka." );
    }

    size_t i = static_cast<size_t>( rng( 0, descriptions.size() * 2 ) );
    if( i < descriptions.size() ) {
        return _( descriptions[i] );
    }
    // Not one of the hard-coded versions, let's apply a random string made up
    // of snippets {a, b, c}, but only a 50% chance
    // Actual chance = 24.5% of being selected
    if( one_in( 2 ) ) {
        return SNIPPET.expand( SNIPPET.random_from_category( "<music_description>" ).value_or(
                                   translation() ).translated() );
    }

    return _( "a sweet guitar solo!" );
}

void iuse::play_music( player &p, const tripoint &source, const int volume, const int max_morale )
{
    // TODO: what about other "player", e.g. when a NPC is listening or when the PC is listening,
    // the other characters around should be able to profit as well.
    const bool do_effects = p.can_hear( source, volume ) && !p.has_effect( effect_sleep );
    std::string sound = "music";
    if( calendar::once_every( 1_hours ) ) {
        // Every 5 minutes, describe the music
        const std::string music = get_music_description();
        if( !music.empty() ) {
            sound = music;
            // descriptions aren't printed for sounds at our position
            if( p.pos() == source && p.can_hear( source, volume ) ) {
                p.add_msg_if_player( _( "You listen to %s" ), music );
            }
        }
    }
    // do not process mp3 player
    if( volume != 0 ) {
        sounds::ambient_sound( source, volume, sounds::sound_t::music, sound );
    }
    if( do_effects ) {
        p.add_effect( effect_music, 1_turns );
        p.add_morale( MORALE_MUSIC, 1, max_morale, 5_minutes, 2_minutes, true );
        // mp3 player reduces hearing
        if( volume == 0 ) {
            p.add_effect( effect_earphones, 1_turns );
        }
    }
}

int iuse::mp3_on( player *p, item *it, bool t, const tripoint &pos )
{
    if( t ) { // Normal use
        if( p->has_item( *it ) ) {
            // mp3 player in inventory, we can listen
            play_music( *p, pos, 0, 20 );
        }
    } else { // Turning it off
        // Creatively make it so that the reversion isn't hard-coded
        // There's *probably* a better way to do this, but this works
        std::string active_item = it->typeId().str();
        std::string base_item = active_item.erase( active_item.rfind( '_' ) );

        p->add_msg_if_player( _( "The %s turns off." ), it->display_name() );
        it->convert( itype_id( base_item ) );
        it->deactivate();

        p->mod_moves( -200 );
    }
    return it->type->charges_to_use();
}

int iuse::rpgdie( player *you, item *die, bool, const tripoint & )
{
    if( you->is_mounted() ) {
        you->add_msg_if_player( m_info, _( "You cannot do that while mounted." ) );
        return 0;
    }
    int num_sides = die->get_var( "die_num_sides", 0 );
    if( num_sides == 0 ) {
        const std::vector<int> sides_options = { 4, 6, 8, 10, 12, 20, 50 };
        const int sides = sides_options[ rng( 0, sides_options.size() - 1 ) ];
        num_sides = sides;
        die->set_var( "die_num_sides", sides );
    }
    const int roll = rng( 1, num_sides );
    //~ %1$d: roll number, %2$d: side number of a die, %3$s: die item name
    you->add_msg_if_player( pgettext( "dice", "You roll a %1$d on your %2$d sided %3$s" ), roll,
                            num_sides, die->tname() );
    if( roll == num_sides ) {
        add_msg( m_good, _( "Critical!" ) );
    }
    return 0;
}

int iuse::dive_tank( player *p, item *it, bool t, const tripoint & )
{
    if( t ) { // Normal use
        if( p->is_worn( *it ) ) {
            if( p->is_underwater() && p->oxygen < 10 ) {
                p->oxygen += 20;
            }
            if( one_in( 15 ) ) {
                p->add_msg_if_player( m_bad, _( "You take a deep breath from your %s." ), it->tname() );
            }
            if( it->charges == 0 ) {
                p->add_msg_if_player( m_bad, _( "Air in your %s runs out." ), it->tname() );
                it->set_var( "overwrite_env_resist", 0 );
                it->convert( itype_id( it->typeId().str().substr( 0,
                                       it->typeId().str().size() - 3 ) ) );
                it->deactivate(); // 3 = "_on"
            }
        } else { // not worn = off thanks to on-demand regulator
            it->set_var( "overwrite_env_resist", 0 );
            it->convert( itype_id( it->typeId().str().substr( 0,
                                   it->typeId().str().size() - 3 ) ) );
            it->deactivate(); // 3 = "_on"
        }

    } else { // Turning it on/off
        if( it->charges == 0 ) {
            p->add_msg_if_player( _( "Your %s is empty." ), it->tname() );
        } else if( it->is_active() ) { //off
            p->add_msg_if_player( _( "You turn off the regulator and close the air valve." ) );
            it->set_var( "overwrite_env_resist", 0 );
            it->convert( itype_id( it->typeId().str().substr( 0,
                                   it->typeId().str().size() - 3 ) ) );
            it->deactivate(); // 3 = "_on"
        } else { //on
            if( !p->is_worn( *it ) ) {
                p->add_msg_if_player( _( "You should wear it first." ) );
            } else {
                p->add_msg_if_player( _( "You turn on the regulator and open the air valve." ) );
                it->set_var( "overwrite_env_resist", it->get_base_env_resist_w_filter() );
                it->convert( itype_id( it->typeId().str() + "_on" ) );
                it->activate();
            }
        }
    }
    if( it->charges == 0 ) {
        it->set_var( "overwrite_env_resist", 0 );
        it->convert( itype_id( it->typeId().str().substr( 0,
                               it->typeId().str().size() - 3 ) ) );
        it->deactivate(); // 3 = "_on"
    }
    return it->type->charges_to_use();
}

int iuse::solarpack( player *p, item *it, bool, const tripoint & )
{
    const bionic_id rem_bid = p->get_remote_fueled_bionic();
    if( rem_bid.is_empty() ) {  // Cable CBM required
        p->add_msg_if_player(
            _( "You have no cable charging system to plug it in, so you leave it alone." ) );
        return 0;
    } else if( !p->has_active_bionic( rem_bid ) ) {  // when OFF it takes no effect
        p->add_msg_if_player( _( "Activate your cable charging system to take advantage of it." ) );
    }

    if( it->is_armor() && !( p->is_worn( *it ) ) ) {
        p->add_msg_if_player( m_neutral, _( "You need to wear the %1$s before you can unfold it." ),
                              it->tname() );
        return 0;
    }
    // no doubled sources of power
    if( p->worn_with_flag( flag_SOLARPACK_ON ) ) {
        p->add_msg_if_player( m_neutral, _( "You cannot use the %1$s with another of it's kind." ),
                              it->tname() );
        return 0;
    }
    p->add_msg_if_player(
        _( "You unfold solar array from the pack.  You still need to connect it with a cable." ) );

    it->convert( itype_id( it->typeId().str() + "_on" ) );
    return 0;
}

int iuse::solarpack_off( player *p, item *it, bool, const tripoint & )
{
    if( !p->is_worn( *it ) ) {  // folding when not worn
        p->add_msg_if_player( _( "You fold your portable solar array into the pack." ) );
    } else {
        p->add_msg_if_player( _( "You unplug and fold your portable solar array into the pack." ) );
    }

    // 3 = "_on"
    it->convert( itype_id( it->typeId().str().substr( 0,
                           it->typeId().str().size() - 3 ) ) );
    it->deactivate();
    return 0;
}

int iuse::gasmask( player *p, item *it, bool t, const tripoint &pos )
{
    if( t ) { // Normal use
        if( p->is_worn( *it ) ) {
            // calculate amount of absorbed gas per filter charge
            const field &gasfield = g->m.field_at( pos );
            for( auto &dfield : gasfield ) {
                const field_entry &entry = dfield.second;
                if( entry.get_gas_absorption_factor() > 0 ) {
                    it->set_var( "gas_absorbed", it->get_var( "gas_absorbed", 0 ) + entry.get_gas_absorption_factor() );
                }
            }
            if( it->get_var( "gas_absorbed", 0 ) >= 100 ) {
                it->ammo_consume( 1, p->pos() );
                it->set_var( "gas_absorbed", 0 );
            }
            if( it->charges == 0 ) {
                p->add_msg_player_or_npc(
                    m_bad,
                    _( "Your %s requires new filter!" ),
                    _( "<npcname> needs new gas mask filter!" )
                    , it->tname() );
            }
        }
    } else { // activate
        if( it->charges == 0 ) {
            p->add_msg_if_player( _( "Your %s don't have a filter." ), it->tname() );
        } else {
            p->add_msg_if_player( _( "You prepared your %s." ), it->tname() );
            it->activate();
            it->set_var( "overwrite_env_resist", it->get_base_env_resist_w_filter() );
        }
    }
    if( it->charges == 0 ) {
        it->set_var( "overwrite_env_resist", 0 );
        it->deactivate();
    }
    return it->type->charges_to_use();
}

int iuse::portable_game( player *p, item *it, bool t, const tripoint & )
{
    if( t ) {
        return 0;
    }
    if( p->is_npc() ) {
        // Long action
        return 0;
    }
    if( p->is_mounted() ) {
        p->add_msg_if_player( m_info, _( "You cannot do that while mounted." ) );
        return 0;
    }
    if( p->is_underwater() ) {
        p->add_msg_if_player( m_info, _( "You can't do that while underwater." ) );
        return 0;
    }
    if( p->has_trait( trait_ILLITERATE ) ) {
        p->add_msg_if_player( m_info, _( "You're illiterate!" ) );
        return 0;
    } else if( it->units_remaining( *p ) < ( it->ammo_required() * 15 ) ) {
        p->add_msg_if_player( m_info, _( "You don't have enough charges to play." ) );
        return 0;
    } else {
        std::string loaded_software = "robot_finds_kitten";

        uilist as_m;
        as_m.text = _( "What do you want to play?" );
        as_m.entries.emplace_back( 1, true, '1', _( "Robot finds Kitten" ) );
        as_m.entries.emplace_back( 2, true, '2', _( "S N A K E" ) );
        as_m.entries.emplace_back( 3, true, '3', _( "Sokoban" ) );
        as_m.entries.emplace_back( 4, true, '4', _( "Minesweeper" ) );
        as_m.entries.emplace_back( 5, true, '5', _( "Lights on!" ) );
        as_m.entries.emplace_back( 6, true, '6', _( "Play anything for a while" ) );
        as_m.query();

        switch( as_m.ret ) {
            case 1:
                loaded_software = "robot_finds_kitten";
                break;
            case 2:
                loaded_software = "snake_game";
                break;
            case 3:
                loaded_software = "sokoban_game";
                break;
            case 4:
                loaded_software = "minesweeper_game";
                break;
            case 5:
                loaded_software = "lightson_game";
                break;
            case 6:
                loaded_software = "null";
                break;
            default:
                //Cancel
                return 0;
        }

        //Play in 15-minute chunks
        const int moves = to_moves<int>( 15_minutes );

        p->add_msg_if_player( _( "You play on your %s for a while." ), it->tname() );
        p->assign_activity( ACT_GAME, moves, -1, 0, "gaming" );
        p->activity->targets.emplace_back( it );
        std::string end_message;
        end_message.clear();
        int game_score = 0;

        play_videogame( loaded_software, end_message, game_score );

        if( !end_message.empty() ) {
            p->add_msg_if_player( end_message );
        }

        if( game_score != 0 ) {
            p->add_morale( MORALE_GAME, game_score, 60, 2_hours, 30_minutes, true );
        }
    }
    return 0;
}

int iuse::hand_crank( player *p, item *it, bool, const tripoint & )
{
    if( p->is_npc() ) {
        // Long action
        return 0;
    }
    if( p->is_underwater() ) {
        p->add_msg_if_player( m_info, _( "It's not waterproof enough to work underwater." ) );
        return 0;
    }
    if( p->get_fatigue() >= fatigue_levels::dead_tired ) {
        p->add_msg_if_player( m_info, _( "You're too exhausted to keep cranking." ) );
        return 0;
    }
    item *magazine = it->magazine_current();
    if( magazine && magazine->has_flag( flag_RECHARGE ) ) {
        // 1600 minutes. It shouldn't ever run this long, but it's an upper bound.
        // expectation is it runs until the player is too tired.
        int moves = to_moves<int>( 1600_minutes );
        if( it->ammo_capacity() > it->ammo_remaining() ) {
            p->add_msg_if_player( _( "You start cranking the %s to charge its %s." ), it->tname(),
                                  it->magazine_current()->tname() );
            p->assign_activity( ACT_HAND_CRANK, moves, -1, 0, "hand-cranking" );
            p->activity->tools.emplace_back( it );
        } else {
            p->add_msg_if_player( _( "You could use the %s to charge its %s, but it's already charged." ),
                                  it->tname(), magazine->tname() );
        }
    } else {
        p->add_msg_if_player( m_info, _( "You need a rechargeable battery cell to charge." ) );
    }
    return 0;
}

int iuse::vibe( player *p, item *it, bool, const tripoint & )
{
    if( p->is_npc() ) {
        // Long action
        // Also, that would be creepy as fuck, seriously
        return 0;
    }
    if( p->is_mounted() ) {
        p->add_msg_if_player( m_info, _( "You cannot do… that while mounted." ) );
        return 0;
    }
    if( ( p->is_underwater() ) && ( !( ( p->has_trait( trait_GILLS ) ) ||
                                       ( p->has_trait( trait_GILLS_CEPH ) ) ||
                                       ( p->is_wearing( itype_rebreather_on ) ) ||
                                       ( p->is_wearing( itype_rebreather_xl_on ) ) ||
                                       ( p->is_wearing( itype_mask_h20survivor_on ) ) ) ) ) {
        p->add_msg_if_player( m_info, _( "It's waterproof, but oxygen maybe?" ) );
        return 0;
    }
    if( !it->units_sufficient( *p ) ) {
        p->add_msg_if_player( m_info, _( "The %s's batteries are dead." ), it->tname() );
        return 0;
    }
    if( p->get_fatigue() >= fatigue_levels::dead_tired ) {
        p->add_msg_if_player( m_info, _( "*Your* batteries are dead." ) );
        return 0;
    } else {
        int moves = to_moves<int>( 20_minutes );
        if( it->ammo_remaining() > 0 ) {
            p->add_msg_if_player( _( "You fire up your %s and start getting the tension out." ),
                                  it->tname() );
        } else {
            p->add_msg_if_player( _( "You whip out your %s and start getting the tension out." ),
                                  it->tname() );
        }
        p->assign_activity( ACT_VIBE, moves, -1, 0, "de-stressing" );
        p->activity->tools.emplace_back( it );
    }
    return it->type->charges_to_use();
}

int iuse::vortex( player *p, item *it, bool, const tripoint & )
{
    std::vector<point> spawn;
    for( int i = -3; i <= 3; i++ ) {
        spawn.emplace_back( -3, i );
        spawn.emplace_back( +3, i );
        spawn.emplace_back( i, -3 );
        spawn.emplace_back( i, +3 );
    }

    while( !spawn.empty() ) {
        const tripoint offset( random_entry_removed( spawn ), 0 );
        monster *const mon = g->place_critter_at( mon_vortex, offset + p->pos() );
        if( !mon ) {
            continue;
        }
        p->add_msg_if_player( m_warning, _( "Air swirls all over…" ) );
        p->moves -= to_moves<int>( 1_seconds );
        it->convert( itype_spiral_stone );
        mon->friendly = -1;
        return it->type->charges_to_use();
    }

    // Only reachable when no monster has been spawned.
    p->add_msg_if_player( m_warning, _( "Air swirls around you for a moment." ) );
    it->convert( itype_spiral_stone );
    return it->type->charges_to_use();
}

int iuse::dog_whistle( player *p, item *it, bool, const tripoint & )
{
    if( p->is_underwater() ) {
        p->add_msg_if_player( m_info, _( "You can't do that while underwater." ) );
        return 0;
    }
    p->add_msg_if_player( _( "You blow your dog whistle." ) );
    for( monster &critter : g->all_monsters() ) {
        if( critter.friendly != 0 && critter.has_flag( MF_DOGFOOD ) ) {
            bool u_see = g->u.sees( critter );
            if( critter.has_effect( effect_docile ) ) {
                if( u_see ) {
                    p->add_msg_if_player( _( "Your %s looks ready to attack." ), critter.name() );
                }
                critter.remove_effect( effect_docile );
            } else {
                if( u_see ) {
                    p->add_msg_if_player( _( "Your %s goes docile." ), critter.name() );
                }
                critter.add_effect( effect_docile, 1_turns );
            }
        }
    }
    return it->type->charges_to_use();
}

int iuse::call_of_tindalos( player *p, item *it, bool, const tripoint & )
{
    for( const tripoint &dest : g->m.points_in_radius( p->pos(), 12 ) ) {
        if( g->m.is_cornerfloor( dest ) ) {
            g->m.add_field( dest, fd_tindalos_rift, 3 );
            add_msg( m_info, _( "You hear a low-pitched echoing howl." ) );
        }
    }
    return it->type->charges_to_use();
}

int iuse::blood_draw( player *p, item *it, bool, const tripoint & )
{
    if( p->is_npc() ) {
        return 0;    // No NPCs for now!
    }
    if( p->is_mounted() ) {
        p->add_msg_if_player( m_info, _( "You cannot do that while mounted." ) );
        return 0;
    }
    if( !it->contents.empty() ) {
        p->add_msg_if_player( m_info, _( "That %s is full!" ), it->tname() );
        return 0;
    }

    const mtype *mt = nullptr;
    bool drew_blood = false;
    bool acid_blood = false;
    for( auto &map_it : g->m.i_at( point( p->posx(), p->posy() ) ) ) {
        if( map_it->is_corpse() ) {
            bool has_blood = false;
            mt = map_it->get_mtype();
            if( mt != nullptr ) {
                for( const harvest_entry &entry : mt->harvest.obj() ) {
                    if( entry.type == "blood" ) {
                        has_blood = true;
                    }
                }
            }
            if( !has_blood ) {
                p->add_msg_if_player( m_info, _( "The %s doesn't seem to have any blood to draw." ),
                                      map_it->tname() );
                break;
            }
            if( map_it->has_flag( flag_BLED ) ) {
                p->add_msg_if_player( m_info, _( "That %s has already been bled dry." ), map_it->tname() );
                break;
            }
            if( query_yn( _( "Draw blood from %s?" ),
                          colorize( map_it->tname(), map_it->color_in_inventory() ) ) ) {
                // No real way to track and deplete the max potential yield of blood so just randomize
                if( one_in( 10 ) ) {
                    map_it->set_flag( flag_BLED );
                    p->add_msg_if_player( m_info, _( "You drained the last dregs of blood from the %s…" ),
                                          map_it->tname() );
                } else {
                    p->add_msg_if_player( m_info, _( "You drew blood from the %s…" ), map_it->tname() );
                }
                drew_blood = true;
                auto bloodtype( map_it->get_mtype()->bloodType() );
                if( bloodtype.obj().has_acid ) {
                    acid_blood = true;
                } else {
                    // Checking again here to actually get the detached_ptr
                    for( const harvest_entry &entry : mt->harvest.obj() ) {
                        if( entry.type == "blood" ) {
                            detached_ptr<item> blood = item::spawn( entry.drop, map_it->birthday() );

                            if( !liquid_handler::handle_liquid( std::move( blood ), 1 ) ) {
                                // NOLINTNEXTLINE(bugprone-use-after-move)
                                it->put_in( std::move( blood ) );
                            }
                            return it->type->charges_to_use();
                        }
                    }
                }
                break;
            }
        }
    }

    if( !drew_blood && query_yn( _( "Draw your own blood?" ) ) ) {
        p->add_msg_if_player( m_info, _( "You drew your own blood…" ) );
        drew_blood = true;
        detached_ptr<item> blood = item::spawn( "blood", calendar::turn );
        if( p->has_trait( trait_ACIDBLOOD ) ) {
            acid_blood = true;
        }
        p->mod_stored_nutr( 10 );
        p->mod_thirst( 10 );
        p->mod_pain( 3 );
        if( !liquid_handler::handle_liquid( std::move( blood ), 1 ) ) {
            // NOLINTNEXTLINE(bugprone-use-after-move)
            it->put_in( std::move( blood ) );
        }
        return it->type->charges_to_use();
    }

    if( acid_blood ) {
        detached_ptr<item> acid = item::spawn( "acid", calendar::turn );
        if( one_in( 3 ) ) {
            if( it->inc_damage( DT_ACID ) ) {
                p->add_msg_if_player( m_info, _( "…but acidic blood melts the %s, destroying it!" ),
                                      it->tname() );
                it->detach();
                return 0;
            }
            p->add_msg_if_player( m_info, _( "…but acidic blood damages the %s!" ), it->tname() );
        }
        if( !liquid_handler::handle_liquid( std::move( acid ), 1 ) ) {
            // NOLINTNEXTLINE(bugprone-use-after-move)
            it->put_in( std::move( acid ) );
        }
        return it->type->charges_to_use();
    }

    return it->type->charges_to_use();
}

//This is just used for robofac_intercom_mission_2
int iuse::mind_splicer( player *p, item *it, bool, const tripoint & )
{
    if( p->is_mounted() ) {
        p->add_msg_if_player( m_info, _( "You cannot do that while mounted." ) );
        return 0;
    }
    for( auto &map_it : g->m.i_at( point( p->posx(), p->posy() ) ) ) {
        if( map_it->typeId() == itype_rmi2_corpse &&
            query_yn( _( "Use the mind splicer kit on the %s?" ), colorize( map_it->tname(),
                      map_it->color_in_inventory() ) ) ) {

            auto filter = []( const item & it ) {
                return it.typeId() == itype_data_card;
            };
            avatar *you = p->as_avatar();
            item *loc = nullptr;
            if( you != nullptr ) {
                loc = game_menus::inv::titled_filter_menu( filter, *you, _( "Select storage media" ) );
            }
            if( !loc ) {
                add_msg( m_info, _( "Nevermind." ) );
                return 0;
            }
            item &data_card = *loc;
            ///\EFFECT_DEX makes using the mind splicer faster
            ///\EFFECT_FIRSTAID makes using the mind splicer faster
            const time_duration time = std::max( 150_minutes - 20_minutes * ( p->get_skill_level(
                    skill_firstaid ) - 1 ) - 10_minutes * ( p->get_dex() - 8 ), 30_minutes );

            std::unique_ptr<player_activity> act = std::make_unique<player_activity>( ACT_MIND_SPLICER,
                                                   to_moves<int>( time ) );
            act->targets.emplace_back( &data_card );
            p->assign_activity( std::move( act ) );
            return it->type->charges_to_use();
        }
    }
    add_msg( m_info, _( "There's nothing to use the %s on here." ), it->tname() );
    return 0;
}

void iuse::cut_log_into_planks( player &p )
{
    if( p.is_mounted() ) {
        p.add_msg_if_player( m_info, _( "You cannot do that while mounted." ) );
        return;
    }
    const int moves = to_moves<int>( 20_minutes );
    p.add_msg_if_player( _( "You cut the log into planks." ) );

    p.assign_activity( ACT_CHOP_PLANKS, moves, -1 );
    p.activity->placement = g->m.getabs( p.pos() );
}

int iuse::lumber( player *p, item *it, bool t, const tripoint & )
{
    if( t ) {
        return 0;
    }
    if( p->is_mounted() ) {
        p->add_msg_if_player( m_info, _( "You cannot do that while mounted." ) );
        return 0;
    }
    // Check if player is standing on any lumber
    item *standing = nullptr;
    for( auto &i : g->m.i_at( p->pos() ) ) {
        if( i->typeId() == itype_log ) {
            standing = &*i;
            break;
        }
    }
    if( standing ) {
        g->m.i_rem( p->pos(), standing );
        cut_log_into_planks( *p );
        return it->type->charges_to_use();
    }

    // If the player is not standing on a log, check inventory
    avatar *you = p->as_avatar();
    item *loc = nullptr;
    auto filter = []( const item & it ) {
        return it.typeId() == itype_log;
    };
    if( you != nullptr ) {
        loc = game_menus::inv::titled_filter_menu( filter, *you, _( "Cut up what?" ) );
    }

    if( !loc ) {
        p->add_msg_if_player( m_info, _( "You do not have that item!" ) );
        return 0;
    }
    loc->detach();
    cut_log_into_planks( *p );
    return it->type->charges_to_use();
}

int iuse::chop_moves( Character &ch, item &tool )
{
    // quality of tool
    const int quality = tool.get_quality( qual_AXE );

    // attribute; regular tools - based on STR, powered tools - based on DEX
    const int attr = tool.has_flag( flag_POWERED ) ? ch.dex_cur : ch.str_cur;

    return to_moves<int>( std::max( 10_minutes, time_duration::from_minutes( 60 - attr ) / quality ) );
}

int iuse::chop_tree( player *p, item *it, bool t, const tripoint & )
{
    if( !p || t ) {
        return 0;
    }
    if( p->is_mounted() ) {
        p->add_msg_if_player( m_info, _( "You cannot do that while mounted." ) );
        return 0;
    }
    const std::function<bool( const tripoint & )> f = []( const tripoint & pnt ) {
        if( pnt == g->u.pos() ) {
            return false;
        }
        return g->m.has_flag( "TREE", pnt );
    };

    const std::optional<tripoint> pnt_ = choose_adjacent_highlight(
            _( "Chop down which tree?" ), _( "There is no tree to chop down nearby." ), f, false );
    if( !pnt_ ) {
        return 0;
    }
    const tripoint &pnt = *pnt_;
    if( !f( pnt ) ) {
        if( pnt == p->pos() ) {
            p->add_msg_if_player( m_info, _( "You're not stern enough to shave yourself with THIS." ) );
        } else {
            p->add_msg_if_player( m_info, _( "You can't chop down that." ) );
        }
        return 0;
    }
    int moves = chop_moves( *p, *it );

    const std::vector<npc *> helpers = character_funcs::get_crafting_helpers( *p, 3 );
    for( const npc *np : helpers ) {
        add_msg( m_info, _( "%s helps with this task…" ), np->name );
    }
    moves = moves * ( 10 - helpers.size() ) / 10;

    p->assign_activity( ACT_CHOP_TREE, moves, -1, p->get_item_position( it ) );
    p->activity->tools.emplace_back( it );
    p->activity->placement = g->m.getabs( pnt );

    return it->type->charges_to_use();
}

int iuse::chop_logs( player *p, item *it, bool t, const tripoint & )
{
    if( !p || t ) {
        return 0;
    }
    if( p->is_mounted() ) {
        p->add_msg_if_player( m_info, _( "You cannot do that while mounted." ) );
        return 0;
    }

    const std::set<ter_id> allowed_ter_id {
        t_trunk,
        t_stump
    };
    const std::function<bool( const tripoint & )> f = [&allowed_ter_id]( const tripoint & pnt ) {
        const ter_id type = g->m.ter( pnt );
        const bool is_allowed_terrain = allowed_ter_id.contains( type );
        return is_allowed_terrain;
    };

    const std::optional<tripoint> pnt_ = choose_adjacent_highlight(
            _( "Chop which tree trunk?" ), _( "There is no tree trunk to chop nearby." ), f, false );
    if( !pnt_ ) {
        return 0;
    }
    const tripoint &pnt = *pnt_;
    if( !f( pnt ) ) {
        p->add_msg_if_player( m_info, _( "You can't chop that." ) );
        return 0;
    }

    int moves = chop_moves( *p, *it );

    const std::vector<npc *> helpers = character_funcs::get_crafting_helpers( *p, 3 );
    for( const npc *np : helpers ) {
        add_msg( m_info, _( "%s helps with this task…" ), np->name );
    }
    moves = moves * ( 10 - helpers.size() ) / 10;

    p->assign_activity( ACT_CHOP_LOGS, moves, -1, p->get_item_position( it ) );
    p->activity->placement = g->m.getabs( pnt );
    p->activity->tools.emplace_back( it );

    return it->type->charges_to_use();
}

int iuse::oxytorch( player *p, item *it, bool, const tripoint & )
{
    if( p->is_npc() ) {
        // Long action
        return 0;
    }
    if( p->is_mounted() ) {
        p->add_msg_if_player( m_info, _( "You cannot do that while mounted." ) );
        return 0;
    }
    static const quality_id GLARE( "GLARE" );
    if( !p->has_quality( GLARE, 2 ) ) {
        p->add_msg_if_player( m_info, _( "You need welding goggles to do that." ) );
        return 0;
    }

    map &here = get_map();
    const std::function<bool( const tripoint & )> f =
    [&here, p]( const tripoint & pnt ) {
        if( pnt == p->pos() ) {
            return false;
        } else if( here.has_furn( pnt ) ) {
            return here.furn( pnt )->oxytorch->valid();
        } else if( !here.ter( pnt )->is_null() ) {
            return here.ter( pnt )->oxytorch->valid();
        }
        return false;
    };

    const std::optional<tripoint> pnt_ = choose_adjacent_highlight(
            _( "Cut up metal where?" ), _( "There is no metal to cut up nearby." ), f, false );
    if( !pnt_ ) {
        return 0;
    }
    const tripoint &pnt = *pnt_;
    if( !f( pnt ) ) {
        if( pnt == p->pos() ) {
            p->add_msg_if_player( m_info, _( "Yuck.  Acetylene gas smells weird." ) );
        } else {
            p->add_msg_if_player( m_info, _( "You can't cut that." ) );
        }
        return 0;
    }
    const int fuel_requirement = it->ammo_required() * ( here.has_furn( pnt ) ? to_seconds<int>
                                 ( here.furn( pnt )->oxytorch->duration() ) : to_seconds<int>( here.ter(
                                         pnt )->oxytorch->duration() ) );

    if( fuel_requirement > it->ammo_remaining() ) {
        p->add_msg_if_player( m_bad,
                              _( "Your %1$s doesn't have enough charges to cut this, %2$s required." ), it->tname(),
                              fuel_requirement );
        return 0;
    }

    p->assign_activity( std::make_unique<player_activity>( std::make_unique<oxytorch_activity_actor>(
                            pnt, safe_reference<item>( *it )
                        ) ) );

    return 0;
}

int iuse::hacksaw( player *p, item *it, bool t, const tripoint & )
{
    if( !p || t ) {
        return 0;
    }
    if( p->is_mounted() ) {
        p->add_msg_if_player( m_info, _( "You cannot do that while mounted." ) );
        return 0;
    }

    map &here = get_map();
    const std::function<bool( const tripoint & )> f =
    [&here, p]( const tripoint & pnt ) {
        if( pnt == p->pos() ) {
            return false;
        } else if( here.has_furn( pnt ) ) {
            return here.furn( pnt )->hacksaw->valid();
        } else if( !here.ter( pnt )->is_null() ) {
            return here.ter( pnt )->hacksaw->valid();
        }
        return false;
    };

    const std::optional<tripoint> pnt_ = choose_adjacent_highlight(
            _( "Cut up metal where?" ), _( "There is no metal to cut up nearby." ), f, false );
    if( !pnt_ ) {
        return 0;
    }
    const tripoint &pnt = *pnt_;
    if( !f( pnt ) ) {
        if( pnt == p->pos() ) {
            p->add_msg_if_player( m_info, _( "Why would you do that?" ) );
            p->add_msg_if_player( m_info, _( "You're not even chained to a boiler." ) );
        } else {
            p->add_msg_if_player( m_info, _( "You can't cut that." ) );
        }
        return 0;
    }

    p->assign_activity( std::make_unique<player_activity>( std::make_unique<hacksaw_activity_actor>(
                            pnt, safe_reference<item>( *it )
                        ) ) );

    return 0;
}

int iuse::boltcutters( player *p, item *it, bool, const tripoint & )
{
    if( p->is_mounted() ) {
        p->add_msg_if_player( m_info, _( "You cannot do that while mounted." ) );
        return 0;
    }

    map &here = get_map();
    const std::function<bool( const tripoint & )> f =
    [&here, p]( const tripoint & pnt ) {
        if( pnt == p->pos() ) {
            return false;
        } else if( here.has_furn( pnt ) ) {
            return here.furn( pnt )->boltcut->valid();
        } else if( !here.ter( pnt )->is_null() ) {
            return here.ter( pnt )->boltcut->valid();
        }
        return false;
    };

    const std::optional<tripoint> pnt_ = choose_adjacent_highlight(
            _( "Cut up metal where?" ), _( "There is no metal to cut up nearby." ), f, false );
    if( !pnt_ ) {
        return 0;
    }
    const tripoint &pnt = *pnt_;
    if( !f( pnt ) ) {
        if( pnt == p->pos() ) {
            p->add_msg_if_player( m_info,
                                  _( "You neatly sever all of the veins and arteries in your body.  Oh wait, Never mind." ) );
        } else {
            p->add_msg_if_player( m_info, _( "You can't cut that." ) );
        }
        return 0;
    }

    p->assign_activity( std::make_unique<player_activity>( std::make_unique<boltcutting_activity_actor>(
                            pnt, safe_reference<item>( *it )
                        ) ) );

    return 0;
}

namespace
{

auto mop_normal( const tripoint &pos ) -> bool
{
    return get_map().mop_spills( pos );
}

auto mop_blindly( const tripoint &pos ) -> bool
{
    return one_in( 3 ) && get_map().mop_spills( pos );
}

} // namespace

auto iuse::mop( player *p, item *it, bool, const tripoint & ) -> int
{
    const auto mop = p->is_blind() ? mop_blindly : mop_normal;
    const auto xs = closest_points_first( p->pos(), 1 );

    const int mopped_tiles = std::count_if( xs.begin(), xs.end(), mop );

    if( p->is_blind() ) {
        p->add_msg_if_player( m_info, _( "You move the mop around, unsure whether it's doing any good." ) );
    } else if( mopped_tiles == 0 ) {
        p->add_msg_if_player( m_bad, _( "There's nothing to mop there." ) );
    } else {
        p->add_msg_if_player( m_info, _( "You mop up the spill." ) );
    }

    p->moves -= 150 * mopped_tiles;
    return it->type->charges_to_use();
}

int iuse::artifact( player *p, item *it, bool, const tripoint & )
{
    if( p->is_npc() ) {
        // TODO: Allow this for trusting NPCs
        return 0;
    }

    if( !it->is_artifact() ) {
        debugmsg( "iuse::artifact called on a non-artifact item!  %s",
                  it->tname() );
        return 0;
    } else if( !it->is_tool() ) {
        debugmsg( "iuse::artifact called on a non-tool artifact!  %s",
                  it->tname() );
        return 0;
    }
    g->events().send<event_type::activates_artifact>( p->getID(), it->tname( 1, false ) );

    const auto &art = it->type->artifact;
    size_t num_used = rng( 1, art->effects_activated.size() );
    if( num_used < art->effects_activated.size() ) {
        num_used += rng( 1, art->effects_activated.size() - num_used );
    }

    std::vector<art_effect_active> effects = art->effects_activated;
    for( size_t i = 0; i < num_used && !effects.empty(); i++ ) {
        const art_effect_active used = random_entry_removed( effects );

        switch( used ) {
            case AEA_STORM: {
                sounds::sound( p->pos(), 10, sounds::sound_t::combat, _( "Ka-BOOM!" ), true, "environment",
                               "thunder_near" );
                int num_bolts = rng( 2, 4 );
                for( int j = 0; j < num_bolts; j++ ) {
                    point dir;
                    while( dir.x == 0 && dir.y == 0 ) {
                        dir.x = rng( -1, 1 );
                        dir.y = rng( -1, 1 );
                    }
                    int dist = rng( 4, 12 );
                    point bolt( p->posx(), p->posy() );
                    for( int n = 0; n < dist; n++ ) {
                        bolt.x += dir.x;
                        bolt.y += dir.y;
                        g->m.add_field( {bolt, p->posz()}, fd_electricity, rng( 2, 3 ) );
                        if( one_in( 4 ) ) {
                            if( dir.x == 0 ) {
                                dir.x = rng( 0, 1 ) * 2 - 1;
                            } else {
                                dir.x = 0;
                            }
                        }
                        if( one_in( 4 ) ) {
                            if( dir.y == 0 ) {
                                dir.y = rng( 0, 1 ) * 2 - 1;
                            } else {
                                dir.y = 0;
                            }
                        }
                    }
                }
            }
            break;

            case AEA_FIREBALL: {
                if( const std::optional<tripoint> fireball = g->look_around() ) {
                    // only the player can trigger artifact
                    explosion_handler::explosion( *fireball, p, 180, 0.5, true );
                }
            }
            break;

            case AEA_ADRENALINE:
                p->add_msg_if_player( m_good, _( "You're filled with a roaring energy!" ) );
                p->add_effect( effect_adrenaline, rng( 2_minutes, 3_minutes ) );
                break;

            case AEA_MAP: {
                const tripoint_abs_omt center = p->global_omt_location();
                const bool new_map = overmap_buffer.reveal( center.xy(), 20, center.z() );
                if( new_map ) {
                    p->add_msg_if_player( m_warning, _( "You have a vision of the surrounding area…" ) );
                    p->moves -= to_moves<int>( 1_seconds );
                }
            }
            break;

            case AEA_BLOOD: {
                bool blood = false;
                for( const tripoint &tmp : g->m.points_in_radius( p->pos(), 4 ) ) {
                    if( !one_in( 4 ) && g->m.add_field( tmp, fd_blood, 3 ) &&
                        ( blood || g->u.sees( tmp ) ) ) {
                        blood = true;
                    }
                }
                if( blood ) {
                    p->add_msg_if_player( m_warning, _( "Blood soaks out of the ground and walls." ) );
                }
            }
            break;

            case AEA_FATIGUE: {
                p->add_msg_if_player( m_warning, _( "The fabric of space seems to decay." ) );
                point p2{ rng( p->posx() - 3, p->posx() + 3 ), rng( p->posy() - 3, p->posy() + 3 ) };
                g->m.add_field( {p2, p->posz()}, fd_fatigue, rng( 1, 2 ) );
            }
            break;

            case AEA_ACIDBALL: {
                if( const std::optional<tripoint> acidball = g->look_around() ) {
                    for( const tripoint &tmp : g->m.points_in_radius( *acidball, 1 ) ) {
                        g->m.add_field( tmp, fd_acid, rng( 2, 3 ) );
                    }
                }
            }
            break;

            case AEA_PULSE:
                sounds::sound( p->pos(), 30, sounds::sound_t::combat, _( "The earth shakes!" ), true, "misc",
                               "earthquake" );
                for( const tripoint &pt : g->m.points_in_radius( p->pos(), 2 ) ) {
                    g->m.bash( pt, 40 );
                    g->m.bash( pt, 40 );  // Multibash effect, so that doors &c will fall
                    g->m.bash( pt, 40 );
                    if( g->m.is_bashable( pt ) && rng( 1, 10 ) >= 3 ) {
                        g->m.bash( pt, 999, false, true );
                    }
                }
                break;

            case AEA_HEAL:
                p->add_msg_if_player( m_good, _( "You feel healed." ) );
                p->healall( 2 );
                break;

            case AEA_CONFUSED:
                for( const tripoint &dest : g->m.points_in_radius( p->pos(), 8 ) ) {
                    if( monster *const mon = g->critter_at<monster>( dest, true ) ) {
                        mon->add_effect( effect_stunned, rng( 5_turns, 15_turns ) );
                    }
                }
                break;

            case AEA_ENTRANCE:
                for( const tripoint &dest : g->m.points_in_radius( p->pos(), 8 ) ) {
                    monster *const mon = g->critter_at<monster>( dest, true );
                    if( mon && mon->friendly == 0 && rng( 0, 600 ) > mon->get_hp() ) {
                        mon->make_friendly();
                    }
                }
                break;

            case AEA_BUGS: {
                int roll = rng( 1, 10 );
                mtype_id bug = mtype_id::NULL_ID();
                int num = 0;
                if( roll <= 4 ) {
                    p->add_msg_if_player( m_warning, _( "Flies buzz around you." ) );
                } else if( roll <= 7 ) {
                    p->add_msg_if_player( m_warning, _( "Giant flies appear!" ) );
                    bug = mon_fly;
                    num = rng( 2, 4 );
                } else if( roll <= 9 ) {
                    p->add_msg_if_player( m_warning, _( "Giant bees appear!" ) );
                    bug = mon_bee;
                    num = rng( 1, 3 );
                } else {
                    p->add_msg_if_player( m_warning, _( "Giant wasps appear!" ) );
                    bug = mon_wasp;
                    num = rng( 1, 2 );
                }
                if( bug ) {
                    for( int j = 0; j < num; j++ ) {
                        if( monster *const b = g->place_critter_around( bug, p->pos(), 1 ) ) {
                            b->friendly = -1;
                            b->add_effect( effect_pet, 1_turns );
                        }
                    }
                }
            }
            break;

            case AEA_TELEPORT:
                teleport::teleport( *p );
                break;

            case AEA_LIGHT:
                p->add_msg_if_player( _( "The %s glows brightly!" ), it->tname() );
                g->timed_events.add( TIMED_EVENT_ARTIFACT_LIGHT, calendar::turn + 3_minutes );
                break;

            case AEA_GROWTH: {
                monster tmptriffid( mtype_id::NULL_ID(), p->pos() );
                mattack::growplants( &tmptriffid );
            }
            break;

            case AEA_HURTALL:
                for( monster &critter : g->all_monsters() ) {
                    critter.apply_damage( nullptr, bodypart_id( "torso" ), rng( 0, 5 ) );
                }
                break;

            case AEA_RADIATION:
                add_msg( m_warning, _( "Horrible gases are emitted!" ) );
                for( const tripoint &dest : g->m.points_in_radius( p->pos(), 1 ) ) {
                    g->m.add_field( dest, fd_nuke_gas, rng( 2, 3 ) );
                }
                break;

            case AEA_PAIN:
                p->add_msg_if_player( m_bad, _( "You're wracked with pain!" ) );
                // OK, the Lovecraftian thingamajig can bring Deadened
                // masochists & Cenobites the stimulation they've been
                // craving ;)
                p->mod_pain_noresist( rng( 5, 15 ) );
                break;

            case AEA_MUTATE:
                if( !one_in( 3 ) ) {
                    p->mutate();
                }
                break;

            case AEA_PARALYZE:
                p->add_msg_if_player( m_bad, _( "You're paralyzed!" ) );
                p->moves -= rng( 50, 200 );
                break;

            case AEA_FIRESTORM: {
                p->add_msg_if_player( m_bad, _( "Fire rains down around you!" ) );
                std::vector<tripoint> ps = closest_points_first( p->pos(), 3 );
                for( auto p_it : ps ) {
                    if( !one_in( 3 ) ) {
                        g->m.add_field( p_it, fd_fire, 1 + rng( 0, 1 ) * rng( 0, 1 ), 3_minutes );
                    }
                }
                break;
            }

            case AEA_ATTENTION:
                p->add_msg_if_player( m_warning, _( "You feel like your action has attracted attention." ) );
                p->add_effect( effect_attention, rng( 1_hours, 3_hours ) );
                break;

            case AEA_TELEGLOW:
                p->add_msg_if_player( m_warning, _( "You feel unhinged." ) );
                p->add_effect( effect_teleglow, rng( 30_minutes, 120_minutes ) );
                break;

            case AEA_NOISE:
                sounds::sound( p->pos(), 100, sounds::sound_t::combat,
                               string_format( _( "a deafening boom from %s %s" ),
                                              p->disp_name( true ), it->tname() ), true, "misc", "shockwave" );
                break;

            case AEA_SCREAM:
                sounds::sound( p->pos(), 40, sounds::sound_t::alert,
                               string_format( _( "a disturbing scream from %s %s" ),
                                              p->disp_name( true ), it->tname() ), true, "shout", "scream" );
                if( !p->is_deaf() ) {
                    p->add_morale( MORALE_SCREAM, -10, 0, 30_minutes, 1_minutes );
                }
                break;

            case AEA_DIM:
                p->add_msg_if_player( _( "The sky starts to dim." ) );
                g->timed_events.add( TIMED_EVENT_DIM, calendar::turn + 5_minutes );
                break;

            case AEA_FLASH:
                p->add_msg_if_player( _( "The %s flashes brightly!" ), it->tname() );
                explosion_handler::flashbang( p->pos(), false, "explosion" );
                break;

            case AEA_VOMIT:
                p->add_msg_if_player( m_bad, _( "A wave of nausea passes through you!" ) );
                p->vomit();
                break;

            case AEA_SHADOWS: {
                int num_shadows = rng( 4, 8 );
                int num_spawned = 0;
                for( int j = 0; j < num_shadows; j++ ) {
                    for( int tries = 0; tries < 10; ++tries ) {
                        tripoint monp = p->pos();
                        if( one_in( 2 ) ) {
                            monp.x = rng( p->posx() - 5, p->posx() + 5 );
                            monp.y = ( one_in( 2 ) ? p->posy() - 5 : p->posy() + 5 );
                        } else {
                            monp.x = ( one_in( 2 ) ? p->posx() - 5 : p->posx() + 5 );
                            monp.y = rng( p->posy() - 5, p->posy() + 5 );
                        }
                        if( !g->m.sees( monp, p->pos(), 10 ) ) {
                            continue;
                        }
                        if( monster *const  spawned = g->place_critter_at( mon_shadow, monp ) ) {
                            num_spawned++;
                            spawned->reset_special_rng( "DISAPPEAR" );
                            break;
                        }
                    }
                }
                if( num_spawned > 1 ) {
                    p->add_msg_if_player( m_warning, _( "Shadows form around you." ) );
                } else if( num_spawned == 1 ) {
                    p->add_msg_if_player( m_warning, _( "A shadow forms nearby." ) );
                }
            }
            break;

            case AEA_STAMINA_EMPTY:
                p->add_msg_if_player( m_bad, _( "Your body feels like jelly." ) );
                p->set_stamina( p->get_stamina() * 1 / ( rng( 3, 8 ) ) );
                break;

            case AEA_FUN:
                p->add_msg_if_player( m_good, _( "You're filled with euphoria!" ) );
                p->add_morale( MORALE_FEELING_GOOD, rng( 20, 50 ), 0, 5_minutes, 5_turns, false );
                break;

            case AEA_SPLIT:
                // TODO: Add something
                break;

            case AEA_NULL:
            // BUG
            case NUM_AEAS:
            default:
                debugmsg( "iuse::artifact(): wrong artifact type (%d)", used );
                break;
        }
    }
    return it->type->charges_to_use();
}

int iuse::spray_can( player *p, item *it, bool, const tripoint & )
{
    const std::optional<tripoint> dest_ = choose_adjacent( _( "Spray where?" ) );
    if( !dest_ ) {
        return 0;
    }
    return handle_ground_graffiti( *p, it, _( "Spray what?" ), dest_.value() );
}

int iuse::handle_ground_graffiti( player &p, item *it, const std::string &prefix,
                                  const tripoint &where )
{
    string_input_popup popup;
    std::string message = popup
                          .description( prefix + " " + _( "(To delete, clear the text and confirm)" ) )
                          .text( g->m.has_graffiti_at( where ) ? g->m.graffiti_at( where ) : std::string() )
                          .identifier( "graffiti" )
                          .query_string();
    if( popup.canceled() ) {
        return 0;
    }

    bool grave = g->m.ter( where ) == t_grave_new;
    int move_cost;
    if( message.empty() ) {
        if( g->m.has_graffiti_at( where ) ) {
            move_cost = 3 * g->m.graffiti_at( where ).length();
            g->m.delete_graffiti( where );
            if( grave ) {
                p.add_msg_if_player( m_info, _( "You blur the inscription on the grave." ) );
            } else {
                p.add_msg_if_player( m_info, _( "You manage to get rid of the message on the surface." ) );
            }
        } else {
            return 0;
        }
    } else {
        g->m.set_graffiti( where, message );
        if( grave ) {
            p.add_msg_if_player( m_info, _( "You carve an inscription on the grave." ) );
        } else {
            p.add_msg_if_player( m_info, _( "You write a message on the surface." ) );
        }
        move_cost = 2 * message.length();
    }
    p.moves -= move_cost;
    if( it != nullptr ) {
        return it->type->charges_to_use();
    } else {
        return 0;
    }
}

int iuse::towel( player *p, item *it, bool t, const tripoint & )
{
    return towel_common( p, it, t );
}

int iuse::towel_common( player *p, item *it, bool t )
{
    if( t ) {
        // Continuous usage, do nothing as not initiated by the player, this is for
        // wet towels only as they are active items.
        return 0;
    }
    bool slime = p->has_effect( effect_slimed );
    bool boom = p->has_effect( effect_boomered );
    bool glow = p->has_effect( effect_glowing );
    int mult = slime + boom + glow; // cleaning off more than one at once makes it take longer
    bool towelUsed = false;
    const std::string name = it ? it->tname() : _( "towel" );

    // can't use an already wet towel!
    if( it && it->has_flag( flag_WET ) ) {
        p->add_msg_if_player( m_info, _( "That %s is too wet to soak up any more liquid!" ),
                              it->tname() );
        // clean off the messes first, more important
    } else if( slime || boom || glow ) {
        p->remove_effect( effect_slimed ); // able to clean off all at once
        p->remove_effect( effect_boomered );
        p->remove_effect( effect_glowing );
        p->add_msg_if_player( _( "You use the %s to clean yourself off, saturating it with slime!" ),
                              name );

        towelUsed = true;
        if( it && it->typeId() == itype_towel ) {
            it->convert( itype_towel_soiled );
        }

        // dry off from being wet
    } else if( p->has_morale( MORALE_WET ) ) {
        p->rem_morale( MORALE_WET );
        for( auto &pr : p->get_body() ) {
            pr.second.set_wetness( 0 );
        }
        p->add_msg_if_player( _( "You use the %s to dry off, saturating it with water!" ),
                              name );

        towelUsed = true;
        if( it ) {
            it->set_counter( to_turns<int>( 30_minutes ) );
        }

        // default message
    } else {
        p->add_msg_if_player( _( "You are already dry, the %s does nothing." ), name );
    }

    // towel was used
    if( towelUsed ) {
        if( mult == 0 ) {
            mult = 1;
        }
        p->moves -= 50 * mult;
        if( it ) {
            // change "towel" to a "towel_wet" (different flavor text/color)
            if( it->typeId() == itype_towel ) {
                it->convert( itype_towel_wet );
            }

            // WET, active items have their timer decremented every turn
            it->set_flag( flag_WET );
            it->activate();
        }
    }
    return it ? it->type->charges_to_use() : 0;
}

int iuse::unfold_generic( player *p, item *it, bool, const tripoint & )
{
    if( p->is_underwater() ) {
        p->add_msg_if_player( m_info, _( "You can't do that while underwater." ) );
        return 0;
    }
    if( p->is_mounted() ) {
        p->add_msg_if_player( m_info, _( "You cannot do that while mounted." ) );
        return 0;
    }
    map &here = get_map();
    vehicle *veh = here.add_vehicle( vproto_id( "none" ), p->pos(), 0_degrees, 0, 0, false );
    if( veh == nullptr ) {
        p->add_msg_if_player( m_info, _( "There's no room to unfold the %s." ), it->tname() );
        return 0;
    }
    veh->name = it->get_var( "vehicle_name" );
    if( !veh->restore( it->get_var( "folding_bicycle_parts" ) ) ) {
        g->m.destroy_vehicle( veh );
        return 0;
    }
    const bool can_float = veh->can_float();

    const auto invalid_pos = []( const tripoint & pp, bool can_float ) {
        return ( g->m.has_flag_ter( TFLAG_DEEP_WATER, pp ) && !can_float ) ||
               g->m.veh_at( pp ) || g->m.impassable( pp );
    };
    for( const vpart_reference &vp : veh->get_all_parts() ) {
        if( vp.info().location != "structure" && !vp.info().has_flag( VPFLAG_EXTENDABLE ) ) {
            continue;
        }
        const tripoint pp = vp.pos();
        if( invalid_pos( pp, can_float ) ) {
            p->add_msg_if_player( m_info, _( "There's no room to unfold the %s." ), it->tname() );
            g->m.destroy_vehicle( veh );
            return 0;
        }
    }

    g->m.add_vehicle_to_cache( veh );

    std::string unfold_msg = it->get_var( "unfold_msg" );
    if( unfold_msg.empty() ) {
        unfold_msg = _( "You painstakingly unfold the %s and make it ready to ride." );
    } else {
        unfold_msg = _( unfold_msg );
    }
    veh->set_owner( *p );
    g->m.board_vehicle( p->pos(), p );
    p->add_msg_if_player( m_neutral, unfold_msg, veh->name );

    p->moves -= it->get_var( "moves", to_turns<int>( 5_seconds ) );
    return 1;
}

int iuse::adrenaline_injector( player *p, item *it, bool, const tripoint & )
{
    if( p->is_npc() && p->get_effect_dur( effect_adrenaline ) >= 3_minutes ) {
        return 0;
    }

    p->moves -= to_moves<int>( 1_seconds );
    p->add_msg_player_or_npc( _( "You inject yourself with adrenaline." ),
                              _( "<npcname> injects themselves with adrenaline." ) );

    p->i_add( item::spawn( "syringe", it->birthday() ) );
    if( p->has_effect( effect_adrenaline ) ) {
        p->add_msg_if_player( m_bad, _( "Your heart spasms!" ) );
        // Note: not the mod, the health
        p->mod_healthy( -20 );
    }

    p->add_effect( effect_adrenaline, 2_minutes );

    return it->type->charges_to_use();
}

int iuse::jet_injector( player *p, item *it, bool, const tripoint & )
{
    if( !it->ammo_sufficient() ) {
        p->add_msg_if_player( m_info, _( "The jet injector is empty." ) );
        return 0;
    } else {
        p->add_msg_if_player( _( "You inject yourself with the jet injector." ) );
        // Intensity is 2 here because intensity = 1 is the comedown
        p->add_effect( effect_jetinjector, 20_minutes, bodypart_str_id::NULL_ID(), 2 );
        p->mod_painkiller( 20 );
        p->mod_stim( 10 );
        p->healall( 20 );
    }

    if( p->has_effect( effect_jetinjector ) ) {
        if( p->get_effect_dur( effect_jetinjector ) > 20_minutes ) {
            p->add_msg_if_player( m_warning, _( "Your heart is beating alarmingly fast!" ) );
        }
    }
    return it->type->charges_to_use();
}

int iuse::stimpack( player *p, item *it, bool, const tripoint & )
{
    if( p->get_item_position( it ) >= -1 ) {
        p->add_msg_if_player( m_info,
                              _( "You must wear the stimulant delivery system before you can activate it." ) );
        return 0;
    }

    if( !it->ammo_sufficient() ) {
        p->add_msg_if_player( m_info, _( "The stimulant delivery system is empty." ) );
        return 0;
    } else {
        p->add_msg_if_player( _( "You inject yourself with the stimulants." ) );
        // Intensity is 2 here because intensity = 1 is the comedown
        p->add_effect( effect_stimpack, 25_minutes, bodypart_str_id::NULL_ID(), 2 );
        p->mod_painkiller( 2 );
        p->mod_stim( 20 );
        p->mod_fatigue( -100 );
        p->set_stamina( p->get_stamina_max() );
    }
    return it->type->charges_to_use();
}

int iuse::radglove( player *p, item *it, bool, const tripoint & )
{
    if( p->get_item_position( it ) >= -1 ) {
        p->add_msg_if_player( m_info,
                              _( "You must wear the radiation biomonitor before you can activate it." ) );
        return 0;
    } else if( !it->units_sufficient( *p ) ) {
        p->add_msg_if_player( m_info, _( "The radiation biomonitor needs batteries to function." ) );
        return 0;
    } else {
        p->add_msg_if_player( _( "You activate your radiation biomonitor." ) );
        if( p->get_rad() >= 1 ) {
            p->add_msg_if_player( m_warning, _( "You are currently irradiated." ) );
            p->add_msg_player_or_say( m_info,
                                      _( "Your radiation level: %d mSv." ),
                                      _( "It says here that my radiation level is %d mSv." ),
                                      p->get_rad() );
        } else {
            p->add_msg_player_or_say( m_info,
                                      _( "You are not currently irradiated." ),
                                      _( "It says I'm not irradiated" ) );
        }
        p->add_msg_if_player( _( "Have a nice day!" ) );
    }

    return it->type->charges_to_use();
}

int iuse::contacts( player *p, item *it, bool, const tripoint & )
{
    if( p->is_underwater() ) {
        p->add_msg_if_player( m_info, _( "You can't do that while underwater." ) );
        return 0;
    }
    const time_duration duration = rng( 6_days, 8_days );
    if( p->has_effect( effect_contacts ) ) {
        if( query_yn( _( "Replace your current lenses?" ) ) ) {
            p->moves -= to_moves<int>( 20_seconds );
            p->add_msg_if_player( _( "You replace your current %s." ), it->tname() );
            p->remove_effect( effect_contacts );
            p->add_effect( effect_contacts, duration );
            return it->type->charges_to_use();
        } else {
            p->add_msg_if_player( _( "You don't do anything with your %s." ), it->tname() );
            return 0;
        }
    } else if( p->has_trait( trait_HYPEROPIC ) || p->has_trait( trait_MYOPIC ) ||
               p->has_trait( trait_URSINE_EYE ) ) {
        p->moves -= to_moves<int>( 20_seconds );
        p->add_msg_if_player( _( "You put the %s in your eyes." ), it->tname() );
        p->add_effect( effect_contacts, duration );
        return it->type->charges_to_use();
    } else {
        p->add_msg_if_player( m_info, _( "Your vision is fine already." ) );
        return 0;
    }
}

int iuse::talking_doll( player *p, item *it, bool, const tripoint & )
{
    if( !it->units_sufficient( *p ) ) {
        p->add_msg_if_player( m_info, _( "The %s's batteries are dead." ), it->tname() );
        return 0;
    }

    const SpeechBubble &speech = get_speech( it->typeId().str() );

    sounds::sound( p->pos(), speech.volume, sounds::sound_t::electronic_speech,
                   speech.text.translated(), true, "speech", it->typeId().str() );

    // Sound code doesn't describe noises at the player position
    if( p->can_hear( p->pos(), speech.volume ) ) {
        p->add_msg_if_player( _( "You hear \"%s\"" ), speech.text );
    }

    return it->type->charges_to_use();
}

int iuse::gun_clean( player *p, item *, bool, const tripoint & )
{
    item *loc = game_menus::inv::titled_menu( g->u, ( "Select the firearm to clean or mend" ) );
    if( !loc ) {
        p->add_msg_if_player( m_info, _( "You do not have that item!" ) );
        return 0;
    }
    item &fix = *loc;
    if( !fix.is_firearm() ) {
        p->add_msg_if_player( m_info, _( "That isn't a firearm!" ) );
        return 0;
    }

    const auto is_gunmods_not_faulty = []( const auto & xs ) -> bool {
        return std::all_of( xs.begin(), xs.end(), []( const item * mod ) -> bool { return mod->faults.empty(); } );
    };

    if( fix.faults.empty() && is_gunmods_not_faulty( fix.gunmods() ) ) {
        p->add_msg_if_player( m_info, _( "There's nothing you can clean or mend with this." ) );
        return 0;
    }
    avatar_funcs::mend_item( *p->as_avatar(),  *loc );
    return 0;
}

int iuse::gun_repair( player *p, item *it, bool, const tripoint & )
{
    if( !it->units_sufficient( *p ) ) {
        return 0;
    }
    if( p->is_underwater() ) {
        p->add_msg_if_player( m_info, _( "You can't do that while underwater." ) );
        return 0;
    }
    if( p->is_mounted() ) {
        p->add_msg_if_player( m_info, _( "You cannot do that while mounted." ) );
        return 0;
    }
    /** @EFFECT_MECHANICS >1 allows gun repair */
    if( p->get_skill_level( skill_mechanics ) < 2 ) {
        p->add_msg_if_player( m_info, _( "You need a mechanics skill of 2 to use this repair kit." ) );
        return 0;
    }
    item *loc = game_menus::inv::titled_menu( g->u, ( "Select the firearm to repair" ) );
    if( !loc ) {
        p->add_msg_if_player( m_info, _( "You do not have that item!" ) );
        return 0;
    }
    item &fix = *loc;
    if( !fix.is_firearm() ) {
        p->add_msg_if_player( m_info, _( "That isn't a firearm!" ) );
        return 0;
    }
    if( fix.has_flag( flag_NO_REPAIR ) ) {
        p->add_msg_if_player( m_info, _( "You cannot repair your %s." ), fix.tname() );
        return 0;
    }
    if( fix.damage() <= fix.min_damage() ) {
        p->add_msg_if_player( m_info, _( "You cannot improve your %s any more this way." ),
                              fix.tname() );
        return 0;
    }
    if( fix.damage() <= 0 && p->get_skill_level( skill_mechanics ) < 8 ) {
        p->add_msg_if_player( m_info, _( "Your %s is already in peak condition." ), fix.tname() );
        p->add_msg_if_player( m_info,
                              _( "With a higher mechanics skill, you might be able to improve it." ) );
        return 0;
    }
    /** @EFFECT_MECHANICS >=8 allows accurizing ranged weapons */
    const std::string startdurability = fix.durability_indicator( true );
    std::string resultdurability;
    const float vision_mod = character_funcs::fine_detail_vision_mod( *p );
    // TODO: this may render player unable to move for minutes, and so should start an activity instead
    if( fix.damage() <= 0 ) {
        sounds::sound( p->pos(), 6, sounds::sound_t::activity, "crunch", true, "tool", "repair_kit" );
        p->moves -= to_moves<int>( 20_seconds * vision_mod );
        p->practice( skill_mechanics, 10 );
        fix.mod_damage( -itype::damage_scale );
        p->add_msg_if_player( m_good, _( "You accurize your %s." ), fix.tname( 1, false ) );

    } else if( fix.damage() > itype::damage_scale ) {
        sounds::sound( p->pos(), 8, sounds::sound_t::activity, "crunch", true, "tool", "repair_kit" );
        p->moves -= to_moves<int>( 10_seconds * vision_mod );
        p->practice( skill_mechanics, 10 );
        fix.mod_damage( -itype::damage_scale );
        resultdurability = fix.durability_indicator( true );
        p->add_msg_if_player( m_good, _( "You repair your %s!  ( %s-> %s)" ), fix.tname( 1, false ),
                              startdurability, resultdurability );

    } else {
        sounds::sound( p->pos(), 8, sounds::sound_t::activity, "crunch", true, "tool", "repair_kit" );
        p->moves -= to_moves<int>( 5_seconds * vision_mod );
        p->practice( skill_mechanics, 10 );
        fix.set_damage( 0 );
        resultdurability = fix.durability_indicator( true );
        p->add_msg_if_player( m_good, _( "You repair your %s completely!  ( %s-> %s)" ),
                              fix.tname( 1, false ), startdurability, resultdurability );
    }
    return it->type->charges_to_use();
}

int iuse::gunmod_attach( player *p, item *it, bool, const tripoint & )
{
    if( !it || !it->is_gunmod() ) {
        debugmsg( "tried to attach non-gunmod" );
        return 0;
    }

    if( !p ) {
        return 0;
    }

    auto loc = game_menus::inv::gun_to_modify( *p, *it );

    if( !loc ) {
        add_msg( m_info, _( "Never mind." ) );
        return 0;
    }

    avatar_funcs::gunmod_add( *p->as_avatar(), *loc, *it );

    return 0;
}

int iuse::toolmod_attach( player *p, item *it, bool, const tripoint & )
{
    if( !it || !it->is_toolmod() ) {
        debugmsg( "tried to attach non-toolmod" );
        return 0;
    }

    if( !p ) {
        return 0;
    }

    auto filter = [&it]( const item & e ) {
        // don't allow ups battery mods on a UPS or UPS-powered tools
        if( it->has_flag( flag_USE_UPS ) &&
            ( e.has_flag( flag_IS_UPS ) || e.has_flag( flag_USE_UPS ) ) ) {
            return false;
        }

        // can only attach to unmodified tools that use compatible ammo
        return e.is_tool() && e.toolmods().empty() && !e.magazine_current() &&
               std::any_of( it->type->mod->acceptable_ammo.begin(),
        it->type->mod->acceptable_ammo.end(), [&]( const ammotype & at ) {
            return e.ammo_types( false ).count( at );
        } );
    };

    auto loc = g->inv_map_splice( filter, _( "Select tool to modify" ), 1,
                                  _( "You don't have compatible tools." ) );

    if( !loc ) {
        add_msg( m_info, _( "Never mind." ) );
        return 0;
    }

    if( loc->ammo_remaining() ) {
        if( !avatar_funcs::unload_item( *p->as_avatar(), *loc ) ) {
            p->add_msg_if_player( m_info, _( "You cancel unloading the tool." ) );
            return 0;
        }
    }

    avatar_funcs::toolmod_add( *p->as_avatar(), *loc, *it );
    return 0;
}

int iuse::bell( player *p, item *it, bool, const tripoint & )
{
    if( it->typeId() == itype_cow_bell ) {
        sounds::sound( p->pos(), 12, sounds::sound_t::music, _( "Clank!  Clank!" ), true, "misc",
                       "cow_bell" );
        if( !p->is_deaf() ) {
            auto cattle_level =
                p->mutation_category_level.find( mutation_category_id( "CATTLE" ) );
            const int cow_factor = 1 + ( cattle_level == p->mutation_category_level.end() ?
                                         0 :
                                         ( cattle_level->second ) / 8
                                       );
            if( x_in_y( cow_factor, 1 + cow_factor ) ) {
                p->add_morale( MORALE_MUSIC, 1, 15 * ( cow_factor > 10 ? 10 : cow_factor ) );
            }
        }
    } else {
        sounds::sound( p->pos(), 4, sounds::sound_t::music, _( "Ring!  Ring!" ), true, "misc", "bell" );
    }
    return it->type->charges_to_use();
}

int iuse::seed( player *p, item *it, bool, const tripoint & )
{
    if( p->is_npc() ||
        query_yn( _( "Sure you want to eat the %s?  You could plant it in a mound of dirt." ),
                  colorize( it->tname(), it->color_in_inventory() ) ) ) {
        return it->type->charges_to_use(); //This eats the seed object.
    }
    return 0;
}

namespace
{
auto is_hackable_robot( const monster &mon ) -> bool
{
    // HackPRO targets electronic robots regardless of species naming.
    return mon.has_flag( MF_ELECTRONIC );
}

auto get_hackable_friendly_monsters( game &game_ref ) -> std::vector<shared_ptr_fast<monster>>
{
    auto monsters = game_ref.all_monsters();
    auto &items = monsters.items;
    auto results = std::vector<shared_ptr_fast<monster>> {};
    std::ranges::for_each( items, [&]( const auto & weak_monster ) {
        auto current = weak_monster.lock();
        if( !current || current->is_dead() ) {
            return;
        }
        if( current->friendly == 0 || !is_hackable_robot( *current ) ) {
            return;
        }
        results.push_back( std::move( current ) );
    } );
    return results;
}
} // namespace

bool iuse::robotcontrol_can_target( player *p, const monster &m )
{
    return !m.is_dead()
           && is_hackable_robot( m )
           && m.friendly == 0
           && rl_dist( p->pos(), m.pos() ) <= 10;
}

int iuse::robotcontrol( player *p, item *it, bool, const tripoint & )
{
    if( !it->units_sufficient( *p ) ) {
        p->add_msg_if_player( _( "The %s's batteries are dead." ), it->tname() );
        return 0;

    }
    if( p->has_trait( trait_ILLITERATE ) ) {
        p->add_msg_if_player( _( "You cannot read a computer screen." ) );
        return 0;
    }

    if( p->has_trait( trait_HYPEROPIC ) && !p->worn_with_flag( flag_FIX_FARSIGHT ) &&
        !p->has_effect( effect_contacts ) && !p->has_bionic( bio_eye_optic ) ) {
        p->add_msg_if_player( m_info,
                              _( "You'll need to put on reading glasses before you can see the screen." ) );
        return 0;
    }

    int choice = uilist( _( "Welcome to hackPRO!:" ), {
        _( "Prepare IFF protocol override" ),
        _( "Set friendly robots to passive mode" ),
        _( "Set friendly robots to combat mode" )
    } );
    switch( choice ) {
        case 0: { // attempt to make a robot friendly
            uilist pick_robot;
            pick_robot.text = _( "Choose an endpoint to hack." );
            // Build a list of all unfriendly robots in range.
            // TODO: change into vector<Creature*>
            std::vector< shared_ptr_fast< monster> > mons;
            std::vector< tripoint > locations;
            int entry_num = 0;
            for( const monster &candidate : g->all_monsters() ) {
                if( robotcontrol_can_target( p, candidate ) ) {
                    mons.push_back( g->shared_from( candidate ) );
                    pick_robot.addentry( entry_num++, true, MENU_AUTOASSIGN, candidate.name() );
                    tripoint seen_loc;
                    // Show locations of seen robots, center on player if robot is not seen
                    if( p->sees( candidate ) ) {
                        seen_loc = candidate.pos();
                    } else {
                        seen_loc = p->pos();
                    }
                    locations.push_back( seen_loc );
                }
            }
            if( mons.empty() ) {
                p->add_msg_if_player( m_info, _( "No enemy robots in range." ) );
                return it->type->charges_to_use();
            }
            pointmenu_cb callback( locations );
            pick_robot.callback = &callback;
            pick_robot.query();
            if( pick_robot.ret < 0 || static_cast<size_t>( pick_robot.ret ) >= mons.size() ) {
                p->add_msg_if_player( m_info, _( "Never mind" ) );
                return it->type->charges_to_use();
            }
            const size_t mondex = pick_robot.ret;
            shared_ptr_fast< monster > z = mons[mondex];
            p->add_msg_if_player( _( "You start reprogramming the %s into an ally." ), z->name() );

            /** @EFFECT_INT speeds up hacking preperation */
            /** @EFFECT_COMPUTER speeds up hacking preperation */
            int move_cost = std::max( 100, 1000 - p->int_cur * 10 - p->get_skill_level( skill_computer ) * 10 );
            std::unique_ptr<player_activity> act = std::make_unique<player_activity>( ACT_ROBOT_CONTROL,
                                                   move_cost );
            act->monsters.emplace_back( z );

            p->assign_activity( std::move( act ) );

            return it->type->charges_to_use();
        }
        case 1: { //make all friendly robots stop their purposeless extermination of (un)life.
            p->moves -= to_moves<int>( 1_seconds );
            auto hackables = get_hackable_friendly_monsters( *g );
            const auto f = hackables.empty() ? 0 : 1;
            std::ranges::for_each( hackables, [&]( const shared_ptr_fast<monster> &critter ) {
                p->add_msg_if_player( _( "A following %s goes into passive mode." ),
                                      critter->name() );
                critter->add_effect( effect_docile, 1_turns );
            } );
            if( f == 0 ) {
                p->add_msg_if_player( _( "You are not commanding any robots." ) );
                return 0;
            }
            return it->type->charges_to_use();
        }
        case 2: { //make all friendly robots terminate (un)life with extreme prejudice
            p->moves -= to_moves<int>( 1_seconds );
            auto hackables = get_hackable_friendly_monsters( *g );
            const auto f = hackables.empty() ? 0 : 1;
            std::ranges::for_each( hackables, [&]( const shared_ptr_fast<monster> &critter ) {
                p->add_msg_if_player( _( "A following %s goes into combat mode." ),
                                      critter->name() );
                critter->remove_effect( effect_docile );
            } );
            if( f == 0 ) {
                p->add_msg_if_player( _( "You are not commanding any robots." ) );
                return 0;
            }
            return it->type->charges_to_use();
        }
    }
    return 0;
}

static void init_memory_card_with_random_stuff( item &it )
{
    if( it.has_flag( flag_MC_MOBILE ) && ( it.has_flag( flag_MC_RANDOM_STUFF ) ||
                                           it.has_flag( flag_MC_SCIENCE_STUFF ) ) && !( it.has_flag( flag_MC_USED ) ||
                                                   it.has_flag( flag_MC_HAS_DATA ) ) ) {

        it.set_flag( flag_MC_HAS_DATA );

        bool encrypted = false;

        if( it.has_flag( flag_MC_MAY_BE_ENCRYPTED ) && one_in( 8 ) ) {
            it.convert( itype_id( it.typeId().str() + "_encrypted" ) );
        }

        //some special cards can contain "MC_ENCRYPTED" flag
        if( it.has_flag( flag_MC_ENCRYPTED ) ) {
            encrypted = true;
        }

        int data_chance = 2;

        //encrypted memory cards often contain data
        if( encrypted && !one_in( 3 ) ) {
            data_chance--;
        }

        //just empty memory card
        if( !one_in( data_chance ) ) {
            return;
        }

        //add someone's personal photos
        if( one_in( data_chance ) ) {

            //decrease chance to more data
            data_chance++;

            if( encrypted && one_in( 3 ) ) {
                data_chance--;
            }

            const int duckfaces_count = rng( 5, 30 );
            it.set_var( "MC_PHOTOS", duckfaces_count );
        }
        //decrease chance to music and other useful data
        data_chance++;
        if( encrypted && one_in( 2 ) ) {
            data_chance--;
        }

        if( one_in( data_chance ) ) {
            data_chance++;

            if( encrypted && one_in( 3 ) ) {
                data_chance--;
            }

            const int new_songs_count = rng( 5, 15 );
            it.set_var( "MC_MUSIC", new_songs_count );
        }
        data_chance++;
        if( encrypted && one_in( 2 ) ) {
            data_chance--;
        }

        if( one_in( data_chance ) ) {
            it.set_var( "MC_RECIPE", "SIMPLE" );
        }

        if( it.has_flag( flag_MC_SCIENCE_STUFF ) ) {
            it.set_var( "MC_RECIPE", "SCIENCE" );
        }
    }
}

static bool einkpc_download_memory_card( player &p, item &eink, item &mc )
{
    bool something_downloaded = false;
    if( mc.get_var( "MC_PHOTOS", 0 ) > 0 ) {
        something_downloaded = true;

        int new_photos = mc.get_var( "MC_PHOTOS", 0 );
        mc.erase_var( "MC_PHOTOS" );

        p.add_msg_if_player( m_good, vgettext( "You download %d new photo into internal memory.",
                                               "You download %d new photos into internal memory.", new_photos ), new_photos );

        const int old_photos = eink.get_var( "EIPC_PHOTOS", 0 );
        eink.set_var( "EIPC_PHOTOS", old_photos + new_photos );
    }

    if( mc.get_var( "MC_MUSIC", 0 ) > 0 ) {
        something_downloaded = true;

        int new_songs = mc.get_var( "MC_MUSIC", 0 );
        mc.erase_var( "MC_MUSIC" );

        p.add_msg_if_player( m_good, vgettext( "You download %d new song into internal memory.",
                                               "You download %d new songs into internal memory.", new_songs ), new_songs );

        const int old_songs = eink.get_var( "EIPC_MUSIC", 0 );
        eink.set_var( "EIPC_MUSIC", old_songs + new_songs );
    }

    if( !mc.get_var( "MC_RECIPE" ).empty() ) {
        const bool science = mc.get_var( "MC_RECIPE" ) == "SCIENCE";

        mc.erase_var( "MC_RECIPE" );

        std::vector<const recipe *> candidates;

        for( const auto &e : recipe_dict ) {
            const auto &r = e.second;
            if( r.never_learn ) {
                continue;
            }
            if( science ) {
                if( r.difficulty >= 3 && one_in( r.difficulty + 1 ) ) {
                    candidates.push_back( &r );
                }
            } else {
                if( r.category == "CC_FOOD" ) {
                    if( r.difficulty <= 3 && one_in( r.difficulty ) ) {
                        candidates.push_back( &r );
                    }
                }

            }

        }

        if( !candidates.empty() ) {

            const recipe *r = random_entry( candidates );
            const recipe_id &rident = r->ident();

            const auto old_recipes = eink.get_var( "EIPC_RECIPES" );
            if( old_recipes.empty() ) {
                something_downloaded = true;
                eink.set_var( "EIPC_RECIPES", "," + rident.str() + "," );

                p.add_msg_if_player( m_good, _( "You download a recipe for %s into the tablet's memory." ),
                                     r->result_name() );
            } else {
                if( old_recipes.find( "," + rident.str() + "," ) == std::string::npos ) {
                    something_downloaded = true;
                    eink.set_var( "EIPC_RECIPES", old_recipes + rident.str() + "," );

                    p.add_msg_if_player( m_good, _( "You download a recipe for %s into the tablet's memory." ),
                                         r->result_name() );
                } else {
                    p.add_msg_if_player( m_good, _( "Your tablet already has a recipe for %s." ),
                                         r->result_name() );
                }
            }
        }
    }

    if( mc.has_var( "MC_EXTENDED_PHOTOS" ) ) {
        std::vector<extended_photo_def> extended_photos;
        try {
            item_read_extended_photos( mc, extended_photos, "MC_EXTENDED_PHOTOS" );
            item_read_extended_photos( eink, extended_photos, "EIPC_EXTENDED_PHOTOS", true );
            item_write_extended_photos( eink, extended_photos, "EIPC_EXTENDED_PHOTOS" );
            something_downloaded = true;
            p.add_msg_if_player( m_good, _( "You have downloaded your photos." ) );
        } catch( const JsonError &e ) {
            debugmsg( "Error card reading photos (loaded photos = %i) : %s", extended_photos.size(),
                      e.c_str() );
        }
    }

    const auto monster_photos = mc.get_var( "MC_MONSTER_PHOTOS" );
    if( !monster_photos.empty() ) {
        something_downloaded = true;
        p.add_msg_if_player( m_good, _( "You have updated your monster collection." ) );

        auto photos = eink.get_var( "EINK_MONSTER_PHOTOS" );
        if( photos.empty() ) {
            eink.set_var( "EINK_MONSTER_PHOTOS", monster_photos );
        } else {
            std::istringstream f( monster_photos );
            std::string s;
            while( getline( f, s, ',' ) ) {

                if( s.empty() ) {
                    continue;
                }

                const std::string mtype = s;
                getline( f, s, ',' );
                char *chq = s.data();
                const int quality = atoi( chq );

                const size_t eink_strpos = photos.find( "," + mtype + "," );

                if( eink_strpos == std::string::npos ) {
                    photos += mtype + "," + string_format( "%d", quality ) + ",";
                } else {

                    const size_t strqpos = eink_strpos + mtype.size() + 2;
                    char *chq = &photos[strqpos];
                    const int old_quality = atoi( chq );

                    if( quality > old_quality ) {
                        photos[strqpos] = string_format( "%d", quality )[0];
                    }
                }

            }
            eink.set_var( "EINK_MONSTER_PHOTOS", photos );
        }
    }

    if( mc.has_flag( flag_MC_TURN_USED ) ) {
        mc.clear_vars();
        mc.unset_flags();
        mc.convert( itype_mobile_memory_card_used );
    }

    if( !something_downloaded ) {
        p.add_msg_if_player( m_info, _( "This memory card does not contain any new data." ) );
        return false;
    }

    return true;

}

static std::string photo_quality_name( const int index )
{
    static const std::array<std::string, 6> names {
        {
            //~ photo quality adjective
            { translate_marker( "awful" ) }, { translate_marker( "bad" ) }, { translate_marker( "not bad" ) }, { translate_marker( "good" ) }, { translate_marker( "fine" ) }, { translate_marker( "exceptional" ) }
        }
    };
    return _( names[index] );
}

int iuse::einktabletpc( player *p, item *it, bool t, const tripoint &pos )
{
    if( t ) {
        if( !it->get_var( "EIPC_MUSIC_ON" ).empty() && ( it->ammo_remaining() > 0 ) ) {
            if( calendar::once_every( 5_minutes ) ) {
                it->ammo_consume( 1, p->pos() );
            }

            //the more varied music, the better max mood.
            const int songs = it->get_var( "EIPC_MUSIC", 0 );
            play_music( *p, pos, 8, std::min( 25, songs ) );
        } else {
            it->deactivate();
            it->erase_var( "EIPC_MUSIC_ON" );
            p->add_msg_if_player( m_info, _( "Tablet's batteries are dead." ) );
        }

        return 0;
    } else if( p->is_mounted() ) {
        p->add_msg_if_player( m_info, _( "You cannot do that while mounted." ) );
        return 0;
    } else if( !p->is_npc() ) {

        enum {
            ei_invalid, ei_photo, ei_music, ei_recipe, ei_uploaded_photos, ei_monsters, ei_download, ei_decrypt
        };

        if( p->is_underwater() ) {
            p->add_msg_if_player( m_info, _( "You can't do that while underwater." ) );
            return 0;
        }
        if( p->has_trait( trait_ILLITERATE ) ) {
            p->add_msg_if_player( m_info, _( "You cannot read a computer screen." ) );
            return 0;
        }
        if( p->has_trait( trait_HYPEROPIC ) && !p->worn_with_flag( flag_FIX_FARSIGHT ) &&
            !p->has_effect( effect_contacts ) && !p->has_bionic( bio_eye_optic ) ) {
            p->add_msg_if_player( m_info,
                                  _( "You'll need to put on reading glasses before you can see the screen." ) );
            return 0;
        }

        uilist amenu;

        amenu.text = _( "Choose menu option:" );

        const int photos = it->get_var( "EIPC_PHOTOS", 0 );
        if( photos > 0 ) {
            amenu.addentry( ei_photo, true, 'p', _( "Unsorted photos [%d]" ), photos );
        } else {
            amenu.addentry( ei_photo, false, 'p', _( "No photos on device" ) );
        }

        const int songs = it->get_var( "EIPC_MUSIC", 0 );
        if( songs > 0 ) {
            if( it->is_active() ) {
                amenu.addentry( ei_music, true, 'm', _( "Turn music off" ) );
            } else {
                amenu.addentry( ei_music, true, 'm', _( "Turn music on [%d]" ), songs );
            }
        } else {
            amenu.addentry( ei_music, false, 'm', _( "No music on device" ) );
        }

        if( !it->get_var( "EIPC_RECIPES" ).empty() ) {
            amenu.addentry( ei_recipe, true, 'r', _( "View recipes on E-ink screen" ) );
        }

        if( !it->get_var( "EIPC_EXTENDED_PHOTOS" ).empty() ) {
            amenu.addentry( ei_uploaded_photos, true, 'l', _( "Your photos" ) );
        }

        if( !it->get_var( "EINK_MONSTER_PHOTOS" ).empty() ) {
            amenu.addentry( ei_monsters, true, 'y', _( "Your collection of monsters" ) );
        } else {
            amenu.addentry( ei_monsters, false, 'y', _( "Collection of monsters is empty" ) );
        }

        amenu.addentry( ei_download, true, 'w', _( "Download data from memory card" ) );

        /** @EFFECT_COMPUTER >2 allows decrypting memory cards more easily */
        if( p->get_skill_level( skill_computer ) > 2 ) {
            amenu.addentry( ei_decrypt, true, 'd', _( "Decrypt memory card" ) );
        } else {
            amenu.addentry( ei_decrypt, false, 'd', _( "Decrypt memory card (low skill)" ) );
        }

        amenu.query();

        const int choice = amenu.ret;

        if( ei_photo == choice ) {

            const int photos = it->get_var( "EIPC_PHOTOS", 0 );
            const int viewed = std::min( photos, rng( 10, 30 ) );
            const int count = photos - viewed;
            if( count == 0 ) {
                it->erase_var( "EIPC_PHOTOS" );
            } else {
                it->set_var( "EIPC_PHOTOS", count );
            }

            p->moves -= to_moves<int>( rng( 3_seconds, 7_seconds ) );

            if( p->has_trait( trait_PSYCHOPATH ) ) {
                p->add_msg_if_player( m_info, _( "Wasted time, these pictures do not provoke your senses." ) );
            } else {
                p->add_morale( MORALE_PHOTOS, rng( 15, 30 ), 100 );

                const int random_photo = rng( 1, 20 );
                switch( random_photo ) {
                    case 1:
                        p->add_msg_if_player( m_good, _( "You used to have a dog like this…" ) );
                        break;
                    case 2:
                        p->add_msg_if_player( m_good, _( "Ha-ha!  An amusing cat photo." ) );
                        break;
                    case 3:
                        p->add_msg_if_player( m_good, _( "Excellent pictures of nature." ) );
                        break;
                    case 4:
                        p->add_msg_if_player( m_good, _( "Food photos… your stomach rumbles!" ) );
                        break;
                    case 5:
                        p->add_msg_if_player( m_good, _( "Some very interesting travel photos." ) );
                        break;
                    case 6:
                        p->add_msg_if_player( m_good, _( "Pictures of a concert of popular band." ) );
                        break;
                    case 7:
                        p->add_msg_if_player( m_good, _( "Photos of someone's luxurious house." ) );
                        break;
                    default:
                        p->add_msg_if_player( m_good, _( "You feel nostalgic as you stare at the photo." ) );
                        break;
                }
            }

            return it->type->charges_to_use();
        }

        if( ei_music == choice ) {

            p->moves -= 30;

            if( it->is_active() ) {
                it->deactivate();
                it->erase_var( "EIPC_MUSIC_ON" );

                p->add_msg_if_player( m_info, _( "You turned off music on your %s." ), it->tname() );
            } else {
                it->activate();
                it->set_var( "EIPC_MUSIC_ON", "1" );

                p->add_msg_if_player( m_info, _( "You turned on music on your %s." ), it->tname() );

            }

            return it->type->charges_to_use();
        }

        if( ei_recipe == choice ) {
            p->moves -= 50;

            uilist rmenu;

            rmenu.text = _( "List recipes:" );

            std::vector<recipe_id> candidate_recipes;
            std::istringstream f( it->get_var( "EIPC_RECIPES" ) );
            std::string s;
            int k = 0;
            while( getline( f, s, ',' ) ) {

                if( s.empty() ) {
                    continue;
                }

                candidate_recipes.emplace_back( s );

                const auto &recipe = *candidate_recipes.back();
                if( recipe ) {
                    rmenu.addentry( k++, true, -1, recipe.result_name( /*decorated=*/true ) );
                }
            }

            rmenu.query();

            return it->type->charges_to_use();
        }

        if( ei_uploaded_photos == choice ) {
            show_photo_selection( *p, *it, "EIPC_EXTENDED_PHOTOS" );
            return it->type->charges_to_use();
        }

        if( ei_monsters == choice ) {

            uilist pmenu;

            pmenu.text = _( "Your collection of monsters:" );

            std::vector<mtype_id> monster_photos;

            std::istringstream f( it->get_var( "EINK_MONSTER_PHOTOS" ) );
            std::string s;
            int k = 0;
            while( getline( f, s, ',' ) ) {
                if( s.empty() ) {
                    continue;
                }
                monster_photos.emplace_back( s );
                std::string menu_str;
                const monster dummy( monster_photos.back() );
                menu_str = dummy.name();
                getline( f, s, ',' );
                char *chq = s.data();
                const int quality = atoi( chq );
                menu_str += " [" + photo_quality_name( quality ) + "]";
                pmenu.addentry( k++, true, -1, menu_str.c_str() );
            }

            int choice;
            do {
                pmenu.query();
                choice = pmenu.ret;

                if( choice < 0 ) {
                    break;
                }

                const monster dummy( monster_photos[choice] );
                popup( dummy.type->get_description() );
            } while( true );
            return it->type->charges_to_use();
        }

        avatar *you = p->as_avatar();
        item *loc = nullptr;
        auto filter = []( const item & it ) {
            return it.has_flag( flag_MC_MOBILE );
        };
        const std::string title = _( "Insert memory card" );

        if( ei_download == choice ) {

            p->moves -= to_moves<int>( 2_seconds );

            if( you != nullptr ) {
                loc = game_menus::inv::titled_filter_menu( filter, *you, title );
            }
            if( !loc ) {
                p->add_msg_if_player( m_info, _( "You do not have that item!" ) );
                return it->type->charges_to_use();
            }
            item &mc = *loc;

            if( !mc.has_flag( flag_MC_MOBILE ) ) {
                p->add_msg_if_player( m_info, _( "This is not a compatible memory card." ) );
                return it->type->charges_to_use();
            }

            init_memory_card_with_random_stuff( mc );

            if( mc.has_flag( flag_MC_ENCRYPTED ) ) {
                p->add_msg_if_player( m_info, _( "This memory card is encrypted." ) );
                return it->type->charges_to_use();
            }
            if( !mc.has_flag( flag_MC_HAS_DATA ) ) {
                p->add_msg_if_player( m_info, _( "This memory card does not contain any new data." ) );
                return it->type->charges_to_use();
            }

            einkpc_download_memory_card( *p, *it, mc );

            return it->type->charges_to_use();
        }

        if( ei_decrypt == choice ) {
            p->moves -= to_moves<int>( 2_seconds );
            if( you != nullptr ) {
                loc = game_menus::inv::titled_filter_menu( filter, *you, title );
            }
            if( !loc ) {
                p->add_msg_if_player( m_info, _( "You do not have that item!" ) );
                return it->type->charges_to_use();
            }
            item &mc = *loc;

            if( !mc.has_flag( flag_MC_MOBILE ) ) {
                p->add_msg_if_player( m_info, _( "This is not a compatible memory card." ) );
                return it->type->charges_to_use();
            }

            init_memory_card_with_random_stuff( mc );

            if( !mc.has_flag( flag_MC_ENCRYPTED ) ) {
                p->add_msg_if_player( m_info, _( "This memory card is not encrypted." ) );
                return it->type->charges_to_use();
            }

            p->practice( skill_computer, rng( 2, 5 ) );

            /** @EFFECT_INT increases chance of safely decrypting memory card */

            /** @EFFECT_COMPUTER increases chance of safely decrypting memory card */
            const int success = p->get_skill_level( skill_computer ) * rng( 1,
                                p->get_skill_level( skill_computer ) ) *
                                rng( 1, p->int_cur ) - rng( 30, 80 );
            if( success > 0 ) {
                p->practice( skill_computer, rng( 5, 10 ) );
                p->add_msg_if_player( m_good, _( "You successfully decrypted content on %s!" ),
                                      mc.tname() );
                einkpc_download_memory_card( *p, *it, mc );
            } else {
                if( success > -10 || one_in( 5 ) ) {
                    p->add_msg_if_player( m_neutral, _( "You failed to decrypt the %s." ), mc.tname() );
                } else {
                    p->add_msg_if_player( m_bad,
                                          _( "You tripped the firmware protection, and the card deleted its data!" ) );
                    mc.clear_vars();
                    mc.unset_flags();
                    mc.convert( itype_mobile_memory_card_used );
                }
            }
            return it->type->charges_to_use();
        }
    }
    return 0;
}


static std::string colorized_trap_name_at( const tripoint &point )
{
    const trap &trap = g->m.tr_at( point );
    std::string name;
    if( !trap.is_null() && trap.get_visibility() <= 1 ) {
        name = colorize( trap.name(), trap.color ) + _( " on " );
    }
    return name;
}

static std::string colorized_field_description_at( const tripoint &point )
{
    std::string field_text;
    const field &field = g->m.field_at( point );
    const field_entry *entry = field.find_field( field.displayed_field_type() );
    if( entry ) {
        field_text = string_format( _( description_affixes.at( field.displayed_description_affix() ) ),
                                    colorize( entry->name(), entry->color() ) );
    }
    return field_text;
}

static std::string colorized_item_name( const item &item )
{
    nc_color color = item.color_in_inventory();
    std::string damtext = item.damage() != 0 ? item.durability_indicator() : "";
    return damtext + colorize( item.tname( 1, false ), color );
}

static std::string colorized_item_description( const item &item )
{
    iteminfo_query query = iteminfo_query(
    std::vector<iteminfo_parts> {
        iteminfo_parts::DESCRIPTION,
        iteminfo_parts::DESCRIPTION_NOTES,
        iteminfo_parts::DESCRIPTION_CONTENTS
    } );
    return item.info_string( query, 1 );
}

static const item &get_top_item_at_point( const tripoint &point,
        const units::volume &min_visible_volume )
{
    map_stack items = g->m.i_at( point );
    // iterate from topmost item down to ground
    for( const item * const &it : items ) {
        if( it->volume() > min_visible_volume ) {
            // return top (or first big enough) item to the list
            return *it;
        }
    }
    return null_item_reference();
}

static std::string colorized_ter_name_flags_at( const tripoint &point,
        const std::vector<std::string> &flags, const std::vector<ter_str_id> &ter_whitelist )
{
    const ter_id ter = g->m.ter( point );
    std::string name = colorize( ter->name(), ter->color() );
    const std::string &graffiti_message = g->m.graffiti_at( point );

    if( !graffiti_message.empty() ) {
        name += string_format( _( " with graffiti \"%s\"" ), graffiti_message );
        return name;
    }
    if( ter_whitelist.empty() && flags.empty() ) {
        return name;
    }
    if( !ter->open.is_null() || ( ter->examine != iexamine::none &&
                                  ter->examine != iexamine::fungus &&
                                  ter->examine != iexamine::water_source &&
                                  ter->examine != iexamine::dirtmound ) ) {
        return name;
    }
    for( const ter_str_id &ter_good : ter_whitelist ) {
        if( ter->id == ter_good ) {
            return name;
        }
    }
    for( const std::string &flag : flags ) {
        if( ter->has_flag( flag ) ) {
            return name;
        }
    }

    return std::string();
}

static std::string colorized_feature_description_at( const tripoint &center_point, bool &item_found,
        const units::volume &min_visible_volume )
{
    item_found = false;
    const furn_id furn = g->m.furn( center_point );
    if( furn != f_null && furn.is_valid() ) {
        std::string furn_str = colorize( furn->name(), c_yellow );
        std::string sign_message = g->m.get_signage( center_point );
        if( !sign_message.empty() ) {
            furn_str += string_format( _( " with message \"%s\"" ), sign_message );
        }
        if( !furn->has_flag( "CONTAINER" ) && !furn->has_flag( "SEALED" ) ) {
            const item &item = get_top_item_at_point( center_point, min_visible_volume );
            if( !item.is_null() ) {
                furn_str += string_format( _( " with %s on it" ), colorized_item_name( item ) );
                item_found = true;
            }
        }
        return furn_str;
    }
    return std::string();
}

static std::string format_object_pair( const std::pair<std::string, int> &pair,
                                       const std::string &article )
{
    if( pair.second == 1 ) {
        return article + pair.first;
    } else if( pair.second > 1 ) {
        return string_format( "%i %s", pair.second, pair.first );
    }
    return std::string();
}
static std::string format_object_pair_article( const std::pair<std::string, int> &pair )
{
    return format_object_pair( pair, pgettext( "Article 'a', replace it with empty "
                               "string if it is not used in language", "a " ) );
}
static std::string format_object_pair_no_article( const std::pair<std::string, int> &pair )
{
    return format_object_pair( pair, "" );
}

static std::string effects_description_for_creature( Creature *const creature, std::string &pose,
        const std::string &pronoun_sex )
{
    struct ef_con { // effect constraint
        translation status;
        translation pose;
        int intensity_lower_limit;
        ef_con( const translation &status, const translation &pose, int intensity_lower_limit ) :
            status( status ), pose( pose ), intensity_lower_limit( intensity_lower_limit ) {}
        ef_con( const translation &status, const translation &pose ) :
            status( status ), pose( pose ), intensity_lower_limit( 0 ) {}
        ef_con( const translation &status, int intensity_lower_limit ) :
            status( status ), intensity_lower_limit( intensity_lower_limit ) {}
        ef_con( const translation &status ) :
            status( status ), intensity_lower_limit( 0 ) {}
    };
    static const std::unordered_map<efftype_id, ef_con> vec_effect_status = {
        { effect_onfire, ef_con( to_translation( " is on <color_red>fire</color>. " ) ) },
        { effect_bleed, ef_con( to_translation( " is <color_red>bleeding</color>. " ), 1 ) },
        { effect_happy, ef_con( to_translation( " looks <color_green>happy</color>. " ), 13 ) },
        { effect_downed, ef_con( translation(), to_translation( "downed" ) ) },
        { effect_in_pit, ef_con( translation(), to_translation( "stuck" ) ) },
        { effect_stunned, ef_con( to_translation( " is <color_blue>stunned</color>. " ) ) },
        { effect_dazed, ef_con( to_translation( " is <color_blue>dazed</color>. " ) ) },
        { effect_beartrap, ef_con( to_translation( " is stuck in beartrap. " ) ) },
        { effect_laserlocked, ef_con( to_translation( " have tiny <color_red>red dot</color> on body. " ) ) },
        { effect_boomered, ef_con( to_translation( " is covered in <color_magenta>bile</color>. " ) ) },
        { effect_glowing, ef_con( to_translation( " is covered in <color_yellow>glowing goo</color>. " ) ) },
        { effect_slimed, ef_con( to_translation( " is covered in <color_green>thick goo</color>. " ) ) },
        { effect_corroding, ef_con( to_translation( " is covered in <color_light_green>acid</color>. " ) ) },
        { effect_sap, ef_con( to_translation( " is coated in <color_brown>sap</color>. " ) ) },
        { effect_webbed, ef_con( to_translation( " is covered in <color_dark_gray>webs</color>. " ) ) },
        { effect_spores, ef_con( to_translation( " is covered in <color_green>spores</color>. " ), 1 ) },
        { effect_crushed, ef_con( to_translation( " lies under <color_dark_gray>collapsed debris</color>. " ), to_translation( "lies" ) ) },
        { effect_lack_sleep, ef_con( to_translation( " looks <color_dark_gray>very tired</color>. " ) ) },
        { effect_lying_down, ef_con( to_translation( " is <color_dark_blue>sleeping</color>. " ), to_translation( "lies" ) ) },
        { effect_sleep, ef_con( to_translation( " is <color_dark_blue>sleeping</color>. " ), to_translation( "lies" ) ) },
        { effect_haslight, ef_con( to_translation( " is <color_yellow>lit</color>. " ) ) },
        { effect_saddled, ef_con( to_translation( " is <color_dark_gray>saddled</color>. " ) ) },
        { effect_harnessed, ef_con( to_translation( " is being <color_dark_gray>harnessed</color> by a vehicle. " ) ) },
        { effect_monster_armor, ef_con( to_translation( " is <color_dark_gray>wearing armor</color>. " ) ) },
        { effect_has_bag, ef_con( to_translation( " have <color_dark_gray>bag</color> attached. " ) ) },
        { effect_tied, ef_con( to_translation( " is <color_dark_gray>tied</color>. " ) ) },
        { effect_bouldering, ef_con( translation(), to_translation( "balancing" ) ) }
    };

    std::string figure_effects;
    if( creature ) {
        for( const auto &pair : vec_effect_status ) {
            if( creature->get_effect_int( pair.first ) > pair.second.intensity_lower_limit ) {
                if( !pair.second.status.empty() ) {
                    figure_effects += pronoun_sex + pair.second.status;
                }
                if( !pair.second.pose.empty() ) {
                    pose = pair.second.pose.translated();
                }
            }
        }
        if( creature->has_effect( effect_sad ) ) {
            int intensity = creature->get_effect_int( effect_sad );
            if( intensity > 500 && intensity <= 950 ) {
                figure_effects += pronoun_sex + pgettext( "Someone", " looks <color_blue>sad</color>. " );
            } else if( intensity > 950 ) {
                figure_effects += pronoun_sex + pgettext( "Someone", " looks <color_blue>depressed</color>. " );
            }
        }
        float pain = creature->get_pain() / 10.f;
        if( pain > 3 ) {
            figure_effects += pronoun_sex + pgettext( "Someone", " is writhing in <color_red>pain</color>. " );
        }
        if( creature->has_effect( effect_riding ) ) {
            pose = _( "rides" );
            monster *const mon = g->critter_at<monster>( creature->pos(), false );
            figure_effects += pronoun_sex + string_format( _( " is riding %s. " ),
                              colorize( mon->name(), c_light_blue ) );
        }
        if( creature->has_effect( effect_glowy_led ) ) {
            figure_effects += _( "A bionic LED is <color_yellow>glowing</color> softly. " );
        }
    }
    if( !figure_effects.empty() && figure_effects.back() == ' ' ) { // remove last space
        figure_effects.erase( figure_effects.end() - 1 );
    }
    return figure_effects;
}

struct object_names_collection {
    std::unordered_map<std::string, int>
    furniture,
    vehicles,
    items,
    terrain;

    std::string figure_text;
    std::string obj_nearby_text;
};

static object_names_collection enumerate_objects_around_point( const tripoint &point,
        const int radius, const tripoint &bounds_center_point, const int bounds_radius,
        const tripoint &camera_pos, const units::volume &min_visible_volume, bool create_figure_desc,
        std::unordered_set<tripoint> &ignored_points,
        std::unordered_set<const vehicle *> &vehicles_recorded )
{
    map &here = get_map();
    const tripoint_range<tripoint> bounds =
        here.points_in_radius( bounds_center_point, bounds_radius );
    const tripoint_range<tripoint> points_in_radius = here.points_in_radius( point, radius );
    int dist = rl_dist( camera_pos, point );

    bool item_found = false;
    std::unordered_set<const vehicle *> local_vehicles_recorded( vehicles_recorded );
    object_names_collection ret_obj;

    std::string description_part_on_figure;
    std::string description_furniture_on_figure;
    std::string description_terrain_on_figure;

    // store objects in radius
    for( const tripoint &point_around_figure : points_in_radius ) {
        if( !bounds.is_point_inside( point_around_figure ) ||
            !g->m.sees( camera_pos, point_around_figure, dist + radius ) ||
            ( ignored_points.contains( point_around_figure ) &&
              !( point_around_figure == point && create_figure_desc ) ) ) {
            continue; // disallow photos with not visible objects
        }
        units::volume volume_to_search = point_around_figure == bounds_center_point ? 0_ml :
                                         min_visible_volume;

        std::string furn_desc = colorized_feature_description_at( point_around_figure, item_found,
                                volume_to_search );

        const item &item = get_top_item_at_point( point_around_figure, volume_to_search );

        const optional_vpart_position veh_part_pos = g->m.veh_at( point_around_figure );
        std::string unusual_ter_desc = colorized_ter_name_flags_at( point_around_figure,
                                       camera_ter_whitelist_flags,
                                       camera_ter_whitelist_types );
        std::string ter_desc = colorized_ter_name_flags_at( point_around_figure );

        const std::string trap_name = colorized_trap_name_at( point_around_figure );
        const std::string field_desc = colorized_field_description_at( point_around_figure );

        if( !furn_desc.empty() ) {
            furn_desc = trap_name + furn_desc + field_desc;
            if( point == point_around_figure && create_figure_desc ) {
                description_furniture_on_figure = furn_desc;
            } else {
                ret_obj.furniture[ furn_desc ] ++;
            }
        } else if( veh_part_pos.has_value() ) {
            const vehicle &veh = veh_part_pos->vehicle();
            const std::string veh_name = colorize( veh.disp_name(), c_light_blue );
            const vehicle *veh_hash = &veh_part_pos->vehicle();

            if( !local_vehicles_recorded.contains( veh_hash ) &&
                point != point_around_figure ) {
                // new vehicle, point is not center
                ret_obj.vehicles[ veh_name ] ++;
            } else if( point == point_around_figure ) {
                // point is center
                //~ %1$s: vehicle part name, %2$s: vehicle name
                description_part_on_figure = string_format( pgettext( "vehicle part", "%1$s from %2$s" ),
                                             veh_part_pos.part_displayed()->part().name(), veh_name );
                if( ret_obj.vehicles.contains( veh_name ) &&
                    local_vehicles_recorded.contains( veh_hash ) ) {
                    // remove vehicle name only if we previously added THIS vehicle name (in case of same name)
                    ret_obj.vehicles[ veh_name ] --;
                    if( ret_obj.vehicles[ veh_name ] <= 0 ) {
                        ret_obj.vehicles.erase( veh_name );
                    }
                }
            }
            vehicles_recorded.insert( veh_hash );
            local_vehicles_recorded.insert( veh_hash );
        } else if( !item.is_null() ) {
            std::string item_name = colorized_item_name( item );
            item_name = trap_name + item_name + field_desc;
            if( point == point_around_figure && create_figure_desc ) {
                //~ %1$s: terrain description, %2$s: item name
                description_terrain_on_figure = string_format( pgettext( "terrain and item", "%1$s with a %2$s" ),
                                                ter_desc, item_name );
            } else {
                ret_obj.items[ item_name ] ++;
            }
        } else if( !unusual_ter_desc.empty() ) {
            unusual_ter_desc = trap_name + unusual_ter_desc + field_desc;
            if( point == point_around_figure && create_figure_desc ) {
                description_furniture_on_figure = unusual_ter_desc;
            } else {
                ret_obj.furniture[ unusual_ter_desc ] ++;
            }
        } else if( !ter_desc.empty() && ( !field_desc.empty() || !trap_name.empty() ) ) {
            ter_desc = trap_name + ter_desc + field_desc;
            if( point == point_around_figure && create_figure_desc ) {
                description_terrain_on_figure = ter_desc;
            } else {
                ret_obj.terrain[ ter_desc ] ++;
            }
        } else {
            ter_desc = trap_name + ter_desc + field_desc;
            if( point == point_around_figure && create_figure_desc ) {
                description_terrain_on_figure = ter_desc;
            }
        }
        ignored_points.insert( point_around_figure );
    }

    if( create_figure_desc ) {
        std::vector<std::string> objects_combined_desc;
        int objects_combined_num = 0;
        std::unordered_map<std::string, int> vecs_to_retrieve[4] = {
            ret_obj.furniture, ret_obj.vehicles, ret_obj.items, ret_obj.terrain
        };

        for( int i = 0; i < 4; i++ ) {
            for( const auto &p : vecs_to_retrieve[ i ] ) {
                objects_combined_desc.push_back( i == 1 ?  // vehicle name already includes "the"
                                                 format_object_pair_no_article( p ) : format_object_pair_article( p ) );
                objects_combined_num += p.second;
            }
        }

        const char *transl_str = pgettext( "someone stands/sits *on* something", " on a %s." );
        if( !description_part_on_figure.empty() ) {
            ret_obj.figure_text = string_format( transl_str, description_part_on_figure );
        } else {
            if( !description_furniture_on_figure.empty() ) {
                ret_obj.figure_text = string_format( transl_str, description_furniture_on_figure );
            } else {
                ret_obj.figure_text = string_format( transl_str, description_terrain_on_figure );
            }
        }
        if( !objects_combined_desc.empty() ) {
            // store objects to description_figures_status
            std::string objects_text = enumerate_as_string( objects_combined_desc );
            ret_obj.obj_nearby_text = string_format( vgettext( "Nearby is %s.", "Nearby are %s.",
                                      objects_combined_num ), objects_text );
        }
    }
    return ret_obj;
}

static extended_photo_def photo_def_for_camera_point( const tripoint &aim_point,
        const tripoint &camera_pos,
        std::vector<monster *> &monster_vec, std::vector<player *> &player_vec )
{
    // look for big items on top of stacks in the background for the selfie description
    const units::volume min_visible_volume = 490_ml;

    std::unordered_set<tripoint> ignored_points;
    std::unordered_set<const vehicle *> vehicles_recorded;

    std::unordered_map<std::string, std::string> description_figures_appearance;
    std::vector<std::pair<std::string, std::string>> description_figures_status;

    std::string timestamp = to_string( time_point( calendar::turn ) );
    int dist = rl_dist( camera_pos, aim_point );
    map &here = get_map();
    const tripoint_range<tripoint> bounds = here.points_in_radius( aim_point, 2 );
    extended_photo_def photo;
    bool need_store_weather = false;
    int outside_tiles_num = 0;
    int total_tiles_num = 0;

    const auto map_deincrement_or_erase = []( std::unordered_map<std::string, int> &obj_map,
    const std::string & key ) {
        if( obj_map.contains( key ) ) {
            obj_map[ key ] --;
            if( obj_map[ key ] <= 0 ) {
                obj_map.erase( key );
            }
        }
    };

    // first scan for critters and mark nearby furniture, vehicles and items
    for( const tripoint &current : bounds ) {
        if( !g->m.sees( camera_pos, current, dist + 3 ) ) {
            continue; // disallow photos with non-visible objects
        }
        monster *const mon = g->critter_at<monster>( current, false );
        avatar *guy = g->critter_at<avatar>( current );

        total_tiles_num++;
        if( g->m.is_outside( current ) ) {
            need_store_weather = true;
            outside_tiles_num++;
        }

        if( guy || mon ) {
            std::string figure_appearance, figure_name, pose, pronoun_sex, figure_effects;
            Creature *creature;
            if( mon && mon->has_effect( effect_ridden ) ) {
                // only player can ride, see monexamine::mount_pet
                guy = &g->u;
                description_figures_appearance[ mon->name() ] = "\"" + mon->type->get_description() + "\"";
            }

            if( guy ) {
                if( guy->is_hallucination() ) {
                    continue; // do not include hallucinations
                }
                if( guy->movement_mode_is( CMM_CROUCH ) ) {
                    pose = _( "sits" );
                } else {
                    pose = _( "stands" );
                }
                const std::vector<std::string> vec = guy->short_description_parts();
                figure_appearance = join( vec, "\n\n" );
                figure_name = guy->name;
                pronoun_sex = guy->male ? _( "He" ) : _( "She" );
                creature = guy;
                player_vec.push_back( guy );
            } else {
                if( mon->is_hallucination() || mon->type->in_species( HALLUCINATION ) ) {
                    continue; // do not include hallucinations
                }
                pose = _( "stands" );
                figure_appearance = "\"" + mon->type->get_description() + "\"";
                figure_name = mon->name();
                pronoun_sex = pgettext( "Pronoun", "It" );
                creature = mon;
                monster_vec.push_back( mon );
            }

            figure_effects = effects_description_for_creature( creature, pose, pronoun_sex );
            description_figures_appearance[ figure_name ] = figure_appearance;

            object_names_collection obj_collection = enumerate_objects_around_point( current, 1, aim_point, 2,
                    camera_pos, min_visible_volume, true,
                    ignored_points, vehicles_recorded );
            std::string figure_text = pose + obj_collection.figure_text;

            if( !figure_effects.empty() ) {
                figure_text += " " + figure_effects;
            }
            if( !obj_collection.obj_nearby_text.empty() ) {
                figure_text += " " + obj_collection.obj_nearby_text;
            }
            auto name_text_pair = std::pair<std::string, std::string>( figure_name, figure_text );
            if( current == aim_point ) {
                description_figures_status.insert( description_figures_status.begin(), name_text_pair );
            } else {
                description_figures_status.push_back( name_text_pair );
            }
        }
    }

    // scan for everythin NOT near critters
    object_names_collection obj_coll = enumerate_objects_around_point( aim_point, 2, aim_point, 2,
                                       camera_pos, min_visible_volume, false,
                                       ignored_points, vehicles_recorded );

    std::string photo_text = _( "This is a photo of " );

    bool found_item_aim_point;
    std::string furn_desc = colorized_feature_description_at( aim_point, found_item_aim_point,
                            0_ml );
    const item &item = get_top_item_at_point( aim_point, 0_ml );
    const std::string trap_name = colorized_trap_name_at( aim_point );
    std::string ter_name = colorized_ter_name_flags_at( aim_point, {}, {} );
    const std::string field_desc = colorized_field_description_at( aim_point );

    bool found_vehicle_aim_point = g->m.veh_at( aim_point ).has_value(),
         found_furniture_aim_point = !furn_desc.empty();
    // colorized_feature_description_at do not update flag if no furniture found, so need to check again
    if( !found_furniture_aim_point ) {
        found_item_aim_point = !item.is_null();
    }

    const ter_id ter_aim = g->m.ter( aim_point );
    const furn_id furn_aim = g->m.furn( aim_point );

    if( !description_figures_status.empty() ) {
        std::string names = enumerate_as_string( description_figures_status.begin(),
                            description_figures_status.end(),
        []( const std::pair<std::string, std::string> &it ) {
            return colorize( it.first, c_light_blue );
        } );

        photo.name = names;
        photo_text += names + ".";

        for( const auto &figure_status : description_figures_status ) {
            photo_text += "\n\n" + colorize( figure_status.first, c_light_blue )
                          + " " + figure_status.second;
        }
    } else if( found_vehicle_aim_point ) {
        const optional_vpart_position veh_part_pos = g->m.veh_at( aim_point );
        const std::string veh_name = colorize( veh_part_pos->vehicle().disp_name(), c_light_blue );
        photo.name = veh_name;
        photo_text += veh_name + ".";
        map_deincrement_or_erase( obj_coll.vehicles, veh_name );
    } else if( found_furniture_aim_point || found_item_aim_point )  {
        std::string item_name = colorized_item_name( item );
        if( found_furniture_aim_point ) {
            furn_desc = trap_name + furn_desc + field_desc;
            photo.name = furn_desc;
            photo_text += photo.name + ".";
            map_deincrement_or_erase( obj_coll.furniture, furn_desc );
        } else if( found_item_aim_point ) {
            item_name = trap_name + item_name + field_desc;
            photo.name = item_name;
            photo_text += item_name + ". " + string_format( _( "It lies on the %s." ),
                          ter_name );
            map_deincrement_or_erase( obj_coll.items, item_name );
        }
        if( found_furniture_aim_point && !furn_aim->description.empty() ) {
            photo_text += "\n\n" + colorize( furn_aim->name(), c_yellow ) + ":\n" + furn_aim->description;
        }
        if( found_item_aim_point ) {
            photo_text += "\n\n" + item_name + ":\n" + colorized_item_description( item );
        }
    } else {
        ter_name = trap_name + ter_name + field_desc;
        photo.name = ter_name;
        photo_text += photo.name + ".";
        map_deincrement_or_erase( obj_coll.terrain, ter_name );
        map_deincrement_or_erase( obj_coll.furniture, ter_name );

        if( !ter_aim->description.empty() ) {
            photo_text += "\n\n" + photo.name + ":\n" + ter_aim->description;
        }
    }

    auto num_of = []( const std::unordered_map<std::string, int> &m ) -> int {
        int ret = 0;
        for( const auto &it : m )
        {
            ret += it.second;
        }
        return ret;
    };

    if( !obj_coll.items.empty() ) {
        std::string obj_list = enumerate_as_string( obj_coll.items.begin(), obj_coll.items.end(),
                               format_object_pair_article );
        photo_text += "\n\n" + string_format( vgettext( "There is something lying on the ground: %s.",
                                              "There are some things lying on the ground: %s.", num_of( obj_coll.items ) ),
                                              obj_list );
    }
    if( !obj_coll.furniture.empty() ) {
        std::string obj_list = enumerate_as_string( obj_coll.furniture.begin(), obj_coll.furniture.end(),
                               format_object_pair_article );
        photo_text += "\n\n" + string_format( vgettext( "Something is visible in the background: %s.",
                                              "Some objects are visible in the background: %s.", num_of( obj_coll.furniture ) ),
                                              obj_list );
    }
    if( !obj_coll.vehicles.empty() ) {
        std::string obj_list = enumerate_as_string( obj_coll.vehicles.begin(), obj_coll.vehicles.end(),
                               format_object_pair_no_article );
        photo_text += "\n\n" + string_format( vgettext( "There is %s parked in the background.",
                                              "There are %s parked in the background.", num_of( obj_coll.vehicles ) ),
                                              obj_list );
    }
    if( !obj_coll.terrain.empty() ) {
        std::string obj_list = enumerate_as_string( obj_coll.terrain.begin(), obj_coll.terrain.end(),
                               format_object_pair_article );
        photo_text += "\n\n" + string_format( vgettext( "There is %s in the background.",
                                              "There are %s in the background.", num_of( obj_coll.terrain ) ),
                                              obj_list );
    }

    // TODO: fix point types
    const oter_id &cur_ter =
        overmap_buffer.ter( tripoint_abs_omt( ms_to_omt_copy( g->m.getabs( aim_point ) ) ) );
    std::string overmap_desc = string_format( _( "In the background you can see a %s" ),
                               colorize( cur_ter->get_name(), cur_ter->get_color() ) );
    if( outside_tiles_num == total_tiles_num ) {
        photo_text += _( "\n\nThis photo was taken <color_dark_gray>outside</color>." );
    } else if( outside_tiles_num == 0 ) {
        photo_text += _( "\n\nThis photo was taken <color_dark_gray>inside</color>." );
        overmap_desc += _( " interior" );
    } else if( outside_tiles_num < total_tiles_num / 2.0 ) {
        photo_text += _( "\n\nThis photo was taken mostly <color_dark_gray>inside</color>,"
                         " but <color_dark_gray>outside</color> can be seen." );
        overmap_desc += _( " interior" );
    } else if( outside_tiles_num >= total_tiles_num / 2.0 ) {
        photo_text += _( "\n\nThis photo was taken mostly <color_dark_gray>outside</color>,"
                         " but <color_dark_gray>inside</color> can be seen." );
    }
    photo_text += "\n" + overmap_desc + ".";

    if( g->get_levz() >= 0 && need_store_weather ) {
        photo_text += "\n\n";
        if( is_dawn( calendar::turn ) ) {
            photo_text += _( "It is <color_yellow>sunrise</color>. " );
        } else if( is_dusk( calendar::turn ) ) {
            photo_text += _( "It is <color_light_red>sunset</color>. " );
        } else if( is_night( calendar::turn ) ) {
            photo_text += _( "It is <color_dark_gray>night</color>. " );
        } else {
            photo_text += _( "It is day. " );
        }
        photo_text += string_format( _( "The weather is %s." ), colorize( get_weather().weather_id->name,
                                     get_weather().weather_id->color ) );
    }

    for( const auto &figure : description_figures_appearance ) {
        photo_text += "\n\n" + string_format( _( "%s appearance:" ),
                                              colorize( figure.first, c_light_blue ) ) + "\n" + figure.second;
    }

    photo_text += "\n\n" + string_format( pgettext( "Date", "The photo was taken on %s." ),
                                          colorize( timestamp, c_light_blue ) );

    photo.description = photo_text;

    return photo;
}

static void item_save_monsters( player &p, item &it, const std::vector<monster *> &monster_vec,
                                const int photo_quality )
{
    std::string monster_photos = it.get_var( "CAMERA_MONSTER_PHOTOS" );
    if( monster_photos.empty() ) {
        monster_photos = ",";
    }

    for( monster * const &monster_p : monster_vec ) {
        const std::string mtype = monster_p->type->id.str();
        const std::string name = monster_p->name();

        // position of <monster type string>
        const size_t mon_str_pos = monster_photos.find( "," + mtype + "," );

        // monster gets recorded by the character, add to known types
        p.set_knows_creature_type( monster_p->type->id );

        if( mon_str_pos == std::string::npos ) { // new monster
            monster_photos += string_format( "%s,%d,", mtype, photo_quality );
        } else { // replace quality character, if new photo is better
            const size_t quality_num_pos = mon_str_pos + mtype.size() + 2;
            char *quality_char = &monster_photos[ quality_num_pos ];
            const int old_quality = atoi( quality_char ); // get qual number from char

            if( photo_quality > old_quality ) {
                monster_photos[ quality_num_pos ] = string_format( "%d", photo_quality )[ 0 ];
            }
            if( !p.is_blind() ) {
                if( photo_quality > old_quality ) {
                    p.add_msg_if_player( m_good, _( "The quality of %s image is better than the previous one." ),
                                         colorize( name, c_light_blue ) );
                } else if( old_quality == 5 ) {
                    p.add_msg_if_player( _( "The quality of stored %s image is already maximally detailed." ),
                                         colorize( name, c_light_blue ) );
                } else {
                    p.add_msg_if_player( m_bad, _( "But the quality of %s image is worse than the previous one." ),
                                         colorize( name, c_light_blue ) );
                }
            }
        }
    }
    it.set_var( "CAMERA_MONSTER_PHOTOS", monster_photos );
}

// throws exception
static bool item_read_extended_photos( item &it, std::vector<extended_photo_def> &extended_photos,
                                       const std::string &var_name, bool insert_at_begin )
{
    bool result = false;
    std::istringstream extended_photos_data( it.get_var( var_name ) );
    JsonIn json( extended_photos_data );
    if( insert_at_begin ) {
        std::vector<extended_photo_def> temp_vec;
        result = json.read( temp_vec );
        extended_photos.insert( std::begin( extended_photos ), std::begin( temp_vec ),
                                std::end( temp_vec ) );
    } else {
        result = json.read( extended_photos );
    }
    return result;
}

// throws exception
static void item_write_extended_photos( item &it,
                                        const std::vector<extended_photo_def> &extended_photos,
                                        const std::string &var_name )
{
    std::ostringstream extended_photos_data;
    JsonOut json( extended_photos_data );
    json.write( extended_photos );
    it.set_var( var_name, extended_photos_data.str() );
}

static bool show_photo_selection( player &p, item &it, const std::string &var_name )
{
    if( p.is_blind() ) {
        p.add_msg_if_player( _( "You can't see the camera screen, you're blind." ) );
        return false;
    }

    uilist pmenu;
    pmenu.text = _( "Photos saved on camera:" );

    std::vector<std::string> descriptions;
    std::vector<extended_photo_def> extended_photos;

    try {
        item_read_extended_photos( it, extended_photos, var_name );
    } catch( const JsonError &e ) {
        debugmsg( "Error reading photos: %s", e.c_str() );
    }
    try { // if there is old photos format, append them; delete old and save new
        if( item_read_extended_photos( it, extended_photos, "CAMERA_NPC_PHOTOS", true ) ) {
            it.erase_var( "CAMERA_NPC_PHOTOS" );
            item_write_extended_photos( it, extended_photos, var_name );
        }
    } catch( const JsonError &e ) {
        debugmsg( "Error migrating old photo format: %s", e.c_str() );
    }

    int k = 0;
    for( const extended_photo_def &extended_photo : extended_photos ) {
        std::string menu_str = extended_photo.name;

        size_t index = menu_str.find( p.name );
        if( index != std::string::npos ) {
            menu_str.replace( index, p.name.length(), _( "You" ) );
        }

        descriptions.push_back( extended_photo.description );
        menu_str += " [" + photo_quality_name( extended_photo.quality ) + "]";

        pmenu.addentry( k++, true, -1, menu_str.c_str() );
    }

    int choice;
    do {
        pmenu.query();
        choice = pmenu.ret;

        if( choice < 0 ) {
            break;
        }
        popup( "%s", descriptions[choice].c_str() );

    } while( true );
    return true;
}

int iuse::camera( player *p, item *it, bool, const tripoint & )
{
    enum {c_shot, c_photos, c_monsters, c_upload};

    // CAMERA_NPC_PHOTOS is old save variable
    bool found_extended_photos = !it->get_var( "CAMERA_NPC_PHOTOS" ).empty() ||
                                 !it->get_var( "CAMERA_EXTENDED_PHOTOS" ).empty();
    bool found_monster_photos = !it->get_var( "CAMERA_MONSTER_PHOTOS" ).empty();

    uilist amenu;
    amenu.text = _( "What to do with camera?" );
    amenu.addentry( c_shot, true, 't', _( "Take a photo" ) );
    if( !found_extended_photos && !found_monster_photos ) {
        amenu.addentry( c_photos, false, 'l', _( "No photos in memory" ) );
    } else {
        if( found_extended_photos ) {
            amenu.addentry( c_photos, true, 'l', _( "List photos" ) );
        }
        if( found_monster_photos ) {
            amenu.addentry( c_monsters, true, 'm', _( "Your collection of monsters" ) );
        }
        amenu.addentry( c_upload, true, 'u', _( "Upload photos to memory card" ) );
    }

    amenu.query();
    const int choice = amenu.ret;

    if( choice < 0 ) {
        return 0;
    }

    if( c_shot == choice ) {
        const std::optional<tripoint> aim_point_ = g->look_around();

        if( !aim_point_ ) {
            p->add_msg_if_player( _( "Never mind." ) );
            return 0;
        }
        tripoint aim_point = *aim_point_;
        bool incorrect_focus = false;
        tripoint_range<tripoint> aim_bounds = g->m.points_in_radius( aim_point, 2 );

        std::vector<tripoint> trajectory = line_to( p->pos(), aim_point, 0, 0 );
        trajectory.push_back( aim_point );

        p->moves -= 50;
        sounds::sound( p->pos(), 8, sounds::sound_t::activity, _( "Click." ), true, "tool",
                       "camera_shutter" );

        for( std::vector<tripoint>::iterator point_it = trajectory.begin();
             point_it != trajectory.end();
             ++point_it ) {
            const tripoint trajectory_point = *point_it;
            if( point_it != trajectory.end() ) {
                const tripoint next_point = *( point_it + 1 ); // Trajectory ends on last visible tile
                if( !g->m.sees( p->pos(), next_point, rl_dist( p->pos(), next_point ) + 3 ) ) {
                    p->add_msg_if_player( _( "You have the wrong camera focus." ) );
                    incorrect_focus = true;
                    // recalculate target point
                    aim_point = trajectory_point;
                    aim_bounds = g->m.points_in_radius( trajectory_point, 2 );
                }
            }

            monster *const mon = g->critter_at<monster>( trajectory_point, true );
            player *const guy = g->critter_at<player>( trajectory_point );
            if( mon || guy || trajectory_point == aim_point ) {
                int dist = rl_dist( p->pos(), trajectory_point );

                int camera_bonus = it->has_flag( flag_CAMERA_PRO ) ? 10 : 0;
                int photo_quality = 20 - rng( dist, dist * 2 ) * 2 + rng( camera_bonus / 2, camera_bonus );
                if( photo_quality > 5 ) {
                    photo_quality = 5;
                }
                if( photo_quality < 0 ) {
                    photo_quality = 0;
                }
                if( p->is_blind() ) {
                    photo_quality /= 2;
                }

                if( mon ) {
                    monster &z = *mon;

                    // shoot past small monsters and hallucinations
                    if( trajectory_point != aim_point && ( z.type->size <= creature_size::small ||
                                                           z.is_hallucination() ||
                                                           z.type->in_species( HALLUCINATION ) ) ) {
                        continue;
                    }
                    if( !aim_bounds.is_point_inside( trajectory_point ) ) {
                        // take a photo of the monster that's in the way
                        p->add_msg_if_player( m_warning, _( "A %s got in the way of your photo." ), z.name() );
                        incorrect_focus = true;
                    } else if( trajectory_point != aim_point ) { // shoot past mon that will be in photo anyway
                        continue;
                    }
                    // get an special message if the target is a hallucination
                    if( trajectory_point == aim_point && ( z.is_hallucination() ||
                                                           z.type->in_species( HALLUCINATION ) ) ) {
                        p->add_msg_if_player( _( "Strange… there's nothing in the center of picture?" ) );
                    }
                } else if( guy ) {
                    if( trajectory_point == aim_point && guy->is_hallucination() ) {
                        p->add_msg_if_player( _( "Strange… %s's not visible on the picture?" ), guy->name );
                    } else if( !aim_bounds.is_point_inside( trajectory_point ) ) {
                        // take a photo of the monster that's in the way
                        p->add_msg_if_player( m_warning, _( "%s got in the way of your photo." ), guy->name );
                        incorrect_focus = true;
                    } else if( trajectory_point != aim_point ) {  // shoot past guy that will be in photo anyway
                        continue;
                    }
                }
                if( incorrect_focus ) {
                    photo_quality = photo_quality == 0 ? 0 : photo_quality - 1;
                }

                std::vector<extended_photo_def> extended_photos;
                std::vector<monster *> monster_vec;
                std::vector<player *> player_vec;
                extended_photo_def photo = photo_def_for_camera_point( trajectory_point, p->pos(), monster_vec,
                                           player_vec );
                photo.quality = photo_quality;

                try {
                    item_read_extended_photos( *it, extended_photos, "CAMERA_EXTENDED_PHOTOS" );
                    extended_photos.push_back( photo );
                    item_write_extended_photos( *it, extended_photos, "CAMERA_EXTENDED_PHOTOS" );
                } catch( const JsonError &e ) {
                    debugmsg( "Error when adding new photo (loaded photos = %i): %s", extended_photos.size(),
                              e.c_str() );
                }

                const bool selfie = std::ranges::find( player_vec, p ) != player_vec.end();

                if( selfie ) {
                    p->add_msg_if_player( _( "You took a selfie." ) );
                } else {
                    if( p->is_blind() ) {
                        p->add_msg_if_player( _( "You took a photo of %s." ), photo.name );
                    } else {
                        p->add_msg_if_player( _( "You took a photo of %1$s. It is %2$s." ), photo.name,
                                              photo_quality_name( photo_quality ) );
                    }
                    std::vector<std::string> blinded_names;
                    for( monster * const &monster_p : monster_vec ) {
                        if( dist < 4 && one_in( dist + 2 ) && monster_p->has_flag( MF_SEES ) ) {
                            monster_p->add_effect( effect_blind, rng( 5_turns, 10_turns ) );
                            blinded_names.push_back( monster_p->name() );
                        }
                    }
                    for( player * const &player_p : player_vec ) {
                        if( dist < 4 && one_in( dist + 2 ) && !player_p->is_blind() ) {
                            player_p->add_effect( effect_blind, rng( 5_turns, 10_turns ) );
                            blinded_names.push_back( player_p->name );
                        }
                    }
                    if( !blinded_names.empty() ) {
                        p->add_msg_if_player( _( "%s looks blinded." ), enumerate_as_string( blinded_names.begin(),
                        blinded_names.end(), []( const std::string & it ) {
                            return colorize( it, c_light_blue );
                        } ) );
                    }
                }
                if( !monster_vec.empty() ) {
                    item_save_monsters( *p, *it, monster_vec, photo_quality );
                }
                return it->type->charges_to_use();
            }
        }
        return it->type->charges_to_use();
    }

    if( c_photos == choice ) {
        show_photo_selection( *p, *it, "CAMERA_EXTENDED_PHOTOS" );
        return it->type->charges_to_use();
    }

    if( c_monsters == choice ) {
        if( p->is_blind() ) {
            p->add_msg_if_player( _( "You can't see the camera screen, you're blind." ) );
            return 0;
        }
        uilist pmenu;

        pmenu.text = _( "Your collection of monsters:" );

        std::vector<mtype_id> monster_photos;
        std::vector<std::string> descriptions;

        std::istringstream f_mon( it->get_var( "CAMERA_MONSTER_PHOTOS" ) );
        std::string s;
        int k = 0;
        while( getline( f_mon, s, ',' ) ) {

            if( s.empty() ) {
                continue;
            }

            monster_photos.emplace_back( s );

            std::string menu_str;

            const monster dummy( monster_photos.back() );
            menu_str = dummy.name();
            descriptions.push_back( dummy.type->get_description() );

            getline( f_mon, s, ',' );
            char *chq = s.data();
            const int quality = atoi( chq );

            menu_str += " [" + photo_quality_name( quality ) + "]";

            pmenu.addentry( k++, true, -1, menu_str.c_str() );
        }

        int choice;
        do {
            pmenu.query();
            choice = pmenu.ret;

            if( choice < 0 ) {
                break;
            }

            popup( "%s", descriptions[choice].c_str() );

        } while( true );

        return it->type->charges_to_use();
    }

    if( c_upload == choice ) {

        if( p->is_blind() ) {
            p->add_msg_if_player( _( "You can't see the camera screen, you're blind." ) );
            return 0;
        }

        p->moves -= to_moves<int>( 2_seconds );

        avatar *you = p->as_avatar();
        item *loc = nullptr;
        if( you != nullptr ) {
            loc = game_menus::inv::titled_filter_menu( []( const item & it ) {
                return it.has_flag( flag_MC_MOBILE );
            }, *you, _( "Insert memory card" ) );
        }
        if( !loc ) {
            p->add_msg_if_player( m_info, _( "You do not have that item!" ) );
            return it->type->charges_to_use();
        }
        item &mc = *loc;

        if( !mc.has_flag( flag_MC_MOBILE ) ) {
            p->add_msg_if_player( m_info, _( "This is not a compatible memory card." ) );
            return it->type->charges_to_use();
        }

        init_memory_card_with_random_stuff( mc );

        if( mc.has_flag( flag_MC_ENCRYPTED ) ) {
            if( !query_yn( _( "This memory card is encrypted.  Format and clear data?" ) ) ) {
                return it->type->charges_to_use();
            }
        }
        if( mc.has_flag( flag_MC_HAS_DATA ) ) {
            if( !query_yn( _( "Are you sure you want to clear the old data on the card?" ) ) ) {
                return it->type->charges_to_use();
            }
        }

        mc.convert( itype_mobile_memory_card );
        mc.clear_vars();
        mc.unset_flags();
        mc.set_flag( flag_MC_HAS_DATA );

        mc.set_var( "MC_MONSTER_PHOTOS", it->get_var( "CAMERA_MONSTER_PHOTOS" ) );
        mc.set_var( "MC_EXTENDED_PHOTOS", it->get_var( "CAMERA_EXTENDED_PHOTOS" ) );
        p->add_msg_if_player( m_info,
                              _( "You upload your photos and monster collection to memory card." ) );

        return it->type->charges_to_use();
    }

    return it->type->charges_to_use();
}

int iuse::ehandcuffs( player *p, item *it, bool t, const tripoint &pos )
{

    if( t ) {

        if( g->m.has_flag( "SWIMMABLE", pos.xy() ) ) {
            it->unset_flag( flag_NO_UNWIELD );
            it->ammo_unset();
            it->deactivate();
            add_msg( m_good, _( "%s automatically turned off!" ), it->tname() );
            return it->type->charges_to_use();
        }

        if( it->charges == 0 ) {

            sounds::sound( pos, 2, sounds::sound_t::combat, "Click.", true, "tools", "handcuffs" );
            it->unset_flag( flag_NO_UNWIELD );
            it->deactivate();

            if( p->has_item( *it ) && p->primary_weapon().typeId() == itype_e_handcuffs ) {
                add_msg( m_good, _( "%s on your hands opened!" ), it->tname() );
            }

            return it->type->charges_to_use();
        }

        if( p->has_item( *it ) ) {
            if( p->has_active_bionic( bio_shock ) && p->get_power_level() >= bio_shock->power_trigger &&
                one_in( 5 ) ) {
                p->mod_power_level( -bio_shock->power_trigger );

                it->unset_flag( flag_NO_UNWIELD );
                it->ammo_unset();
                it->deactivate();
                add_msg( m_good, _( "The %s crackle with electricity from your bionic, then come off your hands!" ),
                         it->tname() );

                return it->type->charges_to_use();
            }
        }

        if( calendar::once_every( 1_minutes ) ) {
            sounds::sound( pos, 10, sounds::sound_t::alarm, _( "a police siren, whoop WHOOP." ), true,
                           "environment", "police_siren" );
        }

        const point p2( it->get_var( "HANDCUFFS_X", 0 ), it->get_var( "HANDCUFFS_Y", 0 ) );

        if( ( it->ammo_remaining() > it->type->maximum_charges() - 1000 ) && ( p2.x != pos.x ||
                p2.y != pos.y ) ) {
            if( p->has_item( *it ) && p->primary_weapon().typeId() == itype_e_handcuffs ) {
                if( p->is_elec_immune() ) {
                    if( one_in( 10 ) ) {
                        add_msg( m_good, _( "The cuffs try to shock you, but you're protected from electricity." ) );
                    }
                } else {
                    add_msg( m_bad, _( "Ouch, the cuffs shock you!" ) );

                    p->apply_damage( nullptr, bodypart_id( "arm_l" ), rng( 0, 2 ) );
                    p->apply_damage( nullptr, bodypart_id( "arm_r" ), rng( 0, 2 ) );
                    p->mod_pain( rng( 2, 5 ) );

                }

            } else {
                add_msg( m_bad, _( "The %s spark with electricity!" ), it->tname() );
            }

            it->charges -= 50;
            if( it->charges < 1 ) {
                it->charges = 1;
            }

            it->set_var( "HANDCUFFS_X", pos.x );
            it->set_var( "HANDCUFFS_Y", pos.y );

            return it->type->charges_to_use();

        }

        return it->type->charges_to_use();

    }

    if( it->is_active() ) {
        add_msg( _( "The %s are clamped tightly on your wrists.  You can't take them off." ),
                 it->tname() );
    } else {
        add_msg( _( "The %s have discharged and can be taken off." ), it->tname() );
    }

    return it->type->charges_to_use();
}

int iuse::foodperson( player *p, item *it, bool t, const tripoint &pos )
{
    if( t ) {
        if( calendar::once_every( 1_minutes ) ) {
            const SpeechBubble &speech = get_speech( "foodperson_mask" );
            sounds::sound( pos, speech.volume, sounds::sound_t::alarm, speech.text.translated(), true, "speech",
                           "foodperson_mask" );
        }
        return it->type->charges_to_use();
    }

    time_duration shift = time_duration::from_turns( it->magazine_current()->ammo_remaining() *
                          it->type->tool->turns_per_charge - it->type->tool->turns_active );

    p->add_msg_if_player( m_info, _( "Your HUD lights-up: \"Your shift ends in %s\"." ),
                          to_string( shift ) );
    return 0;
}

int iuse::radiocar( player *p, item *it, bool, const tripoint & )
{
    int choice = -1;
    item *bomb_it = it->contents.get_item_with( []( const item & c ) {
        return c.has_flag( flag_RADIOCARITEM );
    } );
    if( bomb_it == nullptr ) {
        choice = uilist( _( "Using RC car:" ), {
            _( "Turn on" ), _( "Put a bomb to car" )
        } );
    } else {
        choice = uilist( _( "Using RC car:" ), {
            _( "Turn on" ), bomb_it->tname()
        } );
    }
    if( choice < 0 ) {
        return 0;
    }

    if( choice == 0 ) { //Turn car ON
        if( !it->ammo_sufficient() ) {
            p->add_msg_if_player( _( "The RC car's batteries seem to be dead." ) );
            return 0;
        }

        it->convert( itype_radio_car_on );
        it->activate();

        p->add_msg_if_player(
            _( "You turned on your RC car, now place it on ground, and use radio control to play." ) );

        return 0;
    }

    if( choice == 1 ) {

        if( bomb_it == nullptr ) { //arming car with bomb

            avatar *you = p->as_avatar();
            item *loc = nullptr;
            if( you != nullptr ) {
                loc = game_menus::inv::titled_filter_menu( []( const item & it ) {
                    return it.has_flag( flag_RADIOCARITEM );
                }, *you, _( "Arm what?" ) );
            }
            if( !loc ) {
                p->add_msg_if_player( m_info, _( "You do not have that item!" ) );
                return 0;
            }
            item &put = *loc;

            if( put.has_flag( flag_RADIOCARITEM ) && ( put.volume() <= 1250_ml ||
                    ( put.weight() <= 2_kilogram ) ) ) {
                p->moves -= to_moves<int>( 3_seconds );
                p->add_msg_if_player( _( "You armed your RC car with %s." ),
                                      put.tname() );
                it->put_in( put.detach( ) );
            } else if( !put.has_flag( flag_RADIOCARITEM ) ) {
                p->add_msg_if_player( _( "RC car with %s?  How?" ),
                                      put.tname() );
            } else {
                p->add_msg_if_player( _( "Your %s is too heavy or bulky for this RC car." ),
                                      put.tname() );
            }
        } else { // Disarm the car
            p->moves -= to_moves<int>( 2_seconds );

            p->inv_assign_empty_invlet( *bomb_it, true ); // force getting an invlet.
            p->i_add( it->remove_item( *bomb_it ) );

            p->add_msg_if_player( _( "You disarmed your RC car." ) );
        }
    }

    return it->type->charges_to_use();
}

int iuse::radiocaron( player *p, item *it, bool t, const tripoint &pos )
{
    if( t ) {
        //~Sound of a radio controlled car moving around
        sounds::sound( pos, 6, sounds::sound_t::movement, _( "buzzz…" ), true, "misc", "rc_car_drives" );

        return it->type->charges_to_use();
    } else if( !it->ammo_sufficient() ) {
        // Deactivate since other mode has an iuse too.
        it->deactivate();
        return 0;
    }

    int choice = uilist( _( "What to do with activated RC car?" ), {
        _( "Turn off" )
    } );

    if( choice < 0 ) {
        return it->type->charges_to_use();
    }

    if( choice == 0 ) {
        it->convert( itype_radio_car );
        it->deactivate();

        p->add_msg_if_player( _( "You turned off your RC car." ) );
        return it->type->charges_to_use();
    }

    return it->type->charges_to_use();
}

/**
 * Send radio signal from player.
 */
static void emit_radio_signal( player &p, const flag_id &signal )
{
    const auto visitor = [&]( item & it, const tripoint & loc ) -> VisitResponse {
        if( it.has_flag( flag_RADIO_ACTIVATION ) && it.has_flag( signal ) )
        {
            sounds::sound( p.pos(), 6, sounds::sound_t::alarm, _( "beep" ), true, "misc", "beep" );
            bool invoke_proc = it.has_flag( flag_RADIO_INVOKE_PROC );
            // Invoke to transform item
            it.type->invoke( p, it, loc );
            if( invoke_proc ) {
                // Cause invocation of transformed item on next turn processing
                it.ammo_unset();
            }
        }
        return VisitResponse::NEXT;
    };

    int z_min = g->m.has_zlevels() ? -OVERMAP_DEPTH : 0;
    int z_max = g->m.has_zlevels() ? OVERMAP_HEIGHT : 0;
    for( int zlev = z_min; zlev <= z_max; zlev++ ) {
        for( tripoint loc : g->m.points_on_zlevel( zlev ) ) {
            // Items on ground
            map_cursor mc( loc );
            mc.visit_items( [&]( item * it ) {
                return visitor( *it, loc );
            } );

            // Items in vehicles
            optional_vpart_position vp = g->m.veh_at( loc );
            if( !vp ) {
                continue;
            }
            std::optional<vpart_reference> vpr = vp.part_with_feature( "CARGO", false );
            if( !vpr ) {
                continue;
            }
            vehicle_cursor vc( vp->vehicle(), vpr->part_index() );
            vc.visit_items( [&]( item * it ) {
                return visitor( *it, loc );
            } );
        }
    }

    // Items on creatures
    for( Creature &cr : g->all_creatures() ) {
        const tripoint &cr_pos = cr.pos();
        if( cr.is_monster() ) {
            monster &mon = *cr.as_monster();
            mon.visit_items( [&]( item * it ) {
                return visitor( *it, cr_pos );
            } );
        } else {
            Character &ch = *cr.as_character();
            ch.visit_items( [&]( item * it ) {
                return visitor( *it, cr_pos );
            } );
        }
    }
}

int iuse::radiocontrol( player *p, item *it, bool t, const tripoint & )
{
    if( t ) {
        if( !it->units_sufficient( *p ) ) {
            it->deactivate();
            p->remove_value( "remote_controlling" );
        } else if( p->get_value( "remote_controlling" ).empty() ) {
            it->deactivate();
        }

        return it->type->charges_to_use();
    }

    const char *car_action = nullptr;

    if( !it->is_active() ) {
        car_action = _( "Take control of RC car" );
    } else {
        car_action = _( "Stop controlling RC car" );
    }

    int choice = uilist( _( "What to do with radio control?" ), {
        car_action,
        _( "Press red button" ), _( "Press blue button" ), _( "Press green button" )
    } );

    if( choice < 0 ) {
        return 0;
    } else if( choice == 0 ) {
        if( it->is_active() ) {
            it->deactivate();
            p->remove_value( "remote_controlling" );
        } else {
            std::vector<std::pair<tripoint, item *>> rc_pairs;
            for( tripoint pt : g->m.points_on_zlevel( p->posz() ) ) {
                map_cursor mc( pt );
                std::vector<item *> rc_items_here = mc.items_with( [&]( const item & it ) {
                    return it.has_flag( flag_RADIO_CONTROLLED );
                } );
                for( item *it : rc_items_here ) {
                    rc_pairs.emplace_back( pt, it );
                }
            }

            if( rc_pairs.empty() ) {
                p->add_msg_if_player( _( "No active RC cars on ground and in range." ) );
                return it->type->charges_to_use();
            }

            std::vector<tripoint> locations;
            uilist pick_rc;
            pick_rc.text = _( "Choose car to control." );
            for( size_t i = 0; i < rc_pairs.size(); i++ ) {
                pick_rc.addentry( i, true, MENU_AUTOASSIGN, rc_pairs[i].second->display_name() );
                locations.push_back( rc_pairs[i].first );
            }
            pointmenu_cb callback( locations );
            pick_rc.callback = &callback;
            pick_rc.query();
            if( pick_rc.ret < 0 || static_cast<size_t>( pick_rc.ret ) >= rc_pairs.size() ) {
                p->add_msg_if_player( m_info, _( "Never mind." ) );
                return it->type->charges_to_use();
            }

            tripoint rc_loc = locations[pick_rc.ret];

            p->add_msg_if_player( m_good, _( "You take control of the RC car." ) );
            p->set_value( "remote_controlling", serialize_wrapper( [&]( JsonOut & jo ) {
                rc_loc.serialize( jo );
            } ) );
            it->activate();
        }
    } else if( choice > 0 ) {
        const flag_id signal( "RADIOSIGNAL_" + std::to_string( choice ) );

        std::vector<item *> bombs = p->items_with( [&]( const item & it ) -> bool {
            return it.has_flag( flag_RADIO_ACTIVATION ) && it.has_flag( flag_BOMB ) && it.has_flag( signal );
        } );

        if( !bombs.empty() ) {
            p->add_msg_if_player( m_warning,
                                  _( "The %s in your inventory would explode on this signal.  Place it down before sending the signal." ),
                                  bombs.front()->display_name() );
            return 0;
        }

        p->add_msg_if_player( _( "Click." ) );
        emit_radio_signal( *p, signal );
        p->moves -= to_moves<int>( 2_seconds );
    }

    return it->type->charges_to_use();
}

static bool hackveh( player &p, item &it, vehicle &veh )
{
    if( !veh.is_locked || !veh.has_security_working() ) {
        return true;
    }
    const bool advanced = !empty( veh.get_avail_parts( "REMOTE_CONTROLS" ) );
    if( advanced && veh.is_alarm_on ) {
        p.add_msg_if_player( m_bad, _( "This vehicle's security system has locked you out!" ) );
        return false;
    }

    /** @EFFECT_INT increases chance of bypassing vehicle security system */

    /** @EFFECT_COMPUTER increases chance of bypassing vehicle security system */
    int roll = dice( p.get_skill_level( skill_computer ) + 2, p.int_cur ) - ( advanced ? 50 : 25 );
    int effort = 0;
    bool success = false;
    if( roll < -20 ) { // Really bad rolls will trigger the alarm before you know it exists
        effort = 1;
        p.add_msg_if_player( m_bad, _( "You trigger the alarm!" ) );
        veh.is_alarm_on = true;
    } else if( roll >= 20 ) { // Don't bother the player if it's trivial
        effort = 1;
        p.add_msg_if_player( m_good, _( "You quickly bypass the security system!" ) );
        success = true;
    }

    if( effort == 0 && !query_yn( _( "Try to hack this car's security system?" ) ) ) {
        // Scanning for security systems isn't free
        p.moves -= to_moves<int>( 1_seconds );
        it.charges -= 1;
        return false;
    }

    p.practice( skill_computer, advanced ? 10 : 3 );
    if( roll < -10 ) {
        effort = rng( 4, 8 );
        p.add_msg_if_player( m_bad, _( "You waste some time, but fail to affect the security system." ) );
    } else if( roll < 0 ) {
        effort = 1;
        p.add_msg_if_player( m_bad, _( "You fail to affect the security system." ) );
    } else if( roll < 20 ) {
        effort = rng( 2, 8 );
        p.add_msg_if_player( m_mixed,
                             _( "You take some time, but manage to bypass the security system!" ) );
        success = true;
    }

    p.moves -= to_moves<int>( time_duration::from_seconds( effort ) );
    it.charges -= effort;
    if( success && advanced ) { // Unlock controls, but only if they're drive-by-wire
        veh.is_locked = false;
    }
    return success;
}

static vehicle *pickveh( const tripoint &center, bool advanced )
{
    static const std::string ctrl = "CTRL_ELECTRONIC";
    static const std::string advctrl = "REMOTE_CONTROLS";
    uilist pmenu;
    pmenu.title = _( "Select vehicle to access" );
    std::vector< vehicle * > vehs;

    for( auto &veh : g->m.get_vehicles() ) {
        auto &v = veh.v;
        if( rl_dist( center, v->global_pos3() ) < 40 &&
            v->fuel_left( itype_battery, true ) > 0 &&
            ( !empty( v->get_avail_parts( advctrl ) ) ||
              ( !advanced && !empty( v->get_avail_parts( ctrl ) ) ) ) ) {
            vehs.push_back( v );
        }
    }
    std::vector<tripoint> locations;
    for( int i = 0; i < static_cast<int>( vehs.size() ); i++ ) {
        auto veh = vehs[i];
        locations.push_back( veh->global_pos3() );
        pmenu.addentry( i, true, MENU_AUTOASSIGN, veh->name );
    }

    if( vehs.empty() ) {
        add_msg( m_bad, _( "No vehicle available." ) );
        return nullptr;
    }

    pointmenu_cb callback( locations );
    pmenu.callback = &callback;
    pmenu.w_y_setup = 0;
    pmenu.query();

    if( pmenu.ret < 0 || pmenu.ret >= static_cast<int>( vehs.size() ) ) {
        return nullptr;
    } else {
        return vehs[pmenu.ret];
    }
}

int iuse::remoteveh( player *p, item *it, bool t, const tripoint &pos )
{
    vehicle *remote = g->remoteveh();
    if( t ) {
        bool stop = false;
        if( !it->units_sufficient( *p ) ) {
            p->add_msg_if_player( m_bad, _( "The remote control's battery goes dead." ) );
            stop = true;
        } else if( remote == nullptr ) {
            p->add_msg_if_player( _( "Lost contact with the vehicle." ) );
            stop = true;
        } else if( remote->fuel_left( itype_battery, true ) == 0 ) {
            p->add_msg_if_player( m_bad, _( "The vehicle's battery died." ) );
            stop = true;
        }
        if( stop ) {
            it->deactivate();
            g->setremoteveh( nullptr );
        }

        return it->type->charges_to_use();
    }

    bool controlling = it->is_active() && remote != nullptr;
    int choice = uilist( _( "What to do with remote vehicle control:" ), {
        controlling ? _( "Stop controlling the vehicle." ) : _( "Take control of a vehicle." ),
        _( "Execute one vehicle action" )
    } );

    if( choice < 0 || choice > 1 ) {
        return 0;
    }

    if( choice == 0 && controlling ) {
        it->deactivate();
        g->setremoteveh( nullptr );
        return 0;
    }

    point p2( g->u.view_offset.xy() );

    vehicle *veh = pickveh( pos, choice == 0 );

    if( veh == nullptr ) {
        return 0;
    }

    if( !hackveh( *p, *it, *veh ) ) {
        return 0;
    }

    if( choice == 0 ) {
        if( g->u.has_trait( trait_WAYFARER ) ) {
            add_msg( m_info,
                     _( "Despite using a controller, you still refuse to take control of this vehicle." ) );
        } else {
            it->activate();
            g->setremoteveh( veh );
            p->add_msg_if_player( m_good, _( "You take control of the vehicle." ) );
            if( !veh->engine_on ) {
                veh->start_engines();
            }
        }
    } else if( choice == 1 ) {
        const auto rctrl_parts = veh->get_avail_parts( "REMOTE_CONTROLS" );
        // Revert to original behavior if we can't find remote controls.
        if( empty( rctrl_parts ) ) {
            veh->use_controls( pos );
        } else {
            veh->use_controls( rctrl_parts.begin()->pos() );
        }
    }

    g->u.view_offset.x = p2.x;
    g->u.view_offset.y = p2.y;
    return it->type->charges_to_use();
}

int iuse::autoclave( player *p, item *, bool, const tripoint &pos )
{
    iexamine::autoclave_empty( *p, pos );
    return 0;
}

static auto confirm_source_vehicle( const tripoint_abs_ms &global )
{
    optional_vpart_position source_vp = g->m.veh_at( global );
    vehicle *const source_veh = veh_pointer_or_null( source_vp );
    return std::make_tuple( source_vp, source_veh );
};

static tripoint_abs_ms process_map_connection( const Character *who, cable_state state,
        bool tow = false )
{
    const std::optional<tripoint> posp_ = choose_adjacent( _( "Attach cable where?" ) );
    if( !posp_ ) {
        return tripoint_abs_ms_min;
    }
    map &here = get_map();
    const auto posp = here.getglobal( *posp_ );

    switch( state ) {
        case state_vehicle: {
            const optional_vpart_position vp = here.veh_at( posp );
            if( !vp ) {
                who->add_msg_if_player( _( "There's no vehicle there." ) );
                if( !tow ) {
                    return tripoint_abs_ms_min;
                } else {
                    vehicle *const source_veh = veh_pointer_or_null( vp );
                    if( source_veh ) {
                        if( source_veh->has_tow_attached() || source_veh->is_towed() ||
                            source_veh->is_towing() ) {
                            who->add_msg_if_player( _( "That vehicle already has a tow-line attached." ) );
                            return tripoint_abs_ms_min;
                        }
                        if( !source_veh->is_external_part( *posp_ ) ) {
                            who->add_msg_if_player( _( "You can't attach the tow-line to an internal part." ) );
                            return tripoint_abs_ms_min;
                        }
                    }
                }
            }
            break;
        }
        case state_grid: {
            auto *grid_connector = active_tiles::furn_at<vehicle_connector_tile>( posp );
            if( !grid_connector ) {
                who->add_msg_if_player( _( "There's no grid connector there." ) );
                return tripoint_abs_ms_min;
            }
            break;
        }
        default:
            return tripoint_abs_ms_min;
    }
    return posp;
}

static cable_state cable_menu( Character *who, cable_state &state, cable_state &state_other )
{
    const bool has_bio_cable = !who->get_remote_fueled_bionic().is_empty();
    // const bool has_solar_pack = who->worn_with_flag( flag_SOLARPACK );
    const bool has_solar_pack_on = who->worn_with_flag( flag_SOLARPACK_ON );
    //const bool wearing_solar_pack = has_solar_pack || has_solar_pack_on;
    const bool has_ups = who->has_charges( itype_UPS, 1 );

    const bool allow_self = state != state_self && state_other != state_self;
    const bool allow_ups =  state_other == state_self ||
                            ( state == state_none && state_other == state_none );
    const bool allow_grid = state_other != state_UPS && state_other != state_solar_pack;

    // Currently those bools equal provided counterparts, feel free to change those if it's needed in future
    const bool allow_solar = allow_ups;
    const bool allow_vehicle = allow_grid;

    uilist kmenu;
    kmenu.text = _( "Using cable:" );
    if( state != state_none || state_other != state_none ) {
        kmenu.addentry( state_none, true, -1,
                        _( "Detach and re-spool the cable" ) );
    }
    kmenu.addentry( state_self, has_bio_cable && allow_self, -1, _( "Attach cable to self" ) );
    kmenu.addentry( state_vehicle, allow_vehicle, -1, _( "Attach cable to vehicle" ) );
    kmenu.addentry( state_grid, allow_grid, -1, _( "Attach cable to grid connector" ) );
    kmenu.addentry( state_UPS, has_ups && allow_ups, -1, _( "Attach cable to ups" ) );
    kmenu.addentry( state_solar_pack, has_solar_pack_on && allow_solar,
                    -1, _( "Attach cable to solar pack" ) );

    kmenu.query();
    return cable_state( kmenu.ret );
}


static cable_state tow_cable_menu( cable_state &state, cable_state &state_other, bool towed )
{
    uilist kmenu;
    kmenu.text = towed
                 ? _( "Using cable tow cable. Attached vehicle will be towed:" )
                 : _( "Using cable tow cable. Attached vehicle will tow the other:" );
    if( state != state_none || state_other != state_none ) {
        kmenu.addentry( state_none, true, -1, _( "Detach and re-spool the cable" ) );
    }
    kmenu.addentry( state_vehicle, true, -1, _( "Attach loose end to vehicle" ) );

    kmenu.query();
    return cable_state( kmenu.ret );
}

static void set_cable_active( player *const who, item *const it,
                              const cable_connection_data &data )
{
    data.set_vars( it );
    it->activate();
    it->attempt_detach( [&who]( detached_ptr<item> &&e ) {
        return item::process( std::move( e ), who, who->pos(), false );
    } );
    who->mod_moves( -15 );
};

int iuse::tow_attach( player *who, item *cable, bool, const tripoint & )
{
    if( !who ) {
        return 0;
    }
    auto data = cable_connection_data::make_data( cable );
    if( !data ) {
        return 0;
    }
    cable_connection_data::connection *last = nullptr;

    if( data->con1.empty() ) {
        last = &data->con1;
        switch( data->con1.state = tow_cable_menu( data->con1.state, data->con2.state, false ) ) {
            case state_none:
                cable->reset_cable( who );
                return 0;
            case state_vehicle:
                data->con1.point = process_map_connection( who, state_vehicle, true );
                if( data->con1.point_valid() ) {
                    set_cable_active( who, cable, data.value() );
                }
                break;
            default:
                add_msg( _( "Never mind" ) );
                return 0;
        }
    } else if( data->con2.empty() ) {
        last = &data->con2;
        switch( data->con2.state = tow_cable_menu( data->con2.state, data->con1.state, true ) ) {
            case state_none:
                cable->reset_cable( who );
                return 0;
            case state_vehicle:
                data->con2.point = process_map_connection( who, state_vehicle, true );
                if( data->con2.point_valid() ) {
                    set_cable_active( who, cable, data.value() );
                }
                break;
            default:
                add_msg( _( "Never mind" ) );
                return 0;
        }
    }
    if( data->intermap_connection() ) {
        const auto [vp1, v1] = confirm_source_vehicle( data->con1.point );
        const auto [vp2, v2] = confirm_source_vehicle( data->con2.point );

        if( !vp1 || !vp2 ) {
            debugmsg( "Something went wrong with cable connection" );
            who->add_msg_if_player( m_bad, _( "You notice the cable has come loose!" ) );
            cable->reset_cable( who );
            return 0;
        }

        if( v1 == v2 ) {
            who->add_msg_if_player( m_warning, _( "You cannot set a vehicle to tow itself!" ) );
            if( last ) {
                data->unset_con( cable, *last );
            } else {
                cable->reset_cable( who );
            }
            return 0;
        }
        const vpart_id vpid( cable->typeId().str() );

        point vcoords = vp1->mount();
        vehicle_part v1_part( vpid, vcoords, item::spawn( *cable ), v1 );
        v1->install_part( vcoords, std::move( v1_part ) );
        vcoords = vp2->mount();
        vehicle_part v2_part( vpid, vcoords, item::spawn( *cable ), v2 );
        v2->install_part( vcoords, std::move( v2_part ) );

        if( who->has_item( *cable ) ) {
            who->add_msg_if_player( m_good, _( "You link up the %1$s and the %2$s." ),
                                    v1->name, v2->name );
        }
        v1->tow_data.set_towing( v1, v2 );
        return 1; // Let the cable be destroyed.
    }
    return 0;
}

int iuse::cable_attach( player *who, item *cable, bool, const tripoint & )
{
    item *ups_loc = nullptr;
    avatar *you = who->as_avatar();
    const std::string choose_ups = _( "Choose UPS:" );
    const std::string dont_have_ups = _( "You don't have any UPS." );
    auto filter = [&]( const item & itm ) {
        return itm.has_flag( flag_IS_UPS );
    };

    auto data = cable_connection_data::make_data( cable );
    if( !data ) {
        return 0;
    }

    cable_connection_data::connection *con = nullptr;
    cable_connection_data::connection *con_other = nullptr;

    if( data->con1.empty() ) {
        con = &data->con1;
        con_other = &data->con2;
    } else if( data->con2.empty() ) {
        con = &data->con2;
        con_other = &data->con1;
    }

    if( con && con->empty() ) {
        con->state = cable_menu( who, con->state, con_other->state );
        switch( con->state ) {
            case state_self:
                who->add_msg_if_player( m_info, _( "You attach the cable to the Cable Charger System." ) );
                break;
            case state_none:
                cable->reset_cable( who );
                return 0;
            case state_solar_pack:
                who->add_msg_if_player( m_info, _( "You attach the cable to the solar pack." ) );
                break;
            case state_grid: {
                con->point = process_map_connection( who, con->state );
                if( !con->point_valid() ) {
                    return 0;
                }
                //Basically we allow player to try and use grid to grid connection, only TO give them some insight
                if( con_other->state == state_grid ) {
                    if( con->point == con_other->point ) {
                        who->add_msg_if_player( m_info, _( "That would be unwise to short-circuit this grid connector." ) );
                    } else {
                        who->add_msg_if_player( m_info,
                                                _( "To directly connect two networks together, use a voltmeter instead." ) );
                    }
                    return 0;
                }
                break;
            }
            case state_UPS:
                if( you ) {
                    ups_loc = game_menus::inv::titled_filter_menu( filter, *you, choose_ups, dont_have_ups );
                }
                if( !ups_loc ) {
                    add_msg( _( "Never mind" ) );
                    return 0;
                }
                ups_loc->set_var( "cable", "plugged_in" );
                ups_loc->activate();
                who->add_msg_if_player( m_info, _( "You attach the cable to the UPS." ) );
                break;
            case state_vehicle:
                con->point = process_map_connection( who, con->state );
                if( !con->point_valid() ) {
                    return 0;
                }
                break;
            default:
                return 0;
        }
        set_cable_active( who, cable, data.value() );
    }
    // Both ends are currently connected, respool or do nothing
    else {
        uilist kmenu;
        kmenu.text = _( "Using cable:" );
        kmenu.addentry( state_none, true, -1,
                        _( "Detach and re-spool the cable" ) );
        kmenu.query();

        if( kmenu.ret == state_none ) {
            cable->reset_cable( who );
        } else {
            you->add_msg_if_player( m_info, _( "Never mind." ) );
        }
        return 0;
    }

    //Two connections are made, let's process result
    if( data->complete() ) {
        //We've connected something to Character
        if( data->character_connected() ) {
            auto *const nonchar = data->get_nonchar_connection();
            switch( nonchar->state ) {
                case state_grid:
                    who->add_msg_if_player( m_good, _( "You are now plugged to the grid." ) );
                    break;
                case state_solar_pack:
                    who->add_msg_if_player( m_good, _( "You are now plugged to the solar backpack." ) );
                    break;
                case state_UPS:
                    who->add_msg_if_player( m_good, _( "You are now plugged to the UPS." ) );
                    break;
                case state_vehicle: {
                    const auto [_, veh] = confirm_source_vehicle( nonchar->point );
                    if( veh ) {
                        who->add_msg_if_player( m_good, _( "You are now plugged to the vehicle." ) );
                    } else {
                        who->add_msg_if_player( m_bad, _( "You notice the cable has come loose!" ) );
                        cable->reset_cable( who );
                        return 0;
                    }
                    break;
                }
                case state_none:
                    //How tf?
                    cable->reset_cable( who );
                    debugmsg( "Unexpected cable state %s", nonchar->state );
                    break;
                default:
                    add_msg( _( "Never mind" ) );
                    return 0;
            };
            who->find_remote_fuel();
            return 0;
        }
        //We've connected two vehicles
        if( data->con1.state == state_vehicle && data->con2.state == state_vehicle ) {
            const auto [vp1, v1] = confirm_source_vehicle( data->con1.point );
            const auto [vp2, v2] = confirm_source_vehicle( data->con2.point );

            if( !vp1 || !vp2 ) {
                debugmsg( "Something went wrong with cable connection" );
                who->add_msg_if_player( m_bad, _( "You notice the cable has come loose!" ) );
                cable->reset_cable( who );
                return 0;
            }

            if( v1 == v2 ) {
                who->add_msg_if_player( m_warning, _( "The %s already has access to its own electric system!" ),
                                        v1->name );
                cable_connection_data::unset_con2( cable );
                return 0;
            }
            const vpart_id vpid( cable->typeId().str() );

            //Vehicle part1
            point vcoords = vp1->mount();
            vehicle_part p1( vpid, vcoords, item::spawn( *cable ), v1 );
            p1.target.first = data->con2.point.raw();
            p1.target.second = v2->global_square_location().raw();
            v1->install_part( vcoords, std::move( p1 ) );

            //Vehicle part2
            vcoords = vp2->mount();
            vehicle_part p2( vpid, vcoords, item::spawn( *cable ), v2 );
            p2.target.first = data->con1.point.raw();
            p2.target.second = v1->global_square_location().raw();
            v2->install_part( vcoords, std::move( p2 ) );

            if( who != nullptr && who->has_item( *cable ) ) {
                who->add_msg_if_player( m_good, _( "You link up the electric systems of the %1$s and the %2$s." ),
                                        v1->name, v2->name );
            }
            return 1;    // Let the cable be destroyed.
        }
        //We've connected vehicle to grid
        else if( data->con1.state == state_vehicle || data->con2.state == state_vehicle ) {
            auto [vp1, v1] = confirm_source_vehicle( data->con1.point );
            auto [vp2, v2] = confirm_source_vehicle( data->con2.point );

            vehicle *v = nullptr;
            optional_vpart_position vp( std::nullopt );
            tripoint_abs_ms v_global;
            tripoint_abs_ms connector;

            if( v1 ) {
                v = v1;
                vp = vp1;
                v_global = data->con1.point;
                connector = data->con2.point;
            } else if( v2 ) {
                v = v2;
                vp = vp2;
                v_global = data->con2.point;
                connector = data->con1.point;
            } else {
                debugmsg( "Something went wrong with cable connection" );
                who->add_msg_if_player( m_bad, _( "You notice the cable has come loose!" ) );
                cable->reset_cable( who );
                return 0;
            }

            auto *grid_connector = active_tiles::furn_at<vehicle_connector_tile>( connector );
            if( !grid_connector ) {
                who->add_msg_if_player( _( "There's no grid connection there." ) );
                cable->reset_cable( who );
                return 0;
            }

            const vpart_id vpid( cable->typeId().str() );
            point vcoords = vp->mount();
            vehicle_part v_part( vpid, vcoords, item::spawn( *cable ), v );
            v_part.target.first = connector.raw();
            v_part.target.second = connector.raw();
            v_part.set_flag( vehicle_part::targets_grid );
            if( who && who->has_item( *cable ) ) {
                who->add_msg_if_player( m_good, _( "You connect the %s to the electric grid." ),
                                        v->name );
                grid_connector->connected_vehicles.emplace_back( g->m.getabs( v->global_pos3() ) );
                v->install_part( vcoords, std::move( v_part ) );
            }
            return 1;    // Let the cable be destroyed.
        }
    }
    return 0;
}

int iuse::shavekit( player *p, item *it, bool, const tripoint & )
{
    if( p->is_mounted() ) {
        p->add_msg_if_player( m_info, _( "You cannot do that while mounted." ) );
        return 0;
    }
    if( !it->ammo_sufficient() ) {
        p->add_msg_if_player( _( "You need soap to use this." ) );
    } else {
        const int moves = to_moves<int>( 5_minutes );
        p->assign_activity( ACT_SHAVE, moves );
    }
    return it->type->charges_to_use();
}

int iuse::hairkit( player *p, item *it, bool, const tripoint & )
{
    if( p->is_mounted() ) {
        p->add_msg_if_player( m_info, _( "You cannot do that while mounted." ) );
        return 0;
    }
    const int moves = to_moves<int>( 30_minutes );
    p->assign_activity( ACT_HAIRCUT, moves );
    return it->type->charges_to_use();
}

int iuse::weather_tool( player *p, item *it, bool, const tripoint & )
{
    const weather_manager &weather = get_weather();
    const w_point &weatherPoint = get_weather().get_precise();

    /* Possibly used twice. Worth spending the time to precalculate. */
    const auto player_local_temp = weather.get_temperature( p->pos() );

    map &here = get_map();
    if( it->typeId() == itype_weather_reader ) {
        p->add_msg_if_player( m_neutral, _( "The %s's monitor slowly outputs the data…" ),
                              it->tname() );
    }
    if( it->has_flag( flag_THERMOMETER ) ) {
        if( it->typeId() == itype_thermometer ) {
            p->add_msg_if_player( m_neutral, _( "The %1$s reads %2$s." ), it->tname(),
                                  print_temperature( player_local_temp ) );
        } else {
            p->add_msg_if_player( m_neutral, _( "Temperature: %s." ),
                                  print_temperature( player_local_temp ) );
        }
        // TODO: Don't output air temp if we aren't near air
        if( g->m.has_flag( TFLAG_SWIMMABLE, p->pos() ) ) {
            const units::temperature water_temp = weather.get_cur_weather_gen().get_water_temperature(
                    tripoint_abs_ms( here.getabs( p->pos() ) ),
                    calendar::turn, calendar::config, g->get_seed() );
            p->add_msg_if_player( m_neutral, _( "Water temperature: %s." ),
                                  print_temperature( water_temp ) );
        }
    }
    if( it->has_flag( flag_HYGROMETER ) ) {
        if( it->typeId() == itype_hygrometer ) {
            p->add_msg_if_player(
                m_neutral, _( "The %1$s reads %2$s." ), it->tname(),
                print_humidity( get_local_humidity( weatherPoint.humidity, get_weather().weather_id,
                                                    g->is_sheltered( p->pos() ) ) ) );
        } else {
            p->add_msg_if_player(
                m_neutral, _( "Relative Humidity: %s." ),
                print_humidity( get_local_humidity( weatherPoint.humidity, get_weather().weather_id,
                                                    g->is_sheltered( p->pos() ) ) ) );
        }
    }
    if( it->has_flag( flag_BAROMETER ) ) {
        if( it->typeId() == itype_barometer ) {
            p->add_msg_if_player(
                m_neutral, _( "The %1$s reads %2$s." ), it->tname(),
                print_pressure( static_cast<int>( weatherPoint.pressure ) ) );
        } else {
            p->add_msg_if_player( m_neutral, _( "Pressure: %s." ),
                                  print_pressure( static_cast<int>( weatherPoint.pressure ) ) );
        }
    }
    if( it->has_flag( flag_WEATHER_FORECAST ) ) {
        std::string message = string_format( "", message );
        const auto tref = overmap_buffer.find_radio_station( it->frequency );
        if( tref ) {
            {
                message = weather_forecast( tref.abs_sm_pos );
            }
            p->add_msg_if_player( m_neutral, _( "Automatic weather report %s" ), message );
        }
    }
    if( it->has_flag( flag_WINDMETER ) ) {
        int vehwindspeed = 0;
        if( optional_vpart_position vp = g->m.veh_at( p->pos() ) ) {
            vehwindspeed = std::abs( vp->vehicle().velocity / 100 ); // For mph
        }
        const oter_id &cur_om_ter = overmap_buffer.ter( p->global_omt_location() );
        /* windpower defined in internal velocity units (=.01 mph) */
        const double windpower = 100 * get_local_windpower( weather.windspeed + vehwindspeed, cur_om_ter,
                                 p->pos(), weather.winddirection, g->is_sheltered( p->pos() ) );
        std::string dirstring = get_dirstring( weather.winddirection );
        p->add_msg_if_player( m_neutral, _( "Wind: %.1f %2$s from the %3$s.\nFeels like: %4$s." ),
                              convert_velocity( windpower, VU_VEHICLE ),
                              velocity_units( VU_VEHICLE ), dirstring, print_temperature(
                                  get_local_windchill( units::to_fahrenheit( weatherPoint.temperature ),
                                          weatherPoint.humidity,
                                          windpower / 100 ) +
                                  units::to_fahrenheit( player_local_temp ) ) );
    }

    return 0;
}

int iuse::directional_hologram( player *p, item *it, bool, const tripoint &pos )
{
    if( it->is_armor() &&  !( p->is_worn( *it ) ) ) {
        p->add_msg_if_player( m_neutral, _( "You need to wear the %1$s before activating it." ),
                              it->tname() );
        return 0;
    }
    const std::optional<tripoint> posp_ = choose_adjacent( _( "Choose hologram direction." ) );
    if( !posp_ ) {
        return 0;
    }
    const tripoint posp = *posp_;

    monster *const hologram = g->place_critter_at( mon_hologram, posp );
    if( !hologram ) {
        p->add_msg_if_player( m_info, _( "Can't create a hologram there." ) );
        return 0;
    }
    tripoint target = pos;
    target.x = p->posx() + 4 * SEEX * ( posp.x - p->posx() );
    target.y = p->posy() + 4 * SEEY * ( posp.y - p->posy() );
    hologram->friendly = -1;
    hologram->add_effect( effect_docile, 1_hours );
    hologram->wandf = -30;
    hologram->set_summon_time( 60_seconds );
    hologram->set_dest( target );
    p->mod_moves( -to_turns<int>( 1_seconds ) );
    return it->type->charges_to_use();
}

int iuse::capture_monster_veh( player *p, item *it, bool, const tripoint &pos )
{
    if( p->is_mounted() ) {
        p->add_msg_if_player( m_info, _( "You cannot do that while mounted." ) );
        return 0;
    }
    if( !it->has_flag( flag_VEHICLE ) ) {
        p->add_msg_if_player( m_info, _( "The %s must be installed in a vehicle before being loaded." ),
                              it->tname() );
        return 0;
    }
    capture_monster_act( p, it, false, pos );
    return 0;
}

bool item::release_monster( const tripoint &target, const int radius )
{
    shared_ptr_fast<monster> new_monster = make_shared_fast<monster>();
    try {
        ::deserialize( *new_monster, get_var( "contained_json", "" ) );
    } catch( const std::exception &e ) {
        debugmsg( _( "Error restoring monster: %s" ), e.what() );
        return false;
    }
    if( !g->place_critter_around( new_monster, target, radius ) ) {
        return false;
    }
    erase_var( "contained_name" );
    erase_var( "contained_json" );
    erase_var( "name" );
    erase_var( "weight" );
    return true;
}

// didn't want to drag the monster:: definition into item.h, so just reacquire the monster
// at target
int item::contain_monster( const tripoint &target )
{
    const monster *const mon_ptr = g->critter_at<monster>( target );
    if( !mon_ptr ) {
        return 0;
    }
    const monster &f = *mon_ptr;

    set_var( "contained_json", ::serialize( f ) );
    set_var( "contained_name", f.type->nname() );
    set_var( "name", string_format( _( "%s holding %s" ), type->nname( 1 ),
                                    f.type->nname() ) );
    // Need to add the weight of the empty container because item::weight uses the "weight" variable directly.
    set_var( "weight", to_milligram( type->weight + f.get_weight() ) );
    g->remove_zombie( f );
    return 0;
}

int iuse::capture_monster_act( player *p, item *it, bool, const tripoint &pos )
{
    if( p->is_mounted() ) {
        p->add_msg_if_player( m_info, _( "You cannot capture a creature mounted." ) );
        return 0;
    }
    if( it->has_var( "contained_name" ) ) {
        // Remember contained_name for messages after release_monster erases it
        const std::string contained_name = it->get_var( "contained_name", "" );

        if( it->release_monster( pos ) ) {
            // It's been activated somewhere where there isn't a player or monster, good.
            return 0;
        }
        if( it->has_flag( flag_PLACE_RANDOMLY ) ) {
            if( it->release_monster( p->pos(), 1 ) ) {
                return 0;
            }
            p->add_msg_if_player( _( "There is no place to put the %s." ), contained_name );
            return 0;
        } else {
            const std::string query = string_format( _( "Place the %s where?" ), contained_name );
            const std::optional<tripoint> pos_ = choose_adjacent( query );
            if( !pos_ ) {
                return 0;
            }
            if( it->release_monster( *pos_ ) ) {
                p->add_msg_if_player( _( "You release the %s." ), contained_name );
                return 0;
            }
            p->add_msg_if_player( m_info, _( "You cannot place the %s there!" ), contained_name );
            return 0;
        }
    } else {
        if( !it->has_property( "creature_size_capacity" ) ) {
            debugmsg( "%s has no creature_size_capacity.", it->tname() );
            return 0;
        }
        const std::string capacity = it->get_property_string( "creature_size_capacity" );
        if( !Creature::size_map.contains( capacity ) ) {
            debugmsg( "%s has invalid creature_size_capacity %s.",
                      it->tname(), capacity.c_str() );
            return 0;
        }
        const std::function<bool( const tripoint & )> adjacent_capturable = []( const tripoint & pnt ) {
            const monster *mon_ptr = g->critter_at<monster>( pnt );
            return mon_ptr != nullptr;
        };
        const std::string query = string_format( _( "Grab which creature to place in the %s?" ),
                                  it->tname() );
        const std::optional<tripoint> target_ = choose_adjacent_highlight( query,
                                                _( "There is no creature nearby you can capture." ), adjacent_capturable, false );
        if( !target_ ) {
            p->add_msg_if_player( m_info, _( "You cannot use a %s there." ), it->tname() );
            return 0;
        }
        const tripoint target = *target_;

        // Capture the thing, if it's on the target square.
        if( const monster *const mon_ptr = g->critter_at<monster>( target ) ) {
            const monster &f = *mon_ptr;

            if( f.get_size() > Creature::size_map.find( capacity )->second ) {
                p->add_msg_if_player( m_info, _( "The %1$s is too big to put in your %2$s." ),
                                      f.type->nname(), it->tname() );
                return 0;
            }
            // TODO: replace this with some kind of melee check.
            int chance = f.hp_percentage() / 10;
            // A weaker monster is easier to capture.
            // If the monster is friendly, then put it in the item
            // without checking if it rolled a success.
            if( f.friendly != 0 || one_in( chance ) ) {
                p->add_msg_if_player( _( "You capture the %1$s in your %2$s." ),
                                      f.type->nname(), it->tname() );
                return it->contain_monster( target );
            } else {
                p->add_msg_if_player( m_bad, _( "The %1$s avoids your attempts to put it in the %2$s." ),
                                      f.type->nname(), it->type->nname( 1 ) );
            }
            p->moves -= to_moves<int>( 1_seconds );
        } else {
            add_msg( _( "The %s can't capture nothing" ), it->tname() );
            return 0;
        }
    }
    return 0;
}

int iuse::ladder( player *p, item *, bool, const tripoint & )
{
    if( !g->m.has_zlevels() ) {
        debugmsg( "Ladder can't be used in non-z-level mode" );
        return 0;
    }
    if( p->is_mounted() ) {
        p->add_msg_if_player( m_info, _( "You cannot do that while mounted." ) );
        return 0;
    }
    const std::optional<tripoint> pnt_ = choose_adjacent( _( "Put the ladder where?" ) );
    if( !pnt_ ) {
        return 0;
    }
    const tripoint pnt = *pnt_;

    if( !g->is_empty( pnt ) || g->m.has_furn( pnt ) ) {
        p->add_msg_if_player( m_bad, _( "Can't place it there." ) );
        return 0;
    }

    p->add_msg_if_player( _( "You set down the ladder." ) );
    p->moves -= to_moves<int>( 5_seconds );
    g->m.furn_set( pnt, furn_str_id( "f_ladder" ) );
    return 1;
}


int iuse::weak_antibiotic( player *p, item *it, bool, const tripoint & )
{
    p->add_msg_player_or_npc( m_neutral,
                              _( "You take some weak antibiotics." ),
                              _( "<npcname> takes some weak antibiotics." ) );
    if( p->has_effect( effect_infected ) && !p->has_effect( effect_weak_antibiotic ) ) {
        p->add_msg_if_player( m_good, _( "The throbbing of the infection diminishes.  Slightly." ) );
    }
    p->add_effect( effect_weak_antibiotic, 12_hours );
    return it->type->charges_to_use();
}

int iuse::strong_antibiotic( player *p, item *it, bool, const tripoint & )
{
    p->add_msg_player_or_npc( m_neutral,
                              _( "You take some strong antibiotics." ),
                              _( "<npcname> takes some strong antibiotics." ) );
    if( p->has_effect( effect_infected ) && !p->has_effect( effect_strong_antibiotic ) ) {
        p->add_msg_if_player( m_good, _( "You feel much better - almost entirely." ) );
    }
    p->add_effect( effect_strong_antibiotic, 12_hours );
    return it->type->charges_to_use();
}

int iuse::craft( player *p, item *it, bool, const tripoint &pos )
{
    if( p->is_mounted() ) {
        p->add_msg_if_player( m_info, _( "You cannot do that while mounted." ) );
        return 0;
    }

    const std::string craft_name = it->tname();

    if( !it->is_craft() ) {
        debugmsg( "Attempted to start working on non craft '%s.'  Aborting.", craft_name );
        return 0;
    }

    if( !p->can_continue_craft( *it ) ) {
        return 0;
    }
    const recipe &rec = it->get_making();
    if( p->has_recipe( &rec, p->crafting_inventory(),
                       character_funcs::get_crafting_helpers( *p ) ) == -1 ) {
        p->add_msg_player_or_npc(
            _( "You don't know the recipe for the %s and can't continue crafting." ),
            _( "<npcname> doesn't know the recipe for the %s and can't continue crafting." ),
            rec.result_name() );
        return 0;
    }

    bench_location best_bench = find_best_bench( *p, *it );
    p->add_msg_player_or_npc(
        pgettext( "in progress craft", "You start working on the %s." ),
        pgettext( "in progress craft", "<npcname> starts working on the %s." ), craft_name );
    p->assign_activity( ACT_CRAFT );
    // Horrible! We have to find the item again...
    item *where = nullptr;
    if( p->has_item( *it ) ) {
        where = it;
    } else if( const std::optional<vpart_reference> vp = g->m.veh_at( pos ).part_with_feature( "CARGO",
               false ) ) {
        const vehicle_cursor vc = vehicle_cursor( vp->vehicle(), vp->part_index() );
        if( vc.has_item( *it ) ) {
            where = it;
        }
    }

    if( !where ) {
        map_cursor mc = map_cursor( pos );
        if( mc.has_item( *it ) ) {
            where = it;
        } else {
            debugmsg( "Incomplete item couldn't be found" );
            return 0;
        }
    }
    p->activity->targets.emplace_back( where );
    p->activity->coords.push_back( best_bench.position );
    p->activity->values.push_back( 0 ); // Not a long craft
    // Ugly
    p->activity->values.push_back( static_cast<int>( best_bench.type ) );

    return 0;
}

int iuse::disassemble( player *p, item *it, bool, const tripoint & )
{
    if( !p->is_avatar() ) {
        debugmsg( "disassemble iuse is not implemented for NPCs." );
        return 0;
    }
    if( p->is_mounted() ) {
        p->add_msg_if_player( m_info, _( "You cannot do that while mounted." ) );
        return 0;
    }
    if( !p->has_item( *it ) ) {
        return 0;
    }

    crafting::disassemble( *p->as_avatar(), *it );

    return 0;
}

int iuse::melatonin_tablet( player *p, item *it, bool, const tripoint & )
{
    p->add_msg_if_player( _( "You pop a %s." ), it->tname() );
    if( p->has_effect( effect_melatonin_supplements ) ) {
        p->add_msg_if_player( m_warning,
                              _( "Simply taking more melatonin won't help.  You have to go to sleep for it to work." ) );
    }
    p->add_effect( effect_melatonin_supplements, 16_hours );
    return it->type->charges_to_use();
}

int iuse::coin_flip( player *p, item *it, bool, const tripoint & )
{
    p->add_msg_if_player( m_info, _( "You flip a %s." ), it->tname() );
    p->add_msg_if_player( m_info, one_in( 2 ) ? _( "Heads!" ) : _( "Tails!" ) );
    return 0;
}

int iuse::play_game( player *p, item *it, bool t, const tripoint & )
{
    if( t ) {
        return 0;
    }

    if( query_yn( _( "Play a game with the %s?" ), it->tname() ) ) {
        p->add_msg_if_player( _( "You start playing." ) );
        p->assign_activity( ACT_GENERIC_GAME, to_moves<int>( 30_minutes ), -1,
                            p->get_item_position( it ), "gaming" );
    }
    return 0;
}

int iuse::magic_8_ball( player *p, item *it, bool, const tripoint & )
{
    enum {
        BALL8_GOOD,
        BALL8_UNK = 10,
        BALL8_BAD = 15
    };
    static const std::array<const char *, 20> tab = {{
            translate_marker( "It is certain." ),
            translate_marker( "It is decidedly so." ),
            translate_marker( "Without a doubt." ),
            translate_marker( "Yes - definitely." ),
            translate_marker( "You may rely on it." ),
            translate_marker( "As I see it, yes." ),
            translate_marker( "Most likely." ),
            translate_marker( "Outlook good." ),
            translate_marker( "Yes." ),
            translate_marker( "Signs point to yes." ),
            translate_marker( "Reply hazy, try again." ),
            translate_marker( "Ask again later." ),
            translate_marker( "Better not tell you now." ),
            translate_marker( "Cannot predict now." ),
            translate_marker( "Concentrate and ask again." ),
            translate_marker( "Don't count on it." ),
            translate_marker( "My reply is no." ),
            translate_marker( "My sources say no." ),
            translate_marker( "Outlook not so good." ),
            translate_marker( "Very doubtful." )
        }
    };

    p->add_msg_if_player( m_info, _( "You ask the %s, then flip it." ), it->tname() );
    int rn = rng( 0, tab.size() - 1 );
    auto color = ( rn >= BALL8_BAD ? m_bad : rn >= BALL8_UNK ? m_info : m_good );
    p->add_msg_if_player( color, _( "The %s says: %s" ), it->tname(), _( tab[rn] ) );
    return 0;
}

int iuse::toggle_heats_food( player *p, item *it, bool, const tripoint & )
{
    static const flag_id json_flag_HEATS_FOOD( flag_HEATS_FOOD );
    if( !it->has_flag( json_flag_HEATS_FOOD ) ) {
        it->item_tags.insert( json_flag_HEATS_FOOD );
        p->add_msg_if_player(
            _( "You will try to use %s to heat food next time you eat something that should be eaten hot." ),
            it->tname().c_str() );
    } else {
        it->item_tags.erase( json_flag_HEATS_FOOD );
        p->add_msg_if_player( _( "You will no longer use %s to heat food." ), it->tname().c_str() );
    }

    return 0;
}

int iuse::toggle_ups_charging( player *p, item *it, bool, const tripoint & )
{
    static const flag_id json_flag_USE_UPS( flag_USE_UPS );
    if( !it->has_flag( json_flag_USE_UPS ) ) {
        it->item_tags.insert( json_flag_USE_UPS );
        p->add_msg_if_player(
            _( "You will recharge the %s using any available Unified Power System." ),
            it->tname().c_str() );
    } else {
        it->item_tags.erase( json_flag_USE_UPS );
        p->add_msg_if_player( _( "You will no longer recharge the %s via UPS." ), it->tname().c_str() );
    }

    return 0;
}

int iuse::report_grid_charge( player *p, item *, bool, const tripoint &pos )
{
    const tripoint_abs_ms pos_abs( get_map().getabs( pos ) );
    const distribution_grid &gr = get_distribution_grid_tracker().grid_at( pos_abs );
    const int amt = gr.get_resource();
    const auto stat = gr.get_power_stat();

    std::string msg = string_format( _( "This electric grid stores %d kJ of electric power." ), amt );

    // format in MW/kW with three-point precision
    auto display_watt = []( int64_t watts = 0 ) {
        if( std::abs( watts ) >= 1'000'000 ) {
            return string_format( "%.3f MW", watts / 1'000'000.0 );
        } else if( std::abs( watts ) >= 1'000 ) {
            return string_format( "%.3f kW", watts / 1'000.0 );
        } else {
            return string_format( "%d W", watts );
        }
    };

    if( stat.gen_w > 0 || stat.use_w > 0 ) {
        msg += string_format( _( "\nGeneration: %s" ), display_watt( stat.gen_w ) );
        msg += string_format( _( "\nConsumption: %s" ), display_watt( stat.use_w ) );
        msg += string_format( _( "\nNet: %s" ), display_watt( stat.net_w() ) );
    }
    p->add_msg_if_player( "%s", msg );
    return 0;
}

int iuse::report_grid_connections( player *p, item *, bool, const tripoint &pos )
{
    tripoint_abs_omt pos_abs = project_to<coords::omt>( tripoint_abs_ms( get_map().getabs( pos ) ) );
    std::vector<tripoint_rel_omt> connections = overmap_buffer.electric_grid_connectivity_at( pos_abs );

    std::vector<std::string> connection_names;
    connection_names.reserve( connections.size() );
    for( const tripoint_rel_omt &delta : connections ) {
        connection_names.push_back( direction_name( direction_from( delta.raw() ) ) );
    }

    std::string msg;
    if( connection_names.empty() ) {
        msg = _( "This electric grid has no connections." );
    } else {
        //~ %s is list of directions
        msg = string_format( _( "This electric grid has connections: %s." ),
                             enumerate_as_string( connection_names ) );
    }
    p->add_msg_if_player( msg );

    return 0;
}

int iuse::modify_grid_connections( player *p, item *it, bool, const tripoint &pos )
{
    tripoint_abs_omt pos_abs = project_to<coords::omt>( tripoint_abs_ms( get_map().getabs( pos ) ) );
    std::vector<tripoint_rel_omt> connections = overmap_buffer.electric_grid_connectivity_at( pos_abs );

    uilist ui;

    std::bitset<six_cardinal_directions.size()> connection_present;
    for( size_t i = 0; i < six_cardinal_directions.size(); i++ ) {
        const tripoint &delta = six_cardinal_directions[i];
        connection_present[i] = std::count( connections.begin(), connections.end(),
                                            tripoint_rel_omt( delta ) );
        std::string name = direction_name( direction_from( delta ) );
        int i_int = static_cast<int>( i );
        const char *format = connection_present[i]
                             ? _( "Remove connection in direction: %s" )
                             : _( "Add connection in direction: %s" );
        int new_z = pos.z + delta.z;
        bool enabled = new_z >= -10 && new_z <= 10;
        ui.addentry( i_int, enabled, i_int, format, name.c_str() );
    }

    ui.query();
    if( ui.ret < 0 ) {
        return 0;
    }

    size_t ret = static_cast<size_t>( ui.ret );
    tripoint_abs_omt destination_pos_abs = pos_abs + tripoint_rel_omt( six_cardinal_directions[ret] );
    if( connection_present[ret] ) {
        overmap_buffer.remove_grid_connection( pos_abs, destination_pos_abs );
    } else {
        std::set<tripoint_abs_omt> lhs_locations = overmap_buffer.electric_grid_at( pos_abs );
        std::set<tripoint_abs_omt> rhs_locations = overmap_buffer.electric_grid_at( destination_pos_abs );
        int cost_mult;
        if( lhs_locations == rhs_locations ) {
            cost_mult = 0;
        } else {
            cost_mult = lhs_locations.size() + rhs_locations.size();
        }
        const requirement_data &reqs = *requirement_add_grid_connection * cost_mult;
        const inventory &crafting_inv = p->crafting_inventory();
        std::string grid_connection_string;
        if( cost_mult == 0 ) {
            grid_connection_string = string_format(
                                         _( "You are connecting two locations in the same grid, with %lu elements." ),
                                         std::max( lhs_locations.size(), rhs_locations.size() ) );
        } else if( lhs_locations.size() == 1 || rhs_locations.size() == 1 ) {
            grid_connection_string = string_format(
                                         _( "You are extending a grid with %lu elements." ),
                                         std::max( lhs_locations.size(), rhs_locations.size() ) );
        } else {
            grid_connection_string = string_format(
                                         _( "You are connecting a grid with %lu elements to a grid with %lu elements." ),
                                         lhs_locations.size(),
                                         rhs_locations.size() );
        }

        if( !requirement_add_grid_connection->can_make_with_inventory( crafting_inv,
                is_crafting_component ) ) {
            popup( string_format( _( "%s\n%s\n%s" ),
                                  grid_connection_string,
                                  reqs.list_missing(),
                                  reqs.list_all() ) );
            return 0;
        }

        // TODO: Long action
        if( ( cost_mult == 0 &&
              query_yn( string_format( _( "%s\nThis action will not consume any resources.\nAre you sure?" ),
                                       grid_connection_string ) ) ) ||
            query_yn( string_format( std::string( "%s\n%s\n" ) + _( "Are you sure?" ),
                                     grid_connection_string,
                                     reqs.list_all() ) ) )
        {} else {
            return 0;
        }


        for( const auto &e : reqs.get_components() ) {
            p->consume_items( e );
        }
        for( const auto &e : reqs.get_tools() ) {
            p->consume_tools( e );
        }
        p->invalidate_crafting_inventory();

        bool success = overmap_buffer.add_grid_connection( pos_abs, destination_pos_abs );
        if( success ) {
            return it->type->charges_to_use();
        }
    }

    return 0;
}

int iuse::amputate( player *, item *it, bool, const tripoint &pos )
{
    if( !it->ammo_sufficient() ) {
        return 0;
    }

    Creature *patient = g->critter_at<Character>( pos );
    if( !patient ) {
        add_msg( m_info, _( "Nevermind." ) );
        return 0;
    }

    auto &body = patient->get_body();

    uilist bp_menu;
    bp_menu.text = _( "Select body part to amputate:" );
    bp_menu.allow_cancel = true;

    for( const auto &pr : body ) {
        bp_menu.addentry( pr.first->name_as_heading.translated() );
    }

    bp_menu.query();
    if( bp_menu.ret < 0 ) {
        add_msg( m_info, _( "Nevermind." ) );
        return 0;
    }

    auto bp_iter = std::next( body.begin(), bp_menu.ret );
    // Prepare for bugs!
    add_msg( m_bad, _( "Body part removed: %s" ), bp_iter->first->name_as_heading.translated() );
    body.erase( bp_iter );

    return it->type->charges_to_use();
}

void use_function::dump_info( const item &it, std::vector<iteminfo> &dump ) const
{
    if( actor != nullptr ) {
        actor->info( it, dump );
    }
}

ret_val<bool> use_function::can_call( const Character &p, const item &it, bool t,
                                      const tripoint &pos ) const
{
    if( actor == nullptr ) {
        return ret_val<bool>::make_failure( _( "You can't do anything interesting with your %s." ),
                                            it.tname() );
    }

    return actor->can_use( p, it, t, pos );
}

int use_function::call( player &p, item &it, bool active, const tripoint &pos ) const
{
    return actor->use( p, it, active, pos );
}

int iuse::bullet_vibe_on( player *p, item *it, bool t, const tripoint & )
{
    if( t ) { // Normal use
        if( p->has_item( *it ) ) {
            // Only triggers every 1 minute so that fatigue isn't ridiculous
            if( calendar::once_every( 1_minutes ) ) {
                p->add_morale( MORALE_FEELING_GOOD, 1, 30, 20_minutes, 10_minutes, true );
                p->mod_fatigue( 1 );
            }
        }
    } else {
        // Most generic way to figure out the base item I can think of
        // There's *probably* a better way to do this, but this works
        std::string active_item = it->typeId().str();
        std::string base_item = active_item.erase( active_item.rfind( '_' ) );

        p->add_msg_if_player( _( "The %s turns off." ), it->display_name() );
        it->convert( itype_id( base_item ) );
        it->deactivate();

    }
    return it->type->charges_to_use();
}
