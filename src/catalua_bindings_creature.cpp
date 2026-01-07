#include "catalua_bindings.h"

#include "activity_type.h"
#include "avatar.h"
#include "bionics.h"
#include "bodypart.h"
#include "catalua.h"
#include "catalua_bindings_utils.h"
#include "catalua_impl.h"
#include "catalua_log.h"
#include "catalua_luna.h"
#include "catalua_luna_doc.h"
#include "character.h"
#include "creature.h"
#include "damage.h"
#include "disease.h"
#include "enums.h"
#include "field.h"
#include "field_type.h"
#include "flag.h"
#include "flag_trait.h"
#include "inventory.h"
#include "magic.h"
#include "map.h"
#include "monfaction.h"
#include "monster.h"
#include "morale_types.h"
#include "mtype.h"
#include "mutation.h"
#include "npc.h"
#include "player.h"
#include "pldata.h"
#include "recipe.h"
#include "requirements.h"
#include "skill.h"
#include "type_id.h"

void cata::detail::reg_creature_family( sol::state &lua )
{
    reg_creature( lua );
    reg_monster( lua );
    reg_character( lua );
    reg_player( lua );
    reg_npc( lua );
    reg_avatar( lua );
}

void cata::detail::reg_creature( sol::state &lua )
{
#define UT_CLASS Creature
    {
        sol::usertype<UT_CLASS> ut =
        luna::new_usertype<UT_CLASS>(
            lua,
            luna::no_bases,
            luna::no_constructor
        );

        // Methods
        SET_FX_T( get_name, std::string() const );
        DOC_PARAMS( "possessive", "capitalize_first" );
        SET_FX_T( disp_name, std::string( bool, bool ) const );
        SET_FX_T( skin_name, std::string() const );
        SET_FX_T( get_grammatical_genders, std::vector<std::string>() const );

        SET_FX_T( is_avatar, bool() const );
        SET_FX_T( is_npc, bool() const );
        SET_FX_T( is_monster, bool() const );
        SET_FX_T( as_monster, monster * () );
        SET_FX_T( as_npc, npc * () );
        SET_FX_T( as_character, Character * () );
        SET_FX_T( as_avatar, avatar * () );

        SET_FX_T( dodge_roll, float() );
        SET_FX_T( stability_roll, float() const );

        SET_FX_T( attitude_to, Attitude( const Creature & ) const );

        luna::set_fx( ut, "sees", sol::overload(
                          sol::resolve<bool( const Creature & ) const>( &Creature::sees ),
        []( const Creature & cr, const tripoint & t ) -> bool { return cr.sees( t ); }
                      ) );

        SET_FX_T( sight_range, int( int ) const );

        SET_FX_T( power_rating, float() const );

        SET_FX_T( speed_rating, float() const );

        SET_FX_T( ranged_target_size, double() const );

        SET_FX_T( knock_back_to, void( const tripoint & ) );

        SET_FX_T( deal_damage, dealt_damage_instance( Creature * source, bodypart_id bp,
                  const damage_instance & dam ) );

        SET_FX_T( apply_damage, void( Creature * source, bodypart_id bp, int amount,
                                      bool bypass_med ) );

        SET_FX_T( size_melee_penalty, int() const );

        SET_FX_T( digging, bool() const );
        SET_FX_T( is_on_ground, bool() const );
        SET_FX_T( is_underwater, bool() const );
        SET_FX_T( set_underwater, void( bool x ) );
        SET_FX_T( is_warm, bool() const );
        SET_FX_T( in_species, bool( const species_id & ) const );

        SET_FX_T( has_weapon, bool() const );
        SET_FX_T( is_hallucination, bool() const );
        SET_FX_N_T( is_dead_state, "is_dead", bool() const );

        SET_FX_T( is_elec_immune, bool() const );
        SET_FX_T( is_immune_effect, bool( const efftype_id & ) const );
        SET_FX_T( is_immune_damage, bool( damage_type ) const );

        SET_FX_N_T( pos, "get_pos_ms", const tripoint & () const );

        SET_FX_N_T( setpos, "set_pos_ms", void( const tripoint & ) );

        luna::set_fx( ut, "has_effect", []( const Creature & cr, const efftype_id & eff,
        sol::optional<const bodypart_str_id &> bpid ) -> bool {
            if( bpid.has_value() )
            {
                return cr.has_effect( eff, *bpid );
            } else
            {
                return cr.has_effect( eff );
            }
        } );

        luna::set_fx( ut, "get_effect", []( Creature & cr, const efftype_id & eff,
        sol::optional<const bodypart_str_id &> bpid ) -> effect & {
            if( bpid.has_value() )
            {
                return cr.get_effect( eff, *bpid );
            } else
            {
                return cr.get_effect( eff );
            }
        } );

        luna::set_fx( ut, "has_effect_with_flag", []( const Creature & cr,
        const flag_id & flag, sol::optional<const bodypart_str_id &> bpid ) -> bool {
            const bodypart_str_id &bp = bpid ? *bpid : bodypart_str_id::NULL_ID();
            return cr.has_effect_with_flag( flag, bp );
        } );

        luna::set_fx( ut, "get_effect_dur", []( const Creature & cr, const efftype_id & eff,
        sol::optional<const bodypart_str_id &> bpid ) -> time_duration {
            const bodypart_str_id &bp = bpid ? *bpid : bodypart_str_id::NULL_ID();
            return cr.get_effect_dur( eff, bp );
        } );

        luna::set_fx( ut, "get_effect_int", []( const Creature & cr, const efftype_id & eff,
        sol::optional<const bodypart_str_id &> bpid ) -> int {
            const bodypart_str_id &bp = bpid ? *bpid : bodypart_str_id::NULL_ID();
            return cr.get_effect_int( eff, bp );
        } );

        DOC( "Effect type, duration, bodypart and intensity" );
        luna::set_fx( ut, "add_effect", []( Creature & cr, const efftype_id & eff,
                                            const time_duration & dur,
                                            sol::optional<const bodypart_str_id &> bpid,
                                            sol::optional<int> intensity
                                          )
        {
            int eint = intensity ? *intensity : 0;
            const bodypart_str_id &bp = bpid ? *bpid : bodypart_str_id::NULL_ID();
            cr.add_effect( eff, dur, bp, eint );
        } );

        luna::set_fx( ut, "remove_effect", []( Creature & cr, const efftype_id & eff,
        sol::optional<const bodypart_str_id &> bpid ) -> bool {
            const bodypart_str_id &bp = bpid ? *bpid : bodypart_str_id::NULL_ID();
            return cr.remove_effect( eff, bp );
        } );

        SET_FX_T( clear_effects, void() );

        DOC( "Sets an arbitrary key : value pair for the creature.NPC dialogue system uses this, with the format(\"npctalk_var\" + \"_\" + type_var + \"_\" + var_context + \"_\" + var_base_name) used for the key, skipping type or context if empty." );
        SET_FX_T( set_value, void( const std::string &, const std::string & ) );
        DOC( "Removes an arbitrary entry using the same key format as set_value." );
        SET_FX_T( remove_value, void( const std::string & ) );
        DOC( "Retrieves an arbitrary entry using the same key format as set_value." );
        SET_FX_T( get_value, std::string( const std::string & ) const );

        SET_FX_T( get_weight, units::mass() const );

        SET_FX_T( has_trait, bool( const trait_id & ) const );

        SET_FX_T( mod_pain, void( int ) );
        SET_FX_T( mod_pain_noresist, void( int ) );
        SET_FX_T( set_pain, void( int ) );
        SET_FX_T( get_pain, int() const );
        SET_FX_T( get_perceived_pain, int() const );

        SET_FX_T( get_moves, int() const );
        SET_FX_T( mod_moves, void( int ) );
        SET_FX_T( set_moves, void( int ) );

        SET_FX_T( get_num_blocks, int() const );
        SET_FX_T( get_num_dodges, int() const );

        SET_FX_T( get_env_resist, int( bodypart_id ) const );

        SET_FX_T( get_armor_bash, int( bodypart_id ) const );
        SET_FX_T( get_armor_cut, int( bodypart_id ) const );
        SET_FX_T( get_armor_bullet, int( bodypart_id ) const );
        SET_FX_T( get_armor_bash_base, int( bodypart_id ) const );
        SET_FX_T( get_armor_cut_base, int( bodypart_id ) const );
        SET_FX_T( get_armor_bullet_base, int( bodypart_id ) const );
        SET_FX_T( get_armor_bash_bonus, int() const );
        SET_FX_T( get_armor_cut_bonus, int() const );
        SET_FX_T( get_armor_bullet_bonus, int() const );

        SET_FX_T( get_armor_type, int( damage_type, bodypart_id ) const );

        SET_FX_T( get_dodge, float() const );

        SET_FX_T( get_melee, float() const );
        SET_FX_T( get_hit, float() const );

        SET_FX_T( get_speed, int() const );
        SET_FX_T( get_size, creature_size() const );
        luna::set_fx( ut, "get_hp", []( const Creature & cr,
        sol::optional<const bodypart_id &> bpid ) -> int {
            if( bpid.has_value() )
            {
                return cr.get_hp( *bpid );
            } else
            {
                return cr.get_hp();
            }
        } );
        luna::set_fx( ut, "get_hp_max", []( const Creature & cr,
        sol::optional<const bodypart_id &> bpid ) -> int {
            if( bpid.has_value() )
            {
                return cr.get_hp_max( *bpid );
            } else
            {
                return cr.get_hp_max();
            }
        } );

        SET_FX_T( hp_percentage, int() const );
        SET_FX_T( has_flag, bool( const m_flag ) const );

        SET_FX_T( get_part_hp_cur, int( const bodypart_id & ) const );
        SET_FX_T( get_part_hp_max, int( const bodypart_id & ) const );

        SET_FX_T( get_part_healed_total, int( const bodypart_id & ) const );

        SET_FX_T( set_part_hp_cur, void( const bodypart_id &, int ) );
        SET_FX_T( set_part_hp_max, void( const bodypart_id &, int ) );
        SET_FX_T( mod_part_hp_cur, void( const bodypart_id &, int ) );
        SET_FX_T( mod_part_hp_max, void( const bodypart_id &, int ) );

        SET_FX_T( set_all_parts_hp_cur, void( int ) );
        SET_FX_T( mod_all_parts_hp_cur, void( int ) );
        SET_FX_T( set_all_parts_hp_to_max, void() );

        SET_FX_T( set_armor_bash_bonus, void( int ) );
        SET_FX_T( set_armor_cut_bonus, void( int ) );
        SET_FX_T( set_armor_bullet_bonus, void( int ) );

        SET_FX_T( get_speed_base, int() const );
        SET_FX_T( get_speed_bonus, int() const );
        SET_FX_T( get_speed_mult, float() const );
        SET_FX_T( get_block_bonus, int() const );

        SET_FX_T( get_dodge_base, float() const );
        SET_FX_T( get_hit_base, float() const );
        SET_FX_T( get_dodge_bonus, float() const );
        SET_FX_T( get_hit_bonus, float() const );

        SET_FX_T( has_grab_break_tec, bool() const );

        luna::set_fx( ut, "get_weight_capacity", []( UT_CLASS & cr ) -> std::int64_t { return cr.weight_capacity().value(); } );
    }
#undef UT_CLASS // #define UT_CLASS Creature
}

