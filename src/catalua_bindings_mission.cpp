#include <string>

#include "catalua_bindings.h"
#include "catalua.h"
#include "catalua_bindings_utils.h"
#include "catalua_luna.h"
#include "catalua_luna_doc.h"

#include "avatar.h"
#include "coordinates.h"
#include "mission.h"
#include "npc.h"
#include "itype.h"
#include "type_id.h"

void cata::detail::reg_mission( sol::state &lua )
{
#define UT_CLASS mission
    {
        sol::usertype<UT_CLASS> ut =
        luna::new_usertype<UT_CLASS>(
            lua,
            luna::no_bases,
            luna::constructors <
            // Define your actual constructors here
            mission()
            > ()
        );

        DOC( "Returns the mission's name as a string." );
        SET_FX( name );
        DOC( "Returns the mission type ID of this mission." );
        SET_FX( mission_id );
        DOC( "Returns true if the mission has a deadline." );
        SET_FX( has_deadline );
        DOC( "Returns the mission's deadline as a time_point." );
        SET_FX_T( get_deadline, time_point() const );
        DOC( "Returns the mission description." );
        SET_FX_T( get_description, std::string() const );
        DOC( "Returns true if the mission has a target." );
        SET_FX( has_target );
        DOC( "Returns the target of the mission (pointer to tripoint_abs_omt)." );
        luna::set_fx( ut, "get_target_point",
        []( const mission & m ) -> tripoint {
            return m.get_target().raw(); // assuming .raw() returns tripoint
        } );
        // as far as i can tell, there's no reason to include the base mission_type. we already MissionTypeIdRaw and Mission covers the rest
        DOC( "Returns the mission type of the target (pointer to mission_type)." );
        SET_FX_T( get_type, const mission_type & () const );
        DOC( "Returns true if the mission has a follow-up mission." );
        SET_FX( has_follow_up );
        DOC( "Returns the follow-up mission type ID." );
        SET_FX_T( get_follow_up, mission_type_id() const );
        DOC( "Returns the mission's value as an integer." );
        SET_FX( get_value );
        DOC( "Returns the mission's unique ID." );
        SET_FX( get_id );
        DOC( "Returns the item ID associated with the mission." );
        SET_FX_T( get_item_id, const itype_id & () const );
        DOC( "Returns the NPC character ID associated with the mission." );
        SET_FX_T( get_npc_id, character_id() const );

        using reward_list = std::vector<std::pair<int, itype_id>>;
        DOC( "Returns the likely rewards of the mission (vector of (int chance, itype_id) pairs)." );
        SET_FX_T( get_likely_rewards, const reward_list & () const );

        DOC( "Returns true if the mission has generic rewards." );
        SET_FX( has_generic_rewards );
        DOC( "Returns true if the mission is currently assigned." );
        SET_FX( is_assigned );
        DOC( "Fails the mission." );
        SET_FX( fail );
        DOC( "Wraps up the mission successfully." );
        SET_FX( wrap_up );
        DOC( "Returns true if the mission has failed." );
        SET_FX( has_failed );
        DOC( "Returns true if the mission is currently in progress." );
        SET_FX( in_progress );
        DOC( "Marks a mission step as complete, taking an integer step index." );
        SET_FX( step_complete );
        DOC( "Returns true if the mission goal has been completed (optionally checked against given NPC ID)." );
        luna::set_fx( ut, "is_complete",
        []( const mission & m, sol::optional<character_id> npc_id ) -> bool {
            return m.is_complete( npc_id.value_or( character_id() ) );
        } );

        DOC( "Assigns this mission to the given avatar." );
        luna::set_fx( ut, "assign",
        []( mission & m, avatar & u ) -> void {
            m.assign( u );
        } );

        DOC( "Reserves a new mission of the given type for the specified NPC. Returns the new mission." );
        luna::set_fx( ut, "reserve_new",
        []( const mission_type_id & type, const character_id & npc_id ) -> mission * {
            return mission::reserve_new( type, npc_id );
        } );

        DOC( "Reserves a random mission at the specified origin and position for the given NPC. Returns the new mission." );
        luna::set_fx( ut, "reserve_random",
        []( mission_origin origin, const tripoint & p, const character_id & npc_id ) -> mission * {
            return mission::reserve_random( origin, tripoint_abs_omt( p ), npc_id );
        } );

        reg_serde_functions( ut );
    }
#undef UT_CLASS
};

void cata::detail::reg_mission_type( sol::state &lua )
{
#define UT_CLASS mission_type
    {
        auto ut = luna::new_usertype<UT_CLASS>(
            lua,
            luna::no_bases,
            luna::constructors <
            mission_type()
            > ()
        );

        DOC( "Returns the mission's description as a string." );
        SET_MEMB_RO( description );

        DOC( "Returns the mission's goal text." );
        SET_MEMB_RO( goal );

        DOC( "Returns the mission's difficulty as an integer." );
        SET_MEMB_RO( difficulty );

        DOC( "Returns the mission's reward value as an integer." );
        SET_MEMB_RO( value );

        DOC( "Returns the minimum allowed deadline for the mission." );
        SET_MEMB_RO( deadline_low );

        DOC( "Returns the maximum allowed deadline for the mission." );
        SET_MEMB_RO( deadline_high );

        DOC( "Returns true if the mission is marked as urgent." );
        SET_MEMB_RO( urgent );

        DOC( "Returns true if the mission has generic rewards." );
        SET_MEMB_RO( has_generic_rewards );

        DOC( "Returns a vector of likely rewards (chance, itype_id pairs)." );
        SET_MEMB_RO( likely_rewards );

        DOC( "Returns a list of origins from which this mission can be generated." );
        SET_MEMB_RO( origins );

        DOC( "Returns the ID of the mission's main item target, if applicable." );
        SET_MEMB_RO( item_id );

        DOC( "Returns true if the mission requires removing a container." );
        SET_MEMB_RO( remove_container );

        DOC( "Returns true if the mission requires the container to be empty." );
        SET_MEMB_RO( empty_container );

        DOC( "Returns the count of items involved in the mission." );
        SET_MEMB_RO( item_count );

        DOC( "Returns the ID of the target NPC for the mission, if any." );
        SET_MEMB_RO( target_npc_id );

        DOC( "Returns the monster type associated with the mission, if any." );
        SET_MEMB_RO( monster_type );

        // DOC( "Returns the monster species associated with the mission." );
        // SET_MEMB_RO( monster_species );

        DOC( "Returns the number of monsters required to kill for this mission." );
        SET_MEMB_RO( monster_kill_goal );

        // DOC( "Returns the target ID for the mission." );
        // SET_MEMB_RO( target_id );

        DOC( "Returns any follow-up mission type ID." );
        SET_MEMB_RO( follow_up );

        DOC( "Returns any associated dialogue for the mission." );
        SET_MEMB_RO( dialogue );

        DOC( "Returns all available missions." );
        SET_FX( get_all );

        DOC( "Returns a random mission type ID at the specified origin and overmap tile position." );
        luna::set_fx( ut, "get_random_mission_id",
        []( mission_origin origin, const tripoint & p ) -> mission_type_id {
            return mission_type::get_random_id( origin, tripoint_abs_omt( p ) );
        } );

        SET_FX( tname );
    }
#undef UT_CLASS

#define UT_CLASS mission_type_id
    {
        auto ut = luna::new_usertype<UT_CLASS>(
            lua,
            luna::no_bases,
            luna::constructors <
            mission_type_id( std::string )
            > ()
        );
    }
#undef UT_CLASS
};