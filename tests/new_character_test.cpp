#include "catch/catch.hpp"

#include <array>
#include <cstddef>
#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "avatar.h"
#include "filesystem.h"
#include "game.h"
#include "item.h"
#include "item_contents.h"
#include "itype.h"
#include "newcharacter.h"
#include "path_info.h"
#include "pldata.h"
#include "profession.h"
#include "ret_val.h"
#include "scenario.h"
#include "state_helpers.h"
#include "string_id.h"
#include "type_id.h"

static std::ostream &operator<<( std::ostream &s, const std::vector<trait_id> &v )
{
    for( const auto &e : v ) {
        s << e.c_str() << " ";
    }
    return s;
}

static std::vector<trait_id> next_subset( const std::vector<trait_id> &set )
{
    // Doing it this way conveniently returns a vector containing solely set[foo] before
    // it returns any other vectors with set[foo] in it
    static unsigned bitset = 0;
    std::vector<trait_id> ret;

    ++bitset;
    // Check each bit position for a match
    for( size_t idx = 0; idx < set.size(); idx++ ) {
        if( bitset & ( 1 << idx ) ) {
            ret.push_back( set[idx] );
        }
    }
    return ret;
}

static bool try_set_traits( Character &ch, const std::vector<trait_id> &traits )
{
    ch.clear_mutations();
    newcharacter::add_traits( ch ); // mandatory prof/scen traits
    for( const trait_id &tr : traits ) {
        if( newcharacter::has_conflicting_trait( ch, tr ) || !g->scen->traitquery( tr ) ) {
            return false;
        } else if( !ch.has_trait( tr ) ) {
            ch.set_mutation( tr );
        }
    }
    return true;
}

static avatar get_sanitized_player()
{
    // You'd think that this hp stuff would be in the c'tor...
    // But the ctor is called by game::game, before we load anatomies
    avatar ret = avatar();
    ret.set_body();
    ret.recalc_hp();

    // Set these to starving/parched so can_eat doesn't return TOO_FULL
    ret.set_stored_kcal( 100 );
    ret.set_thirst( 10000 );
    return ret;
}

struct failure {
    string_id<profession> prof;
    std::vector<trait_id> mut;
    itype_id item_name;
    std::string reason;
};

namespace std
{
template<>
struct less<failure> {
    bool operator()( const failure &lhs, const failure &rhs ) const {
        return lhs.prof < rhs.prof;
    }
};
} // namespace std

// TODO: According to profiling (interrupt, backtrace, wait a few seconds, repeat) with a sample
// size of 20, 70% of the time is due to the call to Character::set_mutation in try_set_traits.
// When the mutation stuff isn't commented out, the test takes 110 minutes (not a typo)!