void cata::detail::reg_monster( sol::state &lua )
{
#define UT_CLASS monster
    {
        sol::usertype<UT_CLASS> ut =
        luna::new_usertype<UT_CLASS>(
            lua,
            luna::bases<Creature>(),
            luna::no_constructor
        );

        // Members
        SET_MEMB( friendly );
        SET_MEMB( anger );
        SET_MEMB( morale );
        SET_MEMB( faction );
        SET_MEMB( death_drops );
        SET_MEMB( unique_name );

        // Methods
        // I really don't want to break the uniformity, but...
        luna::set_fx( ut, "get_type", []( const monster & m ) { return m.type -> id; } );
        SET_FX_T( can_upgrade, bool() const );
        SET_FX_T( hasten_upgrade, void() );
        SET_FX_T( get_upgrade_time, int() const );
        SET_FX_T( try_upgrade, void( bool ) );
        SET_FX_T( try_reproduce, void() );
        SET_FX_T( refill_udders, void() );
        SET_FX_T( spawn, void( const tripoint & ) );

        SET_FX_T( name, std::string( unsigned int ) const );
        SET_FX_T( name_with_armor, std::string() const );

        SET_FX_T( can_see, bool() const );
        SET_FX_T( can_hear, bool() const );
        SET_FX_T( can_submerge, bool() const );
        SET_FX_T( can_drown, bool() const );
        SET_FX_T( can_climb, bool() const );
        SET_FX_T( can_dig, bool() const );
        SET_FX_T( digs, bool() const );
        SET_FX_T( flies, bool() const );
        SET_FX_T( climbs, bool() const );
        SET_FX_T( swims, bool() const );

        SET_FX_T( move_target, tripoint() );
        SET_FX_N_T( is_wandering, "is_wandering", bool() );

        SET_FX_T( wander_to, void( const tripoint & p, int f ) );
        SET_FX_T( move_to, bool( const tripoint & p, bool force, bool step_on_critter,
                                 float stagger_adjustment ) );

        SET_FX_T( attitude, monster_attitude( const Character * ) const );

        SET_FX_T( heal, int( int, bool ) );

        SET_FX_T( set_hp, void( int ) );

        SET_FX_T( make_fungus, bool() );
        SET_FX_T( make_friendly, void() );

        SET_FX_T( make_ally, void( const monster & ) );

        SET_FX_T( get_items, const std::vector<item *> &() const );
        luna::set_fx( ut, "add_item", []( monster & m, item * it )
        {
            if( it == nullptr ) { return; }
            detached_ptr<item> ptr = item::spawn( *it );
            m.add_item( std::move( ptr ) );
        } );
        SET_FX_T( remove_item, detached_ptr<item>( item * ) );
        SET_FX_T( clear_items, std::vector<detached_ptr<item>>() );
        SET_FX_T( drop_items, void( const tripoint & ) );
        SET_FX_N_T( drop_items, "drop_items_here", void() );

        luna::set_fx( ut, "add_faction_anger", []( monster & m, const std::string & faction_str, int amount )
        {
            m.add_faction_anger( mfaction_id( faction_str ), amount );
        } );

        luna::set_fx( ut, "get_faction_anger", []( const monster & m, const std::string & faction_str ) -> int {
            return m.get_faction_anger( mfaction_id( faction_str ) );
        } );
    }
#undef UT_CLASS // #define UT_CLASS monster
}

