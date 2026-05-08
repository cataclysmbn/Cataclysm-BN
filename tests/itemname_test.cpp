#include "catch/catch.hpp"

#include <memory>
#include <string>

#include "avatar.h"
#include "flag.h"
#include "flat_set.h"
#include "game.h"
#include "item.h"
#include "type_id.h"
#include "state_helpers.h"

TEST_CASE( "item sizing display", "[item][iteminfo][display_name][sizing]" )
{
    clear_all_state();
    GIVEN( "player is a normal size" ) {
        g->u.clear_mutations();

        WHEN( "the item is a normal size" ) {
            std::string name = item::spawn_temporary( "chestguard_hard" )->display_name();
            THEN( "the item name has no qualifier" ) {
                CHECK( name == "<color_c_light_green>||\u00A0</color>hard chest guard" );
            }
        }

        WHEN( "the item is oversized" ) {
            std::string name = item::spawn_temporary( "bootsheath" )->display_name();
            THEN( "the item name has no qualifier" ) {
                CHECK( name == "<color_c_light_green>||\u00A0</color>ankle sheath" );
            }
        }

        WHEN( "the item is undersized" ) {
            item &i = *item::spawn_temporary( "tunic" );
            i.set_flag( flag_UNDERSIZE );
            i.set_flag( flag_FIT );
            std::string name = i.display_name();

            THEN( "we have the correct sizing" ) {
                const item::sizing sizing_level = i.get_sizing( g->u );
                CHECK( sizing_level == item::sizing::small_sized_human_char );
            }

            THEN( "the item name says its too small" ) {
                CHECK( name == "<color_c_light_green>||\u00A0</color>tunic (too small)" );
            }
        }

    }

    GIVEN( "player is a huge size" ) {
        g->u.clear_mutations();
        g->u.toggle_trait( trait_id( "HUGE_OK" ) );

        WHEN( "the item is a normal size" ) {
            std::string name = item::spawn_temporary( "chestguard_hard" )->display_name();
            THEN( "the item name says its too small" ) {
                CHECK( name == "<color_c_light_green>||\u00A0</color>hard chest guard (too small)" );
            }
        }

        WHEN( "the item is oversized" ) {
            std::string name = item::spawn_temporary( "bootsheath" )->display_name();
            THEN( "the item name has no qualifier" ) {
                CHECK( name == "<color_c_light_green>||\u00A0</color>ankle sheath" );
            }
        }

        WHEN( "the item is undersized" ) {
            item &i = *item::spawn_temporary( "tunic" );
            i.set_flag( flag_UNDERSIZE );
            i.set_flag( flag_FIT );
            std::string name = i.display_name();

            THEN( "we have the correct sizing" ) {
                const item::sizing sizing_level = i.get_sizing( g->u );
                CHECK( sizing_level == item::sizing::small_sized_big_char );
            }

            THEN( "the item name says its tiny" ) {
                CHECK( name == "<color_c_light_green>||\u00A0</color>tunic (tiny!)" );
            }
        }

    }

    GIVEN( "player is a tiny size" ) {
        g->u.clear_mutations();
        g->u.toggle_trait( trait_id( "SMALL2_OK" ) );

        WHEN( "the item is a normal size" ) {
            std::string name = item::spawn_temporary( "chestguard_hard" )->display_name();
            THEN( "the item name says its too big" ) {
                CHECK( name == "<color_c_light_green>||\u00A0</color>hard chest guard (too big)" );
            }
        }

        WHEN( "the item is oversized" ) {
            std::string name = item::spawn_temporary( "bootsheath" )->display_name();
            THEN( "the item name has no qualifier" ) {
                CHECK( name == "<color_c_light_green>||\u00A0</color>ankle sheath (huge!)" );
            }
        }

        WHEN( "the item is undersized" ) {
            item &i = *item::spawn_temporary( "tunic" );
            i.set_flag( flag_UNDERSIZE );
            i.set_flag( flag_FIT );
            std::string name = i.display_name();

            THEN( "we have the correct sizing" ) {
                const item::sizing sizing_level = i.get_sizing( g->u );
                CHECK( sizing_level == item::sizing::small_sized_small_char );
            }

            THEN( "the item name has no qualifier" ) {
                CHECK( name == "<color_c_light_green>||\u00A0</color>tunic" );
            }
        }
    }
}

