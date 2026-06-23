#include <ranges>
#include <string>
#include <vector>

#include "catalua_bindings.h"
#include "catalua_coord.h"
#include "catalua.h"
#include "catalua_bindings_utils.h"
#include "catalua_luna.h"
#include "catalua_luna_doc.h"

#include "monstergenerator.h"
#include "mongroup.h"

auto cata::detail::reg_monster_groups( sol::state &lua ) -> void
{
#define UT_CLASS MonsterGroupEntry
    {
        auto ut = luna::new_usertype<UT_CLASS>(
            lua,
            luna::no_bases,
            luna::no_constructor
        );

        SET_MEMB( name );
        SET_MEMB( frequency );
        SET_MEMB( cost_multiplier );
        SET_MEMB( pack_minimum );
        SET_MEMB( pack_maximum );
        SET_MEMB( conditions );
        SET_MEMB( starts );
        SET_MEMB( ends );
        luna::set_fx( ut, "lasts_forever", []( const UT_CLASS & entry ) -> bool {
            return entry.lasts_forever();
        } );
    }
#undef UT_CLASS

#define UT_CLASS MonsterGroup
    {
        auto ut = luna::new_usertype<UT_CLASS>(
            lua,
            luna::no_bases,
            luna::no_constructor
        );

        SET_MEMB( name );
        SET_MEMB( defaultMonster );
        SET_MEMB( monsters );
        SET_MEMB( is_animal );
        SET_MEMB( replace_monster_group );
        SET_MEMB( new_monster_group );
        SET_MEMB( monster_group_time );
        SET_MEMB( is_safe );
        SET_MEMB( freq_total );
        DOC( "Check if a monster type appears in this group." );
        luna::set_fx( ut, "contains",
        []( const UT_CLASS & group, const mtype_id & type_id ) -> bool {
            return group.IsMonsterInGroup( type_id );
        } );
    }
#undef UT_CLASS

#define UT_CLASS mongroup
    {
        auto ut = luna::new_usertype<UT_CLASS>(
            lua,
            luna::no_bases,
            luna::no_constructor
        );

        SET_MEMB( type );
        SET_MEMB( radius );
        SET_MEMB( population );
        SET_MEMB( interest );
        SET_MEMB( horde );
        SET_MEMB( horde_behaviour );
        SET_MEMB( diffuse );

        DOC( "Get individual monster instances tracked inside the horde (if any)." );
        luna::set_fx( ut, "monsters",
        []( UT_CLASS & group ) -> std::vector<monster *> {
            namespace views = std::views;
            return group.monsters
            | views::transform( []( monster & m ) { return &m; } )
            | std::ranges::to<std::vector<monster *>>();
        } );

        DOC( "Get the group position in absolute submap coordinates." );
        luna::set_fx( ut, "abs_pos", []( const UT_CLASS & group ) -> tripoint_abs_sm {
            return group.abs_pos;
        } );
        DOC( "Set the group position in absolute submap coordinates." );
        luna::set_fx( ut, "set_pos", []( UT_CLASS & group, const tripoint_abs_sm & pos ) -> void {
            group.abs_pos = pos;
        } );
        DOC( "Get the current target in absolute submap coordinates." );
        luna::set_fx( ut, "target", []( const UT_CLASS & group ) -> tripoint_abs_sm {
            return group.target;
        } );
        DOC( "Set the current target in absolute submap coordinates." );
        luna::set_fx( ut, "set_target", []( UT_CLASS & group, const tripoint_abs_sm & target ) -> void {
            group.target = target;
        } );
        DOC( "Get the nemesis target for this group." );
        luna::set_fx( ut, "nemesis_target", []( const UT_CLASS & group ) -> tripoint_abs_sm {
            return group.nemesis_target;
        } );
        DOC( "Set the nemesis target for this group using absolute submap coordinates." );
        luna::set_fx( ut, "set_nemesis_target", []( UT_CLASS & group, const tripoint_abs_sm & target ) -> void {
            group.set_nemesis_target( target );
        } );
        DOC( "Check if the group is safe." );
        luna::set_fx( ut, "is_safe", []( const UT_CLASS & group ) -> bool {
            return group.is_safe();
        } );
        DOC( "Check if the group has no members or population." );
        luna::set_fx( ut, "is_empty", []( const UT_CLASS & group ) -> bool {
            return group.empty();
        } );
        DOC( "Remove all monsters and clear population." );
        luna::set_fx( ut, "clear", []( UT_CLASS & group ) -> void {
            group.clear();
        } );
        DOC( "Increase horde interest by the given amount (clamped 15..100)." );
        luna::set_fx( ut, "inc_interest", []( UT_CLASS & group, int change ) -> void {
            group.inc_interest( change );
        } );
        DOC( "Decrease horde interest by the given amount (clamped 15..100)." );
        luna::set_fx( ut, "dec_interest", []( UT_CLASS & group, int change ) -> void {
            group.dec_interest( change );
        } );
        DOC( "Set horde interest directly (clamped 15..100)." );
        luna::set_fx( ut, "set_interest", []( UT_CLASS & group, int value ) -> void {
            group.set_interest( value );
        } );
        DOC( "Average movement speed of monsters in the group." );
        luna::set_fx( ut, "avg_speed", []( const UT_CLASS & group ) -> float {
            return group.avg_speed();
        } );
    }
#undef UT_CLASS

    DOC( "Monster group definitions and helpers" );
    auto lib = luna::begin_lib( lua, "monster_groups" );

    DOC( "List all monster group ids currently loaded." );
    luna::set_fx( lib, "get_all_ids",
    []() -> std::vector<mongroup_id> {
        return MonsterGroupManager::get_all_group_ids();
    } );

    DOC( "Get all monster group definitions." );
    luna::set_fx( lib, "get_all",
    []() -> std::vector<MonsterGroup> {
        namespace views = std::views;
        return MonsterGroupManager::get_all_group_ids()
        | views::transform( []( const mongroup_id & group_id )
        {
            return group_id.obj();
        } )
        | std::ranges::to<std::vector<MonsterGroup>>();
    } );

    DOC( "Get the monster ids directly listed in a group (default monster first)." );
    luna::set_fx( lib, "get_monsters_from_group",
    []( const mongroup_id & group_id ) -> std::vector<mtype_id> {
        return MonsterGroupManager::GetMonstersFromGroup( group_id );
    } );

    DOC( "Get a monster group definition by id." );
    luna::set_fx( lib, "get_group",
    []( const mongroup_id & group_id ) -> const MonsterGroup & {
        return group_id.obj();
    } );

    luna::finalize_lib( lib );
}

auto cata::detail::reg_monster_type_ids( sol::state &lua ) -> void
{
    DOC( "Monster type definitions and helpers" );
    auto lib = luna::begin_lib( lua, "monster_types" );

    DOC( "List all monster type ids currently loaded." );
    luna::set_fx( lib, "get_all_ids",
    []() -> std::vector<mtype_id> {
        namespace views = std::views;
        const auto &mtypes = MonsterGenerator::generator().get_all_mtypes();
        return mtypes
        | views::transform( []( const mtype & type ) { return type.id; } )
        | std::ranges::to<std::vector<mtype_id>>();
    } );

    luna::finalize_lib( lib );
}