void cata::detail::reg_character( sol::state &lua )
{
#define UT_CLASS Character
    {
        sol::usertype<UT_CLASS> ut =
        luna::new_usertype<UT_CLASS>(
            lua,
            luna::bases<Creature>(),
            luna::no_constructor
        );

        // Members
        SET_MEMB( name );
        SET_MEMB( male );

        SET_MEMB( focus_pool );
        SET_MEMB( cash );
        SET_MEMB( follower_ids );

        SET_MEMB( mutation_category_level );

        // Magic system
        DOC( "Access the character's spellbook and mana pool." );
        luna::set_fx( ut, "get_magic", []( UT_CLASS & c ) -> known_magic& {
            return *c.magic;
        } );

        // Methods
        SET_FX_T( getID, character_id() const );

        SET_FX_T( setID, void( character_id, bool ) );

        SET_FX_T( reset, void() );
        SET_FX_T( reset_encumbrance, void() );

        SET_FX_T( get_str, int() const );
        SET_FX_T( get_dex, int() const );
        SET_FX_T( get_per, int() const );
        SET_FX_T( get_int, int() const );

        SET_FX_T( get_str_base, int() const );
        SET_FX_T( get_dex_base, int() const );
        SET_FX_T( get_per_base, int() const );
        SET_FX_T( get_int_base, int() const );

        SET_FX_T( get_str_bonus, int() const );
        SET_FX_T( get_dex_bonus, int() const );
        SET_FX_T( get_per_bonus, int() const );
        SET_FX_T( get_int_bonus, int() const );

        SET_FX_T( set_str_bonus, void( int ) );
        SET_FX_T( set_dex_bonus, void( int ) );
        SET_FX_T( set_per_bonus, void( int ) );
        SET_FX_T( set_int_bonus, void( int ) );
        SET_FX_T( mod_str_bonus, void( int ) );
        SET_FX_T( mod_dex_bonus, void( int ) );
        SET_FX_T( mod_per_bonus, void( int ) );
        SET_FX_T( mod_int_bonus, void( int ) );
        SET_FX_T( mod_speed_bonus, void( int ) );
        SET_FX_T( set_speed_bonus, void( int ) );

        SET_FX_T( get_healthy, float() const );
        SET_FX_T( get_healthy_mod, float() const );

        SET_FX_T( mod_healthy, void( float ) );
        SET_FX_T( mod_healthy_mod, void( float, float ) );

        SET_FX_T( set_healthy, void( float ) );
        SET_FX_T( set_healthy_mod, void( float ) );

        SET_FX_T( get_stored_kcal, int() const );

        SET_FX_T( max_stored_kcal, int() const );
        SET_FX_T( get_kcal_percent, float() const );
        SET_FX_T( get_thirst, int() const );
        SET_FX_T( get_fatigue, int() const );
        SET_FX_T( get_sleep_deprivation, int() const );

        SET_FX_T( mod_stored_kcal, void( int ) );
        SET_FX_T( mod_thirst, void( int ) );
        SET_FX_T( mod_fatigue, void( int ) );
        SET_FX_T( mod_sleep_deprivation, void( int ) );

        SET_FX_T( set_stored_kcal, void( int ) );
        SET_FX_T( set_thirst, void( int ) );
        SET_FX_T( set_fatigue, void( int ) );
        SET_FX_T( set_sleep_deprivation, void( int ) );

        luna::set_fx( ut, "get_faction_id", []( const UT_CLASS & charac ) -> faction_id {
            faction *fac = charac.get_faction();
            return fac == nullptr ? faction_id::NULL_ID() : fac->id;
        } );
        luna::set_fx( ut, "set_faction_id", []( UT_CLASS & charac, faction_id id ) { charac.set_fac_id( id.str() ); } );

        SET_FX_T( sight_impaired, bool() const );

        SET_FX_T( has_alarm_clock, bool() const );

        SET_FX_T( has_watch, bool() const );

        // These are named with 'BTU' (body temperature units) because other
        // body temperature measurements might/can/should be added later.
        DOC( "Gets the current temperature of a specific body part (in Body Temperature Units)." );
        SET_FX_N_T( get_part_temp_cur, "get_part_temp_btu", int( const bodypart_id & id ) const );
        DOC( "Sets a specific body part to a given temperature (in Body Temperature Units)." );
        SET_FX_N_T( set_part_temp_cur, "set_part_temp_btu", void( const bodypart_id & id, int temp ) );
        DOC( "Gets all bodyparts and their associated temperatures (in Body Temperature Units)." );
        // May want to remove the 'resolve' call, but it clarifies the type...
        luna::set_fx( ut, "get_temp_btu", sol::resolve< std::map<bodypart_id, int>() >( &UT_CLASS::get_temp_cur ) );
        DOC( "Sets ALL body parts on a creature to the given temperature (in Body Temperature Units)." );
        SET_FX_N_T( set_temp_cur, "set_temp_btu", void( int temp ) );

        SET_FX_T( blood_loss, int( const bodypart_id & bp ) const );

        SET_FX_N_T( encumb, "get_part_encumbrance", int( const bodypart_str_id & bp ) const );

        SET_FX_T( is_wearing_power_armor, bool( bool * ) const );

        SET_FX_T( is_wearing_active_power_armor, bool() const );

        SET_FX_T( is_wearing_active_optcloak, bool() const );

        SET_FX_T( in_climate_control, bool() );

        SET_FX_T( is_blind, bool() const );

        SET_FX_T( is_invisible, bool() const );

        SET_FX_T( get_movement_mode, character_movemode() const );

        SET_FX_T( set_movement_mode, void( character_movemode ) );

        SET_FX_T( expose_to_disease, void( diseasetype_id ) );

        SET_FX_T( is_quiet, bool() const );

        SET_FX_T( is_stealthy, bool() const );

        SET_FX( uncanny_dodge );

        SET_FX( get_melee_stamina_cost );

        SET_FX_T( cough, void( bool harmful, int loudness ) );

        SET_FX_T( bionic_armor_bonus, float( const bodypart_id &, damage_type ) const );

        SET_FX_T( mabuff_armor_bonus, int( damage_type ) const );

        SET_FX_T( has_base_trait, bool( const trait_id & b ) const );

        SET_FX_T( has_trait_flag, bool( const trait_flag_str_id & b ) const );

        SET_FX_T( has_opposite_trait, bool( const trait_id & flag ) const );

        SET_FX_T( set_mutation, void( const trait_id & ) );
        SET_FX_T( unset_mutation, void( const trait_id & ) );

        SET_FX_T( activate_mutation, void( const trait_id & ) );
        SET_FX_T( deactivate_mutation, void( const trait_id & ) );

        SET_FX_T( can_mount, bool( const monster & critter ) const );
        SET_FX_T( mount_creature, void( monster & z ) );
        SET_FX_T( is_mounted, bool() const );
        SET_FX_T( check_mount_will_move, bool( const tripoint & dest_loc ) );
        SET_FX_T( check_mount_is_spooked, bool() );
        SET_FX_T( dismount, void() );
        SET_FX_T( forced_dismount, void() );

        SET_FX_T( is_deaf, bool() const );

        SET_FX_T( has_two_arms, bool() const );

        SET_FX_T( get_working_arm_count, int() const );

        SET_FX_T( get_working_leg_count, int() const );

        SET_FX_T( is_limb_disabled, bool( const bodypart_id & ) const );

        SET_FX_T( is_limb_broken, bool( const bodypart_id & ) const );

        SET_FX_T( can_run, bool() );

        SET_FX_T( hurtall, void( int dam, Creature * source, bool ) );

        SET_FX_T( hitall, int( int dam, int vary, Creature * source ) );

        SET_FX_T( heal, void( const bodypart_id &, int ) );

        SET_FX_T( healall, void( int dam ) );

        SET_FX_T( global_square_location, tripoint() const );

        SET_FX_T( global_sm_location, tripoint() const );

        SET_FX_T( has_mabuff, bool( const mabuff_id & ) const );

        SET_FX_T( mabuff_tohit_bonus, float() const );

        SET_FX_T( mabuff_dodge_bonus, float() const );

        SET_FX_T( mabuff_block_bonus, int() const );

        SET_FX_T( mabuff_speed_bonus, int() const );

        SET_FX_T( mabuff_arpen_bonus, int( damage_type ) const );

        SET_FX_T( mabuff_damage_mult, float( damage_type ) const );

        SET_FX_T( mabuff_damage_bonus, int( damage_type ) const );

        SET_FX_T( mabuff_attack_cost_penalty, int() const );

        SET_FX_T( mabuff_attack_cost_mult, float() const );

        SET_FX_T( mutation_effect, void( const trait_id & ) );

        SET_FX_T( mutation_loss_effect, void( const trait_id & ) );

        SET_FX_T( has_active_mutation, bool( const trait_id & ) const );

        SET_FX_T( mutate, void() );

        SET_FX_T( mutation_ok, bool( const trait_id &, bool, bool ) const );

        SET_FX_T( mutate_category, void( const mutation_category_id & ) );

        SET_FX_T( mutate_towards, bool( std::vector<trait_id>, int ) );

        SET_FX_T( mutate_towards, bool( const trait_id & ) );

        luna::set_fx( ut, "mutate_towards", sol::overload(
                          sol::resolve<bool( std::vector<trait_id>, int )>( &UT_CLASS::mutate_towards ),
                          sol::resolve<bool( const trait_id & )>( &UT_CLASS::mutate_towards )
                      ) );


        SET_FX_T( remove_mutation, void( const trait_id &, bool ) );

        SET_FX_T( has_child_flag, bool( const trait_id & flag ) const );

        SET_FX_T( remove_child_flag, void( const trait_id & flag ) );

        SET_FX_T( get_highest_category, mutation_category_id() const );

        SET_FX_T( is_weak_to_water, bool() const );

        SET_FX_T( mutation_armor, float( bodypart_id, damage_type ) const );

        SET_FX_T( get_bionics, std::vector<bionic_id>() const );

        SET_FX_T( has_bionic, bool( const bionic_id & b ) const );

        SET_FX_T( has_active_bionic, bool( const bionic_id & b ) const );

        SET_FX_T( has_any_bionic, bool() const );

        SET_FX_T( has_bionics, bool() const );

        SET_FX_T( clear_bionics, void() );
        SET_FX_T( get_used_bionics_slots, int( const bodypart_id & ) const );
        SET_FX_T( get_total_bionics_slots, int( const bodypart_id & ) const );
        SET_FX_T( get_free_bionics_slots, int( const bodypart_id & ) const );

        SET_FX_T( remove_bionic, void( const bionic_id & ) );

        SET_FX_T( add_bionic, void( const bionic_id & ) );

        SET_FX_T( get_power_level, units::energy() const );
        SET_FX_T( get_max_power_level, units::energy() const );
        SET_FX_T( mod_power_level, void( const units::energy & ) );
        SET_FX_T( mod_max_power_level, void( const units::energy & ) );
        SET_FX_T( set_power_level, void( const units::energy & ) );
        SET_FX_T( set_max_power_level, void( const units::energy & ) );
        SET_FX_T( is_max_power, bool() const );
        SET_FX_T( has_power, bool() const );
        SET_FX_T( has_max_power, bool() const );

        SET_FX_T( is_worn, bool( const item & ) const );

        SET_FX_T( weight_carried, units::mass() const );
        SET_FX_T( volume_carried, units::volume() const );

        SET_FX_T( volume_capacity, units::volume() const );

        SET_FX_T( can_pick_volume, bool( units::volume ) const );
        SET_FX_T( can_pick_weight, bool( units::mass, bool ) const );

        SET_FX_T( is_armed, bool() const );

        luna::set_fx( ut, "can_wield", []( const UT_CLASS & utObj, const item & i ) -> bool {
            const auto result = utObj.can_wield( i );
            return result.success() && result.value();
        } );

        SET_FX_T( wield, bool( item & target ) );

        luna::set_fx( ut, "can_unwield", []( const UT_CLASS & utObj, const item & i ) -> bool {
            const auto result = utObj.can_unwield( i );
            return result.success() && result.value();
        } );

        SET_FX_T( unwield, bool() );

        SET_FX_T( is_wielding, bool( const item & ) const );

        SET_FX_T( is_wearing, bool( const item & ) const );

        SET_FX_T( is_wearing_on_bp, bool( const itype_id &, const bodypart_id & ) const );

        SET_FX_T( worn_with_flag, bool( const flag_id &, const bodypart_id & ) const );

        SET_FX_T( worn_with_id, bool( const itype_id &, const bodypart_id & ) const );

        SET_FX_T( item_worn_with_flag,
                  const item * ( const flag_id &, const bodypart_id & ) const );

        SET_FX_T( item_worn_with_id,
                  const item * ( const itype_id &, const bodypart_id & ) const );

        SET_FX_T( get_skill_level, int( const skill_id & ) const );

        SET_FX_T( get_all_skills, const SkillLevelMap & () const );
        SET_FX_T( get_skill_level_object, SkillLevel & ( const skill_id & ident ) );

        SET_FX_T( set_skill_level, void( const skill_id &, int ) );
        SET_FX_T( mod_skill_level, void( const skill_id &, int ) );

        SET_FX_T( rust_rate, int() const );

        SET_FX_T( practice, void( const skill_id &, int, int, bool ) );

        SET_FX_T( read_speed, int( bool ) const );

        SET_FX_T( get_time_died, time_point() const );

        SET_FX_T( is_rad_immune, bool() const );

        SET_FX_T( is_throw_immune, bool() const );

        SET_FX_T( rest_quality, float() const );

        SET_FX_T( healing_rate, float( float ) const );

        SET_FX_T( healing_rate_medicine, float( float, const bodypart_id & ) const );

        SET_FX_T( mutation_value, float( const std::string & ) const );

        SET_FX_T( get_base_traits, std::vector<trait_id>() const );

        SET_FX_T( get_mutations, std::vector<trait_id>( bool ) const );

        SET_FX_T( clear_skills, void() );

        SET_FX_T( clear_mutations, void() );

        SET_FX_T( crossed_threshold, bool() const );

        SET_FX_T( add_addiction, void( add_type, int ) );
        SET_FX_T( rem_addiction, void( add_type ) );
        SET_FX_T( has_addiction, bool( add_type ) const );
        SET_FX_T( addiction_level, int( add_type ) const );

        SET_FX_T( is_hauling, bool() const );

        DOC( "Adds a detached item to the player inventory" );
        luna::set_fx( ut, "add_item", []( UT_CLASS & c, detached_ptr<item> &i )
        {
            c.i_add( std::move( i ) );
        } );

        DOC( "Creates and an item with the given id and amount to the player inventory" );
        luna::set_fx( ut, "create_item", []( UT_CLASS & c, const itype_id & itype, int count )
        {
            return &c.add_item_with_id( itype, count );
        } );

        DOC( "DEPRECATED: use create_item instead" );
        SET_FX_T( add_item_with_id, item & ( const itype_id & itype, int count ) );

        DOC( "Checks for an item with the given id" );
        SET_FX_T( has_item_with_id, bool( const itype_id & itype, bool need_charges ) const );

        DOC( "Gets the first occurrence of an item with the given id" );
        SET_FX_T( get_item_with_id, const item * ( const itype_id & itype, bool need_charges ) const );

        DOC( "Checks for an item with the given flag" );
        SET_FX_T( has_item_with_flag, bool( const flag_id & flag, bool need_charges ) const );

        DOC( "Gets all items with the given flag" );
        SET_FX_T( all_items_with_flag,
                  std::vector<item *>( const flag_id & flag, bool need_charges ) const );

        DOC( "Gets all items" );
        SET_FX_T( all_items, std::vector<item *>( bool need_charges ) const );

        DOC( "Filters items" );
        luna::set_fx( ut, "items_with", &Character::items_with );

        DOC( "DEPRECATED: use remove_item instead" );
        luna::set_fx( ut, "inv_remove_item", &Character::inv_remove_item );

        DOC( "Removes given `Item` from character's inventory. The `Item` must be in the inventory, neither wielded nor worn." );
        luna::set_fx( ut, "remove_item", []( UT_CLASS & c, item & it ) -> detached_ptr<item> {
            return c.inv_remove_item( &it );
        } );

        DOC( "Checks if a given `Item` can be taken off." );
        luna::set_fx( ut, "can_takeoff", []( const UT_CLASS & c, const item & it )
        {
            const auto res = c.can_takeoff( it );
            return res.success() && res.value();
        } );

        DOC( "Attempts to take off the worn `Item` from character." );
        luna::set_fx( ut, "takeoff", []( UT_CLASS & c, item & it )
        {
            return c.takeoff( it, nullptr );
        } );

        DOC( "Attempts to remove the worn `Item` from character." );
        luna::set_fx( ut, "remove_worn", []( UT_CLASS & c, item & it ) -> std::optional<detached_ptr<item>> {
            std::vector<detached_ptr<item>> res{};
            if( c.takeoff( it, &res ) )
                return std::make_optional( std::move( res[0] ) );
            return std::nullopt;
        } );

        luna::set_fx( ut, "get_dependant_worn_items", []( const UT_CLASS & c, const item & it )
        {
            auto lst = c.get_dependent_worn_items( it );
            std::vector<item *> res = {};
            std::ranges::copy( lst, std::back_inserter( res ) );
            return res;
        } );

        // Could also use a std::variant<item*, detached_ptr<item>*> for a single method
        DOC( "Attempts to wear an item not in the creature inventory. If boolean parameter is false, item is worn instantly" );
        luna::set_fx( ut, "wear_detached", []( UT_CLASS & c, detached_ptr<item> &it, bool interactive )
        {
            return !!c.wear_item( std::move( it ), interactive, std::nullopt );
        } );

        DOC( "Attempts to wear an item in the creature inventory. If boolean parameter is false, item is worn instantly" );
        luna::set_fx( ut, "wear", []( UT_CLASS & c, item & it, bool interactive )
        {
            return c.wear_possessed( it, interactive, std::nullopt );
        } );

        DOC( "Checks if creature can wear a given item. If boolean parameter is true, ignores already worn items" );
        luna::set_fx( ut, "can_wear", []( const UT_CLASS & c, const item & it, bool ignore_worn )
        {
            auto res = c.can_wear( it, ignore_worn );
            return res.success() && res.value();
        } );

        luna::set_fx( ut, "get_worn_items", []( const UT_CLASS & c )
        {
            std::vector<item *> res{};
            std::ranges::copy( c.worn, std::back_inserter( res ) );
            return res;
        } );

        SET_FX_T( assign_activity,
                  void( const activity_id &, int, int, int, const std::string & ) );

        SET_FX_T( has_activity, bool( const activity_id & type ) const );

        SET_FX_T( cancel_activity, void() );

        SET_FX_T( metabolic_rate, float() const );

        SET_FX_T( base_age, int() const );
        SET_FX_T( set_base_age, void( int ) );
        SET_FX_T( mod_base_age, void( int ) );

        SET_FX_T( age, int() const );

        SET_FX_T( base_height, int() const );
        SET_FX_T( set_base_height, void( int ) );
        SET_FX_T( mod_base_height, void( int ) );

        SET_FX_T( height, int() const );

        SET_FX_T( bodyweight, units::mass() const );

        SET_FX_T( bionics_weight, units::mass() const );

        SET_FX_T( get_armor_acid, int( bodypart_id bp ) const );

        SET_FX_T( get_stim, int() const );
        SET_FX_T( set_stim, void( int ) );
        SET_FX_T( mod_stim, void( int ) );

        SET_FX_T( get_rad, int() const );
        SET_FX_T( set_rad, void( int ) );
        SET_FX_T( mod_rad, void( int ) );

        SET_FX_T( get_stamina, int() const );
        SET_FX_T( get_stamina_max, int() const );
        SET_FX_T( set_stamina, void( int ) );
        SET_FX_T( mod_stamina, void( int ) );

        SET_FX_T( sound_hallu, void() );

        SET_FX_T( wake_up, void() );

        SET_FX_T( get_shout_volume, int() const );

        SET_FX_T( shout, void( std::string, bool ) );

        SET_FX_T( vomit, void() );

        SET_FX_T( restore_scent, void() );

        SET_FX_T( mod_painkiller, void( int ) );

        SET_FX_T( set_painkiller, void( int ) );

        SET_FX_T( get_painkiller, int() const );

        SET_FX_T( spores, void() );
        SET_FX_T( blossoms, void() );

        SET_FX_T( rooted, void() );

        luna::set_fx( ut, "fall_asleep", sol::overload(
                          sol::resolve<void()>( &UT_CLASS::fall_asleep ),
                          sol::resolve<void( const time_duration &duration )>( &UT_CLASS::fall_asleep )
                      ) );

        SET_FX_T( get_hostile_creatures, std::vector<Creature *>( int ) const );

        SET_FX_T( get_visible_creatures, std::vector<Creature *>( int ) const );

        SET_FX_T( wearing_something_on, bool( const bodypart_id & ) const );

        SET_FX_T( is_wearing_helmet, bool() const );

        SET_FX_T( get_morale_level, int() const );
        SET_FX_T( add_morale,
                  void( const morale_type &, int, int, const time_duration &, const time_duration &,
                        bool, const itype * ) );
        SET_FX_T( has_morale, bool( const morale_type & ) const );
        SET_FX_T( get_morale, int( const morale_type & ) const );
        SET_FX_T( rem_morale, void( const morale_type & ) );
        SET_FX_T( clear_morale, void() );
        SET_FX_T( has_morale_to_read, bool() const );
        SET_FX_T( has_morale_to_craft, bool() const );

        luna::set_fx( ut, "knows_recipe", []( const UT_CLASS & utObj, const recipe_id & rec ) -> bool { return utObj.knows_recipe( &( rec.obj() ) ); } );
        luna::set_fx( ut, "learn_recipe", []( UT_CLASS & utObj, const recipe_id & rec ) -> void { utObj.learn_recipe( &( rec.obj() ) ); } );

        SET_FX_T( suffer, void() );

        SET_FX_T( irradiate, bool( float rads, bool bypass ) );

        SET_FX_T( can_hear, bool( const tripoint & source, int volume ) const );

        SET_FX_T( hearing_ability, float() const );

        SET_FX_T( get_lowest_hp, int() const );

        // Respawn Stuff
        SET_FX_T( drop_inv, void( const int count ) );

        DOC( "Drops all items (inventory, worn, wielded) at the character's current position." );
        luna::set_fx( ut, "drop_all_items", []( Character & ch ) -> void {
            std::vector<detached_ptr<item>> tmp = ch.inv_dump_remove();
            map &here = get_map();
            for( auto &itm : tmp )
            {
                here.add_item_or_charges( ch.pos(), std::move( itm ) );
            }
        } );

        SET_FX( bodypart_exposure );


        SET_FX( use_charges );
        SET_FX( use_charges_if_avail );

        DOC( "Returns the crafting inventory for this character (includes nearby items)" );
        luna::set_fx( ut, "crafting_inventory", []( UT_CLASS & ch ) -> const inventory & {
            return ch.crafting_inventory( tripoint_zero, PICKUP_RANGE, true );
        } );

        DOC( "Invalidates the cached crafting inventory" );
        SET_FX_T( invalidate_crafting_inventory, void() );

        DOC( "Consumes items from inventory based on item component list" );
        luna::set_fx( ut, "consume_items", []( UT_CLASS & ch, const std::vector<item_comp> &components ) -> void {
            ch.consume_items( components );
        } );

        DOC( "Consumes tool charges from inventory based on tool component list" );
        luna::set_fx( ut, "consume_tools", []( UT_CLASS & ch, const std::vector<tool_comp> &tools ) -> void {
            ch.consume_tools( tools );
        } );

    }
#undef UT_CLASS // #define UT_CLASS Character

#define UT_CLASS character_id
    {
        sol::usertype<UT_CLASS> ut =
        luna::new_usertype<UT_CLASS>(
            lua,
            luna::no_bases,
            luna::constructors <
            UT_CLASS(),
            UT_CLASS( int )
            > ()
        );

        SET_FX( is_valid );
        SET_FX( get_value );
    }
#undef UT_CLASS // #define UT_CLASS character_id
}

void cata::detail::reg_player( sol::state &lua )
{
    {
        // Note(AluminumAlman): skipping binding members and methods of this class because
        // most of the methods and members are already binded through Character.
        sol::usertype<player> ut =
            luna::new_usertype<player>(
                lua,
                luna::bases<Character, Creature>(),
                luna::no_constructor
            );
    }
}

void cata::detail::reg_npc( sol::state &lua )
{
#define UT_CLASS npc
    {
        sol::usertype<UT_CLASS> ut =
        luna::new_usertype<UT_CLASS>(
            lua,
            luna::bases<player, Character, Creature>(),
            luna::no_constructor
        );

        // Members
        SET_MEMB( current_activity_id );
        SET_MEMB( personality );
        SET_MEMB( op_of_u );
        SET_MEMB( patience );
        SET_MEMB( marked_for_death );
        SET_MEMB( hit_by_player );
        SET_MEMB( needs );

        // Methods
        SET_FX_N_T( set_fac, "set_faction_id", void( const faction_id & id ) );

        SET_FX_T( turned_hostile, bool() const );

        SET_FX_T( hostile_anger_level, int() const );

        SET_FX_T( make_angry, void() );

        SET_FX_T( is_enemy, bool() const );

        SET_FX_T( is_following, bool() const );
        SET_FX_T( is_obeying, bool( const Character & p ) const );

        SET_FX_T( is_friendly, bool( const Character & p ) const );

        SET_FX_T( is_leader, bool() const );

        SET_FX_T( is_walking_with, bool() const );

        SET_FX_T( is_ally, bool( const Character & p ) const );

        SET_FX_T( is_player_ally, bool() const );

        SET_FX_T( is_stationary, bool( bool include_guards ) const );

        SET_FX_T( is_guarding, bool() const );

        SET_FX_T( is_patrolling, bool() const );

        SET_FX_T( has_player_activity, bool() const );
        SET_FX_T( is_travelling, bool() const );

        SET_FX_T( is_minion, bool() const );

        SET_FX_T( guaranteed_hostile, bool() const );

        SET_FX_T( mutiny, void() );

        SET_FX_T( get_monster_faction, mfaction_id() const );

        SET_FX_T( follow_distance, int() const );

        SET_FX_T( current_target, Creature * () );
        SET_FX_T( current_ally, Creature * () );

        SET_FX_T( danger_assessment, float() );

        luna::set_fx( ut, "say", &UT_CLASS::say<> );

        SET_FX_T( smash_ability, int() const );

        luna::set_fx( ut, "complain_about",
        []( UT_CLASS & npchar, const std::string & issue, const time_duration & dur, const std::string & speech, sol::optional<bool> force ) -> bool {
            return npchar.complain_about( issue, dur, speech, force.value_or( false ) );
        } );

        SET_FX_T( warn_about,
                  void( const std::string & type, const time_duration &, const std::string &,
                        int, const tripoint & ) );

        SET_FX_T( complain, bool() );

        SET_FX_T( evaluate_enemy, float( const Creature & ) const );

        SET_FX_T( can_open_door, bool( const tripoint &, bool ) const );
        SET_FX_T( can_move_to, bool( const tripoint &, bool ) const );

        SET_FX_T( saw_player_recently, bool() const );

        SET_FX_T( has_omt_destination, bool() const );

        SET_FX_T( get_attitude, npc_attitude() const );
        SET_FX_T( set_attitude, void( npc_attitude new_attitude ) );
        SET_FX_T( has_activity, bool() const );
    }
#undef UT_CLASS // #define UT_CLASS npc

#define UT_CLASS npc_personality
    {
        sol::usertype<UT_CLASS> ut =
        luna::new_usertype<UT_CLASS>(
            lua,
            luna::no_bases,
            luna::constructors <
            UT_CLASS()
            > ()
        );
        SET_MEMB( aggression );
        SET_MEMB( bravery );
        SET_MEMB( collector );
        SET_MEMB( altruism );
    }
#undef UT_CLASS // #define UT_CLASS npc_personality

#define UT_CLASS npc_opinion
    {
        sol::usertype<UT_CLASS> ut =
        luna::new_usertype<UT_CLASS>(
            lua,
            luna::no_bases,
            luna::constructors <
            UT_CLASS(),
            UT_CLASS( int, int, int, int, int )
            > ()
        );
        SET_MEMB( trust );
        SET_MEMB( fear );
        SET_MEMB( value );
        SET_MEMB( anger );
        SET_MEMB( owed );
    }
#undef UT_CLASS // #define UT_CLASS npc_opinion
}

void cata::detail::reg_avatar( sol::state &lua )
{
#define UT_CLASS avatar
    {
        // Note(AluminumAlman): skipping binding members and methods of this class because
        // most of the methods and members are already binded through Character.
        sol::usertype<avatar> ut =
        luna::new_usertype<avatar>(
            lua,
            luna::bases<player, Character, Creature>(),
            luna::no_constructor
        );

        SET_FX_T( get_active_missions, std::vector<mission *>() const );
        SET_FX_T( get_completed_missions, std::vector<mission *>() const );
        SET_FX_T( get_failed_missions, std::vector<mission *>() const );

    }
#undef UT_CLASS // #define UT_CLASS npc_opinion
}
