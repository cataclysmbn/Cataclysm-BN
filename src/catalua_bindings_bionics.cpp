#include "catalua_bindings.h"

#include "catalua.h"
#include "catalua_bindings_utils.h"
#include "catalua_impl.h"
#include "catalua_log.h"
#include "catalua_luna.h"
#include "catalua_luna_doc.h"
#include "type_id.h"

#include "bionics.h"
#include "character.h"
#include "character_stat.h"
#include "flag.h"
#include "make_static.h"

void cata::detail::reg_bionics( sol::state &lua )
{
#define UT_CLASS bionic
    {
        sol::usertype<UT_CLASS> ut =
        luna::new_usertype<UT_CLASS>(
            lua,
            luna::no_bases,
            luna::no_constructor
        );

        // Read-only member variables
        luna::set_fx( ut, "id", []( const UT_CLASS & bio ) -> bionic_id {
            return bio.id;
        } );
        SET_MEMB_RO( charge_timer );
        SET_MEMB_RO( invlet );
        SET_MEMB_RO( powered );
        SET_MEMB_RO( show_sprite );
        luna::set_fx( ut, "ammo_loaded", []( const UT_CLASS & bio ) -> std::string {
            return bio.ammo_loaded.str();
        } );
        SET_MEMB_RO( ammo_count );
        SET_MEMB_RO( incapacitated_time );
        SET_MEMB_RO( energy_stored );

        // info() - get the bionic_data definition for this installed bionic
        DOC( "Returns the BionicData definition for this bionic." );
        luna::set_fx( ut, "info", []( const UT_CLASS & bio ) -> const bionic_data * {
            return &bio.info();
        } );

        // Flag methods
        luna::set_fx( ut, "set_flag", []( UT_CLASS & bio, const std::string & flag )
        {
            bio.set_flag( flag );
        } );
        luna::set_fx( ut, "remove_flag", []( UT_CLASS & bio, const std::string & flag )
        {
            bio.remove_flag( flag );
        } );
        luna::set_fx( ut, "has_flag", []( UT_CLASS & bio, const std::string & flag ) -> bool {
            return bio.has_flag( flag );
        } );

        // Quality method
        luna::set_fx( ut, "get_quality", []( UT_CLASS & bio, const std::string & quality_id ) -> int {
            return bio.get_quality( string_id<quality>( quality_id ) );
        } );

        // Fuel method
        luna::set_fx( ut, "is_fueled_by", []( UT_CLASS & bio, const std::string & fuel_id ) -> bool {
            return bio.is_this_fuel_powered( string_id<itype>( fuel_id ) );
        } );

        // Toggle/auto-start methods
        SET_FX( toggle_safe_fuel_mod );
        SET_FX( toggle_auto_start_mod );
        SET_FX_T( set_auto_start_thresh, void( float ) );
        SET_FX_T( get_auto_start_thresh, float() const );
        SET_FX_T( is_auto_start_on, bool() const );
        SET_FX_T( is_auto_start_keep_full, bool() const );

        // Activate/deactivate on a character
        DOC( "Activates this bionic on the given character. Optional block_message (default true) suppresses messages." );
        luna::set_fx( ut, "activate", []( UT_CLASS & bio, Character & ch,
        std::optional<bool> block_message ) -> bool {
            if( !ch.has_bionic( bio.id ) )
            {
                return false;
            }
            bionic &real_bio = ch.get_bionic_state( bio.id );
            real_bio.powered = real_bio.info().has_flag( STATIC( flag_id( "BIONIC_TOGGLED" ) ) ) ||
            real_bio.info().charge_time > 0;
            if( real_bio.info().charge_time > 0 )
            {
                real_bio.charge_timer = real_bio.info().charge_time;
            }
            if( !real_bio.id->enchantments.empty() )
            {
                ch.recalculate_enchantment_cache();
            }
            return ch.activate_bionic( real_bio, block_message.value_or( true ) );
        } );

        DOC( "Deactivates this bionic on the given character. Optional block_message (default true) suppresses messages." );
        luna::set_fx( ut, "deactivate", []( UT_CLASS & bio, Character & ch,
        std::optional<bool> block_message ) -> bool {
            if( !ch.has_bionic( bio.id ) )
            {
                return false;
            }
            bionic &real_bio = ch.get_bionic_state( bio.id );
            return ch.deactivate_bionic( real_bio, block_message.value_or( true ) );
        } );

        // to_string
        luna::set_fx( ut, sol::meta_function::to_string,
        []( const UT_CLASS & bio ) -> std::string {
            return string_format( "%s[%s]",
                                  luna::detail::luna_traits<UT_CLASS>::name, bio.id.c_str() );
        } );
    }
#undef UT_CLASS
}

