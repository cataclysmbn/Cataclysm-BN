#include "bodypart.h"
#include "bionics.h"
#include "catalua_bindings.h"
#include "catalua_bindings_utils.h"
#include "catalua_luna_doc.h"
#include "damage.h"
#include "faction.h"
#include "itype.h"
#include "magic/magic.h"
#include "map/mapdata.h"
#include "material.h"
#include "mission.h"
#include "mutation.h"
#include "mtype.h"
#include "perk.h"
#include "scenario.h"
#include "type_id.h"
#include "units_volume.h"
#include "vitamin.h"

#include <algorithm>
#include <iterator>
#include <ranges>
#include <set>
#include <string>
#include <utility>
#include <vector>

auto cata::detail::reg_types( sol::state &lua ) -> void
{
    {
        auto ut = luna::new_usertype<faction>( lua, luna::no_bases, luna::no_constructor );

        luna::set_fx( ut, "str_id", []( const faction & x ) -> faction_id { return x.id; } );

        // Factions are a pain because they _inherit_ from their type, not reference it by id.
        // This causes various weirdness, so let's omit the fields for now.
    }

#define UT_CLASS scenario
    {
        auto ut = luna::new_usertype<scenario>( lua, luna::no_bases, luna::no_constructor );

        luna::set_prop( ut, "str_id", []( const UT_CLASS & x ) -> string_id<scenario> { return x.ident(); } );
        DOC( "String id of the scenario, e.g. \"evacuee\". Useful for equality checks." );
        luna::set_prop( ut, "id", []( const UT_CLASS & x ) -> std::string { return x.ident().str(); } );
        DOC( "Display name of the scenario for the given gender (true = male)." );
        luna::set_fx( ut, "name", &UT_CLASS::gender_appropriate_name );
        DOC( "Description of the scenario for the given gender (true = male)." );
        luna::set_fx( ut, "description", &UT_CLASS::description );
        DOC( "Name of the starting situation shown during character creation." );
        luna::set_prop( ut, "start_name", []( const UT_CLASS & x ) -> std::string { return x.start_name(); } );
        DOC( "Point cost of the scenario." );
        luna::set_prop( ut, "point_cost", []( const UT_CLASS & x ) -> int { return x.point_cost(); } );
        DOC( "Traits forced by the scenario, granted for free during character creation." );
        luna::set_fx( ut, "forced_traits", []( const UT_CLASS & x ) -> std::vector<trait_id> {
            return std::ranges::to<std::vector<trait_id>>( x.get_locked_traits() );
        } );
        DOC( "Allowed traits, empty meaning all starting traits are allowed." );
        luna::set_fx( ut, "allowed_traits", []( const UT_CLASS & x ) -> std::vector<trait_id> {
            return x.allowed_traits() | std::ranges::to<std::vector<trait_id>>();
        } );
        DOC( "Traits forbidden in this scenario." );
        luna::set_fx( ut, "forbidden_traits", []( const UT_CLASS & x ) -> std::vector<trait_id> {
            return x.forbidden_traits() | std::ranges::to<std::vector<trait_id>>();
        } );
        DOC( "Bionics forced by the scenario." );
        luna::set_fx( ut, "forced_bionics", []( const UT_CLASS & x ) -> std::vector<bionic_id> {
            return std::ranges::to<std::vector<bionic_id>>( x.get_locked_bionics() );
        } );
        DOC( "Allowed bionics, empty meaning all starting bionics are allowed." );
        luna::set_fx( ut, "allowed_bionics", []( const UT_CLASS & x ) -> std::vector<bionic_id> {
            return x.allowed_bionics() | std::ranges::to<std::vector<bionic_id>>();
        } );
        DOC( "Bionics forbidden in this scenario." );
        luna::set_fx( ut, "forbidden_bionics", []( const UT_CLASS & x ) -> std::vector<bionic_id> {
            return x.forbidden_bionics() | std::ranges::to<std::vector<bionic_id>>();
        } );
        DOC( "Spells forced by the scenario." );
        luna::set_fx( ut, "forced_spells", []( const UT_CLASS & x ) -> std::vector<spell_id> {
            return x.forced_spells() | std::ranges::to<std::vector<spell_id>>();
        } );
        DOC( "Allowed spells, empty meaning all starting spells are allowed." );
        luna::set_fx( ut, "allowed_spells", []( const UT_CLASS & x ) -> std::vector<spell_id> {
            return x.allowed_spells() | std::ranges::to<std::vector<spell_id>>();
        } );
        DOC( "Spells forbidden in this scenario." );
        luna::set_fx( ut, "forbidden_spells", []( const UT_CLASS & x ) -> std::vector<spell_id> {
            return x.forbidden_spells() | std::ranges::to<std::vector<spell_id>>();
        } );
        DOC( "Perks forced by the scenario." );
        luna::set_fx( ut, "perks", []( const UT_CLASS & x ) -> std::vector<perk_id> {
            return std::ranges::to<std::vector<perk_id>>( x.get_perks() );
        } );
        DOC( "Mission types assigned at the start of the game." );
        luna::set_fx( ut, "missions", []( const UT_CLASS & x ) -> std::vector<mission_type_id> {
            return x.missions() | std::ranges::to<std::vector<mission_type_id>>();
        } );
        DOC( "All scenario flags, e.g. \"FIRE_START\"." );
        luna::set_fx( ut, "get_flags", []( const UT_CLASS & x ) -> std::vector<std::string> {
            return x.get_flags() | std::ranges::to<std::vector<std::string>>();
        } );
        DOC( "Returns true if the scenario has the given flag." );
        luna::set_fx( ut, "has_flag", &UT_CLASS::has_flag );
        DOC( "String ids of the starting locations allowed by this scenario." );
        luna::set_fx( ut, "allowed_start_locations", []( const UT_CLASS & x ) -> std::vector<std::string> {
            return x.allowed_start_locations()
            | std::views::transform( []( const start_location_id & loc ) { return loc.str(); } )
            | std::ranges::to<std::vector<std::string>>();
        } );
        DOC( "String id of the starting vehicle, or an empty string if the scenario has none." );
        luna::set_prop( ut, "starting_vehicle",
                        []( const UT_CLASS & x ) -> std::string { return x.vehicle().str(); } );
        DOC( "Returns true if the scenario has a starting map extra." );
        luna::set_prop( ut, "has_map_extra", []( const UT_CLASS & x ) -> bool { return x.has_map_extra(); } );
        DOC( "String id of the starting map extra, or an empty string if the scenario has none." );
        luna::set_prop( ut, "map_extra",
                        []( const UT_CLASS & x ) -> std::string { return x.get_map_extra().str(); } );
        DOC( "String ids of professions permitted by this scenario." );
        luna::set_fx( ut, "permitted_professions", []( const UT_CLASS & x ) -> std::vector<std::string> {
            const auto profs = x.permitted_professions();
            return profs
            | std::views::transform( []( const profession_id & prof ) { return prof.str(); } )
            | std::ranges::to<std::vector<std::string>>();
        } );
        DOC( "Returns true if the scenario is blacklisted." );
        luna::set_prop( ut, "is_blacklisted", []( const UT_CLASS & x ) -> bool { return x.scen_is_blacklisted(); } );
        DOC( "Returns true if the given trait is allowed in this scenario." );
        luna::set_fx( ut, "traitquery", &UT_CLASS::traitquery );
        DOC( "Returns true if the given trait is forced by this scenario." );
        luna::set_fx( ut, "is_locked_trait", &UT_CLASS::is_locked_trait );
        DOC( "Returns true if the given trait is forbidden in this scenario." );
        luna::set_fx( ut, "is_forbidden_trait", &UT_CLASS::is_forbidden_trait );
        DOC( "Returns true if the given bionic is allowed in this scenario." );
        luna::set_fx( ut, "bionicquery", &UT_CLASS::bionicquery );
        DOC( "Returns true if the given bionic is forced by this scenario." );
        luna::set_fx( ut, "is_locked_bionic", &UT_CLASS::is_locked_bionic );
        DOC( "Returns true if the given bionic is forbidden in this scenario." );
        luna::set_fx( ut, "is_forbidden_bionic", &UT_CLASS::is_forbidden_bionic );
        DOC( "Returns true if the given spell is allowed in this scenario." );
        luna::set_fx( ut, "spellquery", &UT_CLASS::spellquery );
        DOC( "Returns true if the given spell is forbidden in this scenario." );
        luna::set_fx( ut, "is_forbidden_spell", &UT_CLASS::is_forbidden_spell );
        DOC( "Returns true if the scenario forbids bionics outright." );
        luna::set_prop( ut, "forbids_bionics", []( const UT_CLASS & x ) -> bool { return x.forbids_bionics(); } );
        DOC( "Returns true if the scenario forbids spells outright." );
        luna::set_prop( ut, "forbids_spells", []( const UT_CLASS & x ) -> bool { return x.forbids_spells(); } );
        DOC( "Returns true if the given start location is allowed by this scenario." );
        luna::set_fx( ut, "allowed_start", &UT_CLASS::allowed_start );
        DOC( "Monster groups surrounding the start: array of (mongroup_id, chance) pairs." );
        luna::set_fx( ut, "surround_groups",
        []( const UT_CLASS & x ) -> std::vector<std::pair<mongroup_id, float>> {
            return x.surround_groups() | std::ranges::to<std::vector<std::pair<mongroup_id, float>>>();
        } );
    }
#undef UT_CLASS

#define UT_CLASS material_type
    {
        auto ut = luna::new_usertype<material_type>( lua, luna::no_bases, luna::no_constructor );

        luna::set_fx( ut, "str_id", &material_type::ident );
        SET_FX( name );
    }
#undef UT_CLASS

#define UT_CLASS mtype
    {
        auto ut = luna::new_usertype<mtype>( lua, luna::no_bases, luna::no_constructor );

        SET_MEMB_RO( id );
        SET_MEMB_RO( difficulty );
        SET_MEMB_RO( difficulty_base );
        DOC( "Number of dice of bonus bashing damage on melee hit." );
        SET_MEMB_RO( melee_dice );
        DOC( "Number of sides of the melee bonus damage dice." );
        SET_MEMB_RO( melee_sides );
        DOC( "Base melee attack damage." );
        SET_MEMB_RO( melee_damage );
        DOC( "Intensity of the grabbed effect this monster type applies." );
        SET_MEMB_RO( grab_strength );
        SET_FX_T( nname, std::string( unsigned int ) const );
    }
#undef UT_CLASS

#define UT_CLASS ter_t
    {
        auto ut = luna::new_usertype<ter_t>( lua, luna::no_bases, luna::no_constructor );

        luna::set_fx( ut, "str_id", []( const ter_t & x ) -> ter_str_id { return x.id; } );
        luna::set_fx( ut, "int_id", []( const ter_t & x ) -> ter_id { return x.id.id(); } );

        SET_FX( name );
        SET_FX( get_flags );
        SET_FX_T( has_flag, bool( const std::string & ) const );
        SET_FX( set_flag );
        luna::set_fx( ut, "get_light_emitted", []( ter_t & t ) -> int { return t.light_emitted; } );
        luna::set_fx( ut, "set_light_emitted", []( ter_t & t, int val ) -> void { t.light_emitted = val; } );
        luna::set_fx( ut, "get_movecost", []( ter_t & t ) -> int { return t.movecost; } );
        luna::set_fx( ut, "set_movecost", []( ter_t & t, int val ) -> void { t.movecost = val; } );
        luna::set_fx( ut, "get_coverage", []( ter_t & t ) -> int { return t.coverage; } );
        luna::set_fx( ut, "set_coverage", []( ter_t & t, int val ) -> void { t.coverage = val; } );
        luna::set_fx( ut, "get_max_volume", []( ter_t & t ) -> units::volume { return t.max_volume; } );
        luna::set_fx( ut, "set_max_volume", []( ter_t & t, units::volume val ) -> void { t.max_volume = val; } );
        SET_MEMB( open );
        SET_MEMB( close );
        SET_MEMB( trap_id_str );
        SET_MEMB( transforms_into );
        SET_MEMB( roof );
        SET_MEMB( heat_radiation );
    }
#undef UT_CLASS

#define UT_CLASS furn_t
    {
        auto ut = luna::new_usertype<furn_t>( lua, luna::no_bases, luna::no_constructor );

        luna::set_fx( ut, "str_id", []( const furn_t &x ) -> furn_str_id { return x.id; } );
        luna::set_fx( ut, "int_id", []( const furn_t &x ) -> furn_id { return x.id.id(); } );

        SET_FX( name );
        SET_FX( get_flags );
        SET_FX_T( has_flag, bool( const std::string & ) const );
        SET_FX( set_flag );
        luna::set_fx( ut, "get_light_emitted", []( furn_t &f ) -> int { return f.light_emitted; } );
        luna::set_fx( ut, "set_light_emitted", []( furn_t &f, int val ) -> void { f.light_emitted = val; } );

        luna::set_fx( ut, "get_movecost", []( furn_t &f ) -> int { return f.movecost; } );
        luna::set_fx( ut, "set_movecost", []( furn_t &f, int val ) -> void { f.movecost = val; } );

        luna::set_fx( ut, "get_coverage", []( furn_t &f ) -> int { return f.coverage; } );
        luna::set_fx( ut, "set_coverage", []( furn_t &f, int val ) -> void { f.coverage = val; } );

        luna::set_fx( ut, "get_max_volume", []( furn_t &f ) -> units::volume { return f.max_volume; } );
        luna::set_fx( ut, "set_max_volume", []( furn_t &f, units::volume val ) -> void { f.max_volume = val; } );
        SET_MEMB( open );
        SET_MEMB( close );
        SET_MEMB( transforms_into );
    }
#undef UT_CLASS

#define UT_CLASS armor_portion_data
    {
        auto ut = luna::new_usertype<UT_CLASS>( lua, luna::no_bases, luna::no_constructor );

        SET_MEMB_RO( coverage );
        luna::set_fx( ut, "get_covered_parts", []( const UT_CLASS & c ) {
            auto ret = std::set<bodypart_id> {};
            std::ranges::transform( c.covers, std::inserter( ret, ret.end() ),
            []( const bodypart_str_id & v ) -> bodypart_id {
                return bodypart_id( v );
            } );
            return ret;
        } );
        SET_MEMB_RO( encumber );
        SET_MEMB_RO( max_encumber );
    }
#undef UT_CLASS

#define UT_CLASS resistances
    {
        auto ut = luna::new_usertype<UT_CLASS>( lua, luna::no_bases, luna::no_constructor );

        //SET_FX(combined_with);
        DOC( "Returns flat resistance values keyed by damage type. Damage types without resistance may be absent." );
        luna::set_fx( ut, "get_all_resist", []( const UT_CLASS & c ) { return c.flat; } );
        SET_FX_N( type_resist, "get_resist" );
        SET_FX( get_effective_resist );
        //SET_FX(set_resist);
    }
#undef UT_CLASS

#define UT_CLASS vitamin
    {
        auto ut = luna::new_usertype<UT_CLASS>( lua, luna::no_bases, luna::no_constructor );

        SET_FX( deficiency );
        SET_FX( excess );
        SET_FX( has_flag );
        SET_FX( min );
        SET_FX( max );
        SET_FX( rate );
        SET_FX( severity );
        SET_FX( name );
        SET_FX( is_null );
        SET_FX_N( id, "vitamin_id" );
        SET_FX_N( type, "vitamin_type" );
    }
#undef UT_CLASS
}
