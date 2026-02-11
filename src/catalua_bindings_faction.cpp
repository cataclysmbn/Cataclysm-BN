#include "catalua_bindings.h"

#include "itype.h"
#include "mtype.h"
#include "monfaction.h"
#include "translations.h"
#include "string_id.h"

#include "catalua_bindings_utils.h"
#include "catalua_luna.h"
#include "catalua_luna_doc.h"

#include "faction.h"

void cata::detail::reg_faction( sol::state &lua )
{
#define UT_CLASS faction
    {
        sol::usertype<faction> ut = luna::new_usertype<faction>( lua, luna::no_bases, luna::no_constructor );

        DOC( "Name of the faction." );
        SET_FX( name );
        DOC( "How much the faction likes you." );
        SET_FX( likes_u );
        DOC( "How much the faction respects you." );
        SET_FX( respects_u );
        DOC( "Do you know about this faction?" );
        SET_FX( known_by_u );
        DOC( "ID of the faction." );
        SET_FX( id );
        DOC( "Raw description of the faction." );
        SET_FX( desc );
        DOC( "How big is the sphere of influence of the faction?" );
        SET_FX( size );
        DOC( "How powerful is the faction?" );
        SET_FX( power );
        DOC( "Nutritional value held by faction." );
        SET_FX( food_supply );
        DOC( "Total wealth of the faction." );
        SET_FX( wealth );
        DOC( "Is this a faction for just one person?" );
        SET_FX( lone_wolf_faction );
        DOC( "ID of the faction currency." );
        SET_FX( currency );
        DOC( "What a person have with the faction?" );
        SET_FX( relations );
        DOC( "mon_faction_id of the monster faction; defaults to human." );
        SET_FX( mon_faction );

        DOC( "Getter for desc." );
        SET_FX( describe );
        DOC( "Get faction epilogues." );
        SET_FX( epilogue );
        DOC( "Get a description of the faction's food supply." );
        SET_FX( food_supply_text );
        DOC( "Get the color of the food supply text." );
        SET_FX( food_supply_color );

        // Why was the test not failing before I created this file?
        DOC( "It looks like a test requires this." );
        luna::set_fx( ut, "str_id", [](
                          faction &fac
        ) { return fac.id; } );

        DOC( "Does the faction guy_id have a given relation with this faction?" );
        luna::set_fx( ut, "has_relationship", [](
                          faction & fac,
                          faction_id guy_id,
                          npc_factions::relationship flag
        ) { return fac.has_relationship( guy_id, flag ); } );

        DOC( "Add person to faction. Bool is whether or not the player knows them." );
        luna::set_fx( ut, "add_to_membership", [](
                          faction & fac,
                          character_id guy_id,
                          std::string guy_name,
                          bool known
        ) { fac.add_to_membership( guy_id, guy_name, known ); } );

        DOC( "Removes a person from the faction." );
        luna::set_fx( ut, "remove_member", [](
                          faction & fac,
                          character_id guy_id
        ) { fac.remove_member( guy_id ); } );

        DOC( "Unused as far as I can tell." );
        SET_FX( opinion_of );
        DOC( "Did the faction validate properly?" );
        SET_FX( validated );
        DOC( "List of faction members." );
        SET_FX( members );
    }
#undef UT_CLASS
}

void cata::detail::reg_faction_manager( sol::state &lua )
{
#define UT_CLASS faction_manager
    {
        sol::usertype<faction_manager> ut = luna::new_usertype<faction_manager>( lua, luna::no_bases, luna::no_constructor );

        DOC( "Deletes all factions." );
        luna::set_fx( ut, "clear", [](
                          faction_manager &fac_manager
        ) { fac_manager.clear(); } );

        DOC( "Creates factions if none exist." );
        luna::set_fx( ut, "create_if_needed", [](
                          faction_manager &fac_manager
        ) { fac_manager.create_if_needed(); } );

        DOC( "Displays faction menu (I think)" );
        luna::set_fx( ut, "display", [](
                          faction_manager &fac_manager
        ) { fac_manager.display(); } );

        DOC( "Creates a new faction based on a faction template." );
        luna::set_fx( ut, "add_new_faction", [](
                          faction_manager & fac_manager,
                          std::string name_new,
                          faction_id id_new,
                          faction_id template_id
        ) { return fac_manager.add_new_faction( name_new, id_new, template_id ); } );

        DOC( "Deletes a given faction." );
        luna::set_fx( ut, "remove_faction", [](
                          faction_manager & fac_manager,
                          faction_id id
        ) { fac_manager.remove_faction( id ); } );


        DOC( "Returns a list of factions." );
        SET_FX( all );

        DOC( "Gets a faction by id." );
        luna::set_fx( ut, "get", [](
                          faction_manager & fac_manager,
                          faction_id id,
                          bool complain
        ) { return fac_manager.get( id, complain ); } );

        DOC( "Get player faction." );
        luna::set_fx( ut, "get_player_faction", [](
                          faction_manager &fac_manager
        ) { return fac_manager.get( faction_id( ( "your_followers" ) ), true ); } );
    }
#undef UT_CLASS
}