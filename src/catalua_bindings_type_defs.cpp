#include "catalua_bindings.h"

#include "catalua_bindings_utils.h"
#include "catalua_luna_doc.h"

#include "bodypart.h"
#include "damage.h"
#include "faction.h"
#include "itype.h"
#include "mapdata.h"
#include "material.h"
#include "mtype.h"
#include "type_id.h"
#include "units_volume.h"
#include "vitamin.h"

#include <algorithm>
#include <iterator>
#include <set>
#include <string>

auto cata::detail::reg_types( sol::state &lua ) -> void
{
    {
        auto ut = luna::new_usertype<faction>( lua, luna::no_bases, luna::no_constructor );

        luna::set_fx( ut, "str_id", []( const faction & x ) -> faction_id { return x.id; } );

        // Factions are a pain because they _inherit_ from their type, not reference it by id.
        // This causes various weirdness, so let's omit the fields for now.
    }

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