TEST_CASE( "starting_items", "[slow]" )
{
    clear_all_state();
    // Every starting trait that interferes with food/clothing
    const std::vector<trait_id> mutations = {
        trait_id( "ANTIFRUIT" ),
        trait_id( "ANTIJUNK" ),
        trait_id( "ANTIWHEAT" ),
        //trait_id( "ARM_TENTACLES" ),
        //trait_id( "BEAK" ),
        trait_id( "CANNIBAL" ),
        //trait_id( "CARNIVORE" ),
        //trait_id( "HERBIVORE" ),
        //trait_id( "HOOVES" ),
        trait_id( "LACTOSE" ),
        //trait_id( "LEG_TENTACLES" ),
        trait_id( "MEATARIAN" ),
        //trait_id( "RAP_TALONS" ),
        //trait_id( "TAIL_FLUFFY" ),
        //trait_id( "TAIL_LONG" ),
        trait_id( "VEGETARIAN" ),
        trait_id( "WOOLALLERGY" )
    };
    // Prof/scen combinations that need to be checked.
    std::unordered_map<const scenario *, std::vector<string_id<profession>>> scen_prof_combos;
    for( const auto &id : scenario::generic()->permitted_professions() ) {
        scen_prof_combos[scenario::generic()].push_back( id );
    }

    std::set<failure> failures;

    avatar &ch = get_avatar();

    ch = get_sanitized_player();

    // Avoid false positives from ingredients like salt and cornmeal.
    const avatar control = get_sanitized_player();

    std::vector<trait_id> traits = next_subset( mutations );
    for( ; !traits.empty(); traits = next_subset( mutations ) ) {
        for( const auto &pair : scen_prof_combos ) {
            g->scen = pair.first;
            for( const string_id<profession> &prof : pair.second ) {
                ch.prof = prof;
                if( !try_set_traits( ch, traits ) ) {
                    continue; // Trait conflict: this prof/scen/trait combo is impossible to attain
                }
                for( int i = 0; i < 2; i++ ) {
                    ch.worn.clear();
                    ch.reset_encumbrance();
                    ch.male = i == 0;
                    std::vector<detached_ptr<item>> items = prof->items( ch.male, traits );
                    /*
                    for( item * const &it : items ) {
                        const std::vector<item *> it_contents = it->contents.all_items_top();
                        for( item * const &top_content_item : it_contents ) {
                            items.push_back( top_content_item );
                        }
                    }*/

                    for( detached_ptr<item> &det_it : items ) {
                        item *it = &*det_it;
                        const bool is_food =  !it->is_seed() && it->is_food() &&
                                              !ch.can_eat( *it ).success() && control.can_eat( *it ).success();
                        const bool is_armor = it->is_armor() && ch.wear_item( std::move( det_it ), false );
                        // Seeds don't count- they're for growing things, not eating
                        if( is_food || is_armor ) {
                            failures.insert( failure{ prof->ident(), ch.get_mutations(), it->typeId(), is_food ? "Couldn't eat it" : "Couldn't wear it." } );
                        }

                        const bool is_holster = it->is_armor() && it->type->get_use( "holster" );
                        if( is_holster ) {
                            const item &holstered_it = it->get_contained();
                            const bool empty_holster = holstered_it.is_null();
                            if( !empty_holster && !it->can_holster( holstered_it, true ) ) {
                                failures.insert( failure{ prof->ident(), ch.get_mutations(), it->typeId(), "Couldn't put item back to holster" } );
                            }
                        }
                    }
                } // all genders
            } // all profs
        } // all scens
    }
    std::stringstream failure_messages;
    for( const failure &f : failures ) {
        failure_messages << f.prof.c_str() << " " << f.mut <<
                         " " << f.item_name.str() << ": " << f.reason << "\n";
    }
    INFO( failure_messages.str() );
    REQUIRE( failures.empty() );
}

TEST_CASE( "character_transfer_templates_drop_runtime_state", "[new_character]" )
{
    clear_all_state();

    avatar &source = get_avatar();
    source = get_sanitized_player();
    source.name = "Template Test";
    source.focus_pool = 42;
    source.follower_ids.insert( character_id( 123 ) );
    source.known_monsters.insert( mtype_id( "mon_zombie" ) );
    source.last_target_pos = tripoint( 4, 5, 0 );
    source.destination_point = tripoint( 1, 2, 0 );
    source.get_auto_move_route().push_back( tripoint( 0, 1, 0 ) );
    source.grab( OBJECT_VEHICLE, tripoint( 0, 1, 0 ) );

    const auto template_name = std::string( "unit-test-transfer-template" );
    const auto template_path = PATH_INFO::templatedir() + template_name + ".template";
    remove_file( template_path );

    source.character_to_template( template_name );

    auto loaded = get_sanitized_player();
    auto points = points_left();
    REQUIRE( loaded.load_template( template_name, points ) );

    CHECK( points.limit == points_left::TRANSFER );
    CHECK( loaded.follower_ids.empty() );
    CHECK( loaded.get_known_monsters().empty() );
    CHECK_FALSE( loaded.destination_point.has_value() );
    CHECK( loaded.get_auto_move_route().empty() );
    CHECK( loaded.get_grab_type() == OBJECT_NONE );

    CHECK( remove_file( template_path ) );
}