void cata::detail::mod_bionic_data( sol::state &lua )
{
#define UT_CLASS bionic_data
    {
        sol::usertype<UT_CLASS> ut =
        luna::new_usertype<UT_CLASS>(
            lua,
            luna::no_bases,
            luna::no_constructor
        );

        // ID (return as string to avoid sol2 automagical issues)
        luna::set_fx( ut, "id", []( const UT_CLASS & bd ) -> bionic_id {
            return bd.id;
        } );

        // Name and description (translation type -> exposed as string)
        DOC( "Returns the translated name of this bionic." );
        luna::set_fx( ut, "name", []( const UT_CLASS & bd ) -> std::string {
            return bd.name.translated();
        } );
        DOC( "Returns the translated description of this bionic." );
        luna::set_fx( ut, "description", []( const UT_CLASS & bd ) -> std::string {
            return bd.description.translated();
        } );

        // Power costs (units::energy, registered as LUNA_VAL)
        SET_MEMB_RO( power_activate );
        SET_MEMB_RO( power_deactivate );
        SET_MEMB_RO( power_over_time );
        SET_MEMB_RO( power_trigger );
        SET_MEMB_RO( kcal_trigger );
        SET_MEMB_RO( charge_time );
        SET_MEMB_RO( capacity );

        // Activation/state flags
        SET_MEMB_RO( activated );
        SET_MEMB_RO( included );

        // Weight modifiers
        SET_MEMB_RO( weight_capacity_modifier );
        SET_MEMB_RO( weight_capacity_bonus );

        // Stat bonuses (character_stat not in lua, convert to string keys)
        DOC( "Returns stat bonuses as a table mapping stat name to bonus value." );
        luna::set_fx( ut, "stat_bonus", []( const UT_CLASS & bd ) -> std::map<std::string, int> {
            std::map<std::string, int> rv;
            for( const auto &pair : bd.stat_bonus )
            {
                rv[get_stat_name( pair.first )] = pair.second;
            }
            return rv;
        } );

        // Remote fuel
        SET_MEMB_RO( is_remote_fueled );
        SET_MEMB_RO( remote_fuel_draw );

        // Fuel options (return as strings)
        DOC( "Returns fuel type IDs that can power this bionic." );
        luna::set_fx( ut, "fuel_opts", []( const UT_CLASS & bd ) -> std::vector<std::string> {
            std::vector<std::string> rv;
            rv.reserve( bd.fuel_opts.size() );
            for( const auto &f : bd.fuel_opts )
            {
                rv.push_back( f.str() );
            }
            return rv;
        } );
        SET_MEMB_RO( fuel_capacity );
        SET_MEMB_RO( fuel_efficiency );
        SET_MEMB_RO( fuel_multiplier );
        SET_MEMB_RO( passive_fuel_efficiency );

        // Coverage power gen penalty (optional<float>)
        DOC( "Returns the coverage power gen penalty, or nil if not set." );
        luna::set_fx( ut, "coverage_power_gen_penalty",
        []( const UT_CLASS & bd ) -> sol::optional<float> {
            if( bd.coverage_power_gen_penalty.has_value() )
            {
                return bd.coverage_power_gen_penalty.value();
            }
            return sol::nullopt;
        } );

        SET_MEMB_RO( exothermic_power_gen );
        luna::set_fx( ut, "power_gen_emission", []( const UT_CLASS & bd ) -> std::string {
            return bd.power_gen_emission.str();
        } );

        // Protection maps (return as string-keyed maps)
        DOC( "Environmental protection by body part." );
        luna::set_fx( ut, "env_protec", []( const UT_CLASS & bd ) -> std::map<std::string, int> {
            std::map<std::string, int> rv;
            for( const auto &p : bd.env_protec )
            {
                rv[p.first.str()] = p.second;
            }
            return rv;
        } );
        DOC( "Bash protection by body part." );
        luna::set_fx( ut, "bash_protec", []( const UT_CLASS & bd ) -> std::map<std::string, int> {
            std::map<std::string, int> rv;
            for( const auto &p : bd.bash_protec )
            {
                rv[p.first.str()] = p.second;
            }
            return rv;
        } );
        DOC( "Cut protection by body part." );
        luna::set_fx( ut, "cut_protec", []( const UT_CLASS & bd ) -> std::map<std::string, int> {
            std::map<std::string, int> rv;
            for( const auto &p : bd.cut_protec )
            {
                rv[p.first.str()] = p.second;
            }
            return rv;
        } );
        DOC( "Bullet protection by body part." );
        luna::set_fx( ut, "bullet_protec", []( const UT_CLASS & bd ) -> std::map<std::string, int> {
            std::map<std::string, int> rv;
            for( const auto &p : bd.bullet_protec )
            {
                rv[p.first.str()] = p.second;
            }
            return rv;
        } );

        // Body part slot/encumbrance maps (return as string-keyed maps)
        DOC( "Body parts occupied by this bionic and slot space required." );
        luna::set_fx( ut, "occupied_bodyparts", []( const UT_CLASS & bd ) -> std::map<std::string, int> {
            std::map<std::string, int> rv;
            for( const auto &p : bd.occupied_bodyparts )
            {
                rv[p.first.str()] = p.second;
            }
            return rv;
        } );
        DOC( "Encumbrance caused by this bionic on each body part." );
        luna::set_fx( ut, "encumbrance", []( const UT_CLASS & bd ) -> std::map<std::string, int> {
            std::map<std::string, int> rv;
            for( const auto &p : bd.encumbrance )
            {
                rv[p.first.str()] = p.second;
            }
            return rv;
        } );

        // Fake item (return as string)
        luna::set_fx( ut, "fake_item", []( const UT_CLASS & bd ) -> std::string {
            return bd.fake_item.str();
        } );

        // Canceled mutations (return as strings)
        DOC( "Mutations/traits removed when this bionic is installed." );
        luna::set_fx( ut, "canceled_mutations", []( const UT_CLASS & bd ) -> std::vector<std::string> {
            std::vector<std::string> rv;
            rv.reserve( bd.canceled_mutations.size() );
            for( const auto &m : bd.canceled_mutations )
            {
                rv.push_back( m.str() );
            }
            return rv;
        } );

        // Enchantments (return as strings)
        DOC( "Returns enchantment IDs as string values." );
        luna::set_fx( ut, "enchantments", []( const UT_CLASS & bd ) -> std::vector<std::string> {
            std::vector<std::string> rv;
            rv.reserve( bd.enchantments.size() );
            for( const auto &eid : bd.enchantments )
            {
                rv.push_back( eid.str() );
            }
            return rv;
        } );

        // Learned spells (return as string-keyed map)
        DOC( "Spells learned when this bionic is installed, mapped to level." );
        luna::set_fx( ut, "learned_spells", []( const UT_CLASS & bd ) -> std::map<std::string, int> {
            std::map<std::string, int> rv;
            for( const auto &p : bd.learned_spells )
            {
                rv[p.first.str()] = p.second;
            }
            return rv;
        } );

        // Related bionics (return as strings)
        DOC( "Bionics automatically installed alongside this one." );
        luna::set_fx( ut, "included_bionics", []( const UT_CLASS & bd ) -> std::vector<std::string> {
            std::vector<std::string> rv;
            rv.reserve( bd.included_bionics.size() );
            for( const auto &b : bd.included_bionics )
            {
                rv.push_back( b.str() );
            }
            return rv;
        } );

        luna::set_fx( ut, "upgraded_bionic", []( const UT_CLASS & bd ) -> std::string {
            return bd.upgraded_bionic.str();
        } );

        DOC( "Available upgrade paths for this bionic." );
        luna::set_fx( ut, "available_upgrades", []( const UT_CLASS & bd ) -> std::vector<std::string> {
            std::vector<std::string> rv;
            for( const auto &b : bd.available_upgrades )
            {
                rv.push_back( b.str() );
            }
            return rv;
        } );

        DOC( "Other bionics required for this one to be installed." );
        luna::set_fx( ut, "required_bionics", []( const UT_CLASS & bd ) -> std::vector<std::string> {
            std::vector<std::string> rv;
            rv.reserve( bd.required_bionics.size() );
            for( const auto &b : bd.required_bionics )
            {
                rv.push_back( b.str() );
            }
            return rv;
        } );

        // Installation properties
        SET_MEMB_RO( can_uninstall );
        SET_MEMB_RO( no_uninstall_reason );
        SET_MEMB_RO( starting_bionic );
        SET_MEMB_RO( points );

        // Flags (return as strings)
        DOC( "Returns the set of flag IDs on this bionic definition." );
        luna::set_fx( ut, "flags", []( const UT_CLASS & bd ) -> std::vector<std::string> {
            std::vector<std::string> rv;
            for( const auto &f : bd.flags )
            {
                rv.push_back( f.str() );
            }
            return rv;
        } );
        DOC( "Checks whether this bionic has a specific flag." );
        luna::set_fx( ut, "has_flag", []( const UT_CLASS & bd, const std::string & flag ) -> bool {
            return bd.has_flag( flag_id( flag ) );
        } );

        // Static: get all bionic definitions
        DOC( "Returns a list of every bionic definition in the game." );
        SET_FX_T( get_all, std::vector<bionic_data>() );

        // to_string
        luna::set_fx( ut, sol::meta_function::to_string,
        []( const UT_CLASS & bd ) -> std::string {
            return string_format( "%s[%s]",
                                  luna::detail::luna_traits<UT_CLASS>::name, bd.id.c_str() );
        } );
    }
#undef UT_CLASS
}
