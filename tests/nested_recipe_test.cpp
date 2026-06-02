#include "catch/catch.hpp"

#include "avatar.h"
#include "calendar.h"
#include "inventory.h"
#include "item.h"
#include "player_helpers.h"
#include "recipe.h"
#include "recipe_dictionary.h"
#include "state_helpers.h"
#include "type_id.h"

// Test that nested recipes correctly check if child recipes are known
TEST_CASE( "nested_recipe_known_check", "[crafting][nested][recipe]" )
{
    clear_all_state();
    avatar &dummy = get_avatar();
    clear_character( dummy );

    // backpack_hiking has difficulty 4 and autolearn: true
    // This means it requires tailor skill 4 to be "known" via autolearn
    const recipe *hiking_backpack = &recipe_id( "backpack_hiking" ).obj();

    GIVEN( "character with no skills" ) {
        // Ensure character has zero tailor skill
        dummy.set_skill_level( skill_id( "tailor" ), 0 );

        // Get the character's available recipes (recipes they know)
        inventory crafting_inv = dummy.crafting_inventory();
        const recipe_subset &available = dummy.get_available_recipes( crafting_inv );

        THEN( "hiking backpack should NOT be in available recipes" ) {
            CHECK_FALSE( available.contains( *hiking_backpack ) );
        }
    }

    GIVEN( "character with tailor skill 4" ) {
        // Set tailor skill to 4 (meets difficulty requirement for autolearn)
        dummy.set_skill_level( skill_id( "tailor" ), 4 );

        inventory crafting_inv = dummy.crafting_inventory();
        const recipe_subset &available = dummy.get_available_recipes( crafting_inv );

        THEN( "hiking backpack should be in available recipes" ) {
            CHECK( available.contains( *hiking_backpack ) );
        }
    }
}
