#include "catch/catch.hpp"

#include <algorithm>
#include <climits>
#include <map>
#include <memory>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "activity_handlers.h"
#include "avatar.h"
#include "calendar.h"
#include "drop_token.h"
#include "map.h"
#include "map_helpers.h"
#include "map_selector.h"
#include "item.h"
#include "locations.h"
#include "pickup.h"
#include "pickup_token.h"
#include "player_helpers.h"
#include "state_helpers.h"
#include "units_mass.h"
#include "units_volume.h"

class testing_stack : public item_stack
{
    public:
        testing_stack() : item_stack( new location_vector<item>( new fake_item_location() ) ) { }
        ~testing_stack() override {
            delete( items );
        }

        item_stack::iterator insert_with_return( detached_ptr<item> &&newitem ) {
            return items->insert( items->end(), std::move( newitem ) );
        }
        void insert( detached_ptr<item> &&newitem ) override {
            insert_with_return( std::move( newitem ) );
        }
        detached_ptr<item> remove( item *to_remove ) override {
            for( auto it = items->begin(); it != items->end(); it++ ) {
                if( *it == to_remove ) {
                    detached_ptr<item> out;
                    erase( it, &out );
                    return out;
                }
            }
            return detached_ptr<item>();
        }
        iterator erase( const_iterator it, detached_ptr<item> *out = nullptr ) override {
            return items->erase( it, out );
        }
        int count_limit() const override {
            return INT_MAX;
        }
        units::volume max_volume() const override {
            return units::from_milliliter( std::numeric_limits<units::volume::value_type>::max() );
        }
};

TEST_CASE( "full backpack drop", "[activity][drop_token]" )
{
    clear_all_state();
    avatar dummy;
    item &an_item = *item::spawn_temporary( "bottle_glass" );
    item &backpack = *item::spawn_temporary( "backpack" );
    item &duffel_bag = *item::spawn_temporary( "duffelbag" );

    // Filling item and container types can be freely changed, as long as they meet this:
    REQUIRE( backpack.get_storage() % an_item.volume() == 0_ml );
    REQUIRE( backpack.get_storage() / an_item.volume() > 1 );
    REQUIRE( duffel_bag.get_storage() % an_item.volume() > 0_ml );
    REQUIRE( duffel_bag.get_storage() / an_item.volume() > 1 );

    GIVEN( "a character with a backpack full of items and no other containers" ) {
        REQUIRE( !dummy.wear_item( item::spawn( backpack ), false ) );
        while( dummy.can_pick_weight( an_item, true ) && dummy.can_pick_volume( an_item ) ) {
            dummy.i_add( item::spawn( an_item ) );
        }

        REQUIRE( dummy.worn.size() == 1 );
        REQUIRE( dummy.inv_size() >= 1 );

        WHEN( "he considers dropping the backpack" ) {
            drop_locations drop;
            drop.push_back( drop_location( *dummy.worn.front(), 1 ) );
            std::list<pickup::act_item> drop_list = pickup::reorder_for_dropping( dummy, drop );
            THEN( "he will try to drop all carried items" ) {
                // TODO: Check that all items will be dropped. inv.size() doesn't work because stacks
                AND_THEN( "all of them will have identical drop tokens, marking the backpack as parent" ) {
                    item_drop_token first_token = *drop_list.front().loc->drop_token;
                    for( const pickup::act_item &ait : drop_list ) {
                        CHECK( *ait.loc->drop_token == first_token );
                    }
                }
                AND_THEN( "the backpack will have non-zero drop cost, while all contents will have zero drop cost" ) {
                    REQUIRE( drop_list.front().loc->typeId() == backpack.typeId() );
                    auto iter = drop_list.begin();
                    CHECK( iter->consumed_moves > 0 );
                    iter++;
                    while( iter != drop_list.end() ) {
                        CHECK( iter->consumed_moves == 0 );
                        iter++;
                    }
                }
                AND_THEN( "total volume dropped will equal volume of backpack and all items in inventory" ) {
                    units::volume total_dropped = std::accumulate( drop_list.begin(), drop_list.end(), 0_ml,
                    []( units::volume acc, const pickup::act_item & ait ) {
                        return acc + ait.loc->volume();
                    } );
                    units::volume inventory_volume = dummy.volume_carried();
                    units::volume worn_volume = std::accumulate( dummy.worn.begin(), dummy.worn.end(), 0_ml,
                    []( units::volume acc, const item * const & it ) {
                        return acc + it->volume();
                    } );
                    CHECK( total_dropped == inventory_volume + worn_volume );
                }
            }
        }
    }

    GIVEN( "a character with two duffel bags and two backpacks full of items and no other containers" ) {
        REQUIRE( !dummy.wear_item( item::spawn( duffel_bag ), false ) );
        REQUIRE( !dummy.wear_item( item::spawn( duffel_bag ), false ) );
        REQUIRE( !dummy.wear_item( item::spawn( backpack ), false ) );
        REQUIRE( !dummy.wear_item( item::spawn( backpack ), false ) );
        while( dummy.can_pick_weight( an_item, true ) && dummy.can_pick_volume( an_item ) ) {
            dummy.i_add( item::spawn( an_item ) );
        }

        REQUIRE( dummy.worn.size() == 4 );
        REQUIRE( dummy.inv_size() >= 1 );

        WHEN( "he considers dropping one duffel bag and one backpack" ) {
            drop_locations drop;
            auto first_duffel_iter = std::find_if( dummy.worn.begin(), dummy.worn.end(),
            [&]( const item * const & it ) {
                return it->typeId() == duffel_bag.typeId();
            } );
            auto first_backpack_iter = std::find_if( dummy.worn.begin(), dummy.worn.end(),
            [&]( const item * const & it ) {
                return it->typeId() == backpack.typeId();
            } );
            REQUIRE( first_duffel_iter != dummy.worn.end() );
            REQUIRE( first_backpack_iter != dummy.worn.end() );
            drop.push_back( drop_location( **first_duffel_iter, 1 ) );
            drop.push_back( drop_location( **first_backpack_iter, 1 ) );

            //Clear these lest they trigger safety warnings
            std::list<pickup::act_item> drop_list = pickup::reorder_for_dropping( dummy, drop );
            THEN( "he will try to drop some, but not all of the carried items" ) {
                REQUIRE( drop_list.size() > 4 );

                units::volume total_dropped = std::accumulate( drop_list.begin(), drop_list.end(), 0_ml,
                []( units::volume acc, const pickup::act_item & ait ) {
                    return acc + ait.loc->volume();
                } );
                units::volume inventory_volume = dummy.volume_carried();
                units::volume worn_dropped_volume = ( *first_duffel_iter )->volume() +
                                                    ( *first_backpack_iter )->volume();
                CHECK( total_dropped - worn_dropped_volume < inventory_volume );

                AND_THEN( "dropped duffel bag and backpack will have non-zero drop cost, while all contents will have zero drop cost" ) {
                    auto drop_duffel_iter = std::find_if( drop_list.begin(), drop_list.end(),
                    [&]( const pickup::act_item & ait ) {
                        return ait.loc->typeId() == duffel_bag.typeId();
                    } );
                    auto drop_backpack_iter = std::find_if( drop_list.begin(), drop_list.end(),
                    [&]( const pickup::act_item & ait ) {
                        return ait.loc->typeId() == backpack.typeId();
                    } );
                    REQUIRE( drop_duffel_iter != drop_list.end() );
                    REQUIRE( drop_backpack_iter != drop_list.end() );
                    for( auto iter = drop_list.begin(); iter != drop_list.end(); iter++ ) {
                        if( iter == drop_duffel_iter || iter == drop_backpack_iter ) {
                            CHECK( iter->consumed_moves > 0 );
                        } else {
                            CHECK( iter->consumed_moves == 0 );
                        }
                    }

                    AND_THEN( "both containers will be followed by enough same-drop-token items to fill them" ) {
                        const int expected_duffel_content_count = duffel_bag.get_total_capacity() / an_item.volume();
                        const int expected_backpack_content_count = backpack.get_total_capacity() / an_item.volume();

                        int actual_duffel_content_count = 0;
                        for( auto iter = std::next( drop_duffel_iter ); iter != drop_list.end(); iter++ ) {
                            if( iter->consumed_moves == 0 && iter->loc->typeId() == an_item.typeId() ) {
                                actual_duffel_content_count++;
                            } else {
                                break;
                            }
                        }

                        int actual_backpack_content_count = 0;
                        for( auto iter = std::next( drop_backpack_iter ); iter != drop_list.end(); iter++ ) {
                            if( iter->consumed_moves == 0 && iter->loc->typeId() == an_item.typeId() ) {
                                actual_backpack_content_count++;
                            } else {
                                break;
                            }
                        }

                        first_duffel_iter = {};
                        first_backpack_iter = {};

                        // +1 because we aren't checking tokens, but the last item is still zero cost
                        CHECK( actual_duffel_content_count >= expected_duffel_content_count );
                        CHECK( actual_duffel_content_count <= expected_duffel_content_count + 1 );
                        CHECK( actual_backpack_content_count >= expected_backpack_content_count );
                        CHECK( actual_backpack_content_count <= expected_backpack_content_count + 1 );

                        AND_THEN( "when actually dropping items, each container will be followed by enough items of same token to fill it, "
                                  "followed by one un-contained item" ) {
                            dummy.set_moves( 1000 );
                            std::vector<detached_ptr<item>> tokenized_dropped = obtain_and_tokenize_items( dummy, drop_list );
                            auto tokenized_duffel_iter = std::find_if( tokenized_dropped.begin(), tokenized_dropped.end(),
                            [&]( const detached_ptr<item> &it ) {
                                return it->typeId() == duffel_bag.typeId();
                            } );
                            auto tokenized_backpack_iter = std::find_if( tokenized_dropped.begin(), tokenized_dropped.end(),
                            [&]( const detached_ptr<item>   &it ) {
                                return it->typeId() == backpack.typeId();
                            } );
                            REQUIRE( tokenized_duffel_iter != tokenized_dropped.end() );
                            REQUIRE( tokenized_backpack_iter != tokenized_dropped.end() );
                            REQUIRE( *( *tokenized_duffel_iter )->drop_token != *( *tokenized_backpack_iter )->drop_token );
                            int actual_duffel_tokens = std::count_if( tokenized_dropped.begin(), tokenized_dropped.end(),
                            [&]( const detached_ptr<item> &it ) {
                                return it->drop_token->is_child_of( *( *tokenized_duffel_iter )->drop_token );
                            } );
                            int actual_backpack_tokens = std::count_if( tokenized_dropped.begin(), tokenized_dropped.end(),
                            [&]( const detached_ptr<item> &it ) {
                                return it->drop_token->is_child_of( *( *tokenized_backpack_iter )->drop_token );
                            } );
                            const int expected_duffel_token_count = duffel_bag.get_total_capacity() / an_item.volume();
                            const int expected_backpack_token_count = backpack.get_total_capacity() / an_item.volume();
                            CHECK( actual_duffel_tokens == expected_duffel_token_count );
                            CHECK( actual_backpack_tokens == expected_backpack_token_count );
                            int all_items = tokenized_dropped.size();
                            // Two containers and one item not belonging to any containers
                            CHECK( all_items == actual_duffel_tokens + actual_backpack_tokens + 2 + 1 );
                        }
                    }
                }
            }
        }
    }

    GIVEN( "a character with two duffel bags full of items" ) {
        REQUIRE( !dummy.wear_item( item::spawn( duffel_bag ), false ) );
        REQUIRE( !dummy.wear_item( item::spawn( duffel_bag ), false ) );
        while( dummy.can_pick_weight( an_item, true ) && dummy.can_pick_volume( an_item ) ) {
            dummy.i_add( item::spawn( an_item ) );
        }

        WHEN( "he considers dropping only one of the bags, but all of the items" ) {
            drop_locations drop;
            drop.push_back( drop_location( *dummy.worn.front(), 1 ) );
            std::vector<item *> dump;
            dummy.dump_inv( dump );
            for( item *it : dump ) {
                drop.push_back( drop_location( *it, 1 ) );
            }
            std::list<pickup::act_item> drop_list = pickup::reorder_for_dropping( dummy, drop );
            THEN( "at most half of the non-bag items will have zero drop cost" ) {
                const size_t actual_zero_cost = std::count_if( drop_list.begin(), drop_list.end(),
                [&]( const pickup::act_item & ait ) {
                    return ait.consumed_moves == 0;
                } );
                const size_t expected_zero_cost = ( drop_list.size() - 1 ) / 2;
                CHECK( actual_zero_cost == expected_zero_cost );
            }
        }
    }
}

TEST_CASE( "full backpack pickup", "[drop_token]" )
{
    clear_all_state();
    constexpr tripoint pos = tripoint( 60, 60, 0 );
    avatar &dummy = get_avatar();
    item &an_item = *item::spawn_temporary( "bottle_glass" );
    item &backpack = *item::spawn_temporary( "backpack" );
    item &duffel_bag = *item::spawn_temporary( "duffelbag" );
    dummy.set_moves( 100 );

    dummy.worn.push_back( item::spawn( duffel_bag ) );

    map &here = get_map();
    drop_token_provider &token_provider = drop_token::get_provider();
    GIVEN( "a backpack full of items lying on a ground tile" ) {
        detached_ptr<item> bp = item::spawn( backpack );
        item &parent = *bp;
        here.add_item( pos, std::move( bp ) );
        *parent.drop_token = token_provider.make_next( calendar::turn );
        for( units::volume remaining_storage = backpack.get_storage();
             remaining_storage > an_item.volume();
             remaining_storage -= an_item.volume() ) {
            detached_ptr<item> det = item::spawn( an_item );
            item &child = *det;
            here.add_item( pos, std::move( det ) );
            *child.drop_token = token_provider.make_next( calendar::turn );
            child.drop_token->parent_number = parent.drop_token->drop_number;
        }

        map_stack stack = here.i_at( pos );
        REQUIRE( stack.size() > 2 );
        for( const item * const &it : stack ) {
            REQUIRE( it->drop_token->turn == parent.drop_token->turn );
        }

        WHEN( "a character with enough volume and weight capacity tries to pick it up" ) {
            units::mass weight_sum = std::accumulate( stack.begin(), stack.end(), 0_gram,
            []( units::mass acc, const item * const & it ) {
                return acc + it->weight();
            } );
            units::volume volume_sum = std::accumulate( stack.begin(), stack.end(), 0_ml,
            []( units::volume acc, const item * const & it ) {
                return acc + it->volume();
            } );
            REQUIRE( dummy.weight_capacity() >= weight_sum + duffel_bag.weight() );
            REQUIRE( dummy.volume_capacity() >= volume_sum );

            std::vector<item *> target_locations;
            std::vector<int> quantities;
            map_cursor mc( pos );
            for( item*&it : stack ) {
                target_locations.push_back( it );
                quantities.push_back( 0 );
            }

            std::vector<pickup::pick_drop_selection> targets = pickup::optimize_pickup( target_locations,
                    quantities );

            THEN( "the backpack is a pickup parent and all the contents are children" ) {
                CHECK( targets.front().target->typeId() == backpack.typeId() );
                CHECK( targets.front().children.size() + 1 == target_locations.size() );
            }

            int moves_before = dummy.get_moves();
            REQUIRE( moves_before > 0 );
            // TODO: Pickup doesn't need to be player-centric
            bool did_it = pickup::do_pickup( targets, true );

            THEN( "it succeeds and costs only as much moves as picking the backpack itself would" ) {
                CHECK( did_it );
                CHECK( dummy.get_moves() == moves_before - 100 );
                bool no_items_left = mc.items_with( []( const item & ) {
                    return true;
                } ).empty();
                CHECK( no_items_left );
                CHECK( dummy.volume_carried() == volume_sum );
            }
        }
    }
}

TEST_CASE( "drop overflow prefers non-favorites", "[activity][drop_token][favorite]" )
{
    clear_all_state();
    avatar dummy;
    item &non_favorite_item = *item::spawn_temporary( "bottle_plastic" );
    item &favorite_item = *item::spawn_temporary( "bottle_glass" );
    item &backpack = *item::spawn_temporary( "backpack" );
    item &duffel_bag = *item::spawn_temporary( "duffelbag" );

    REQUIRE( !dummy.wear_item( item::spawn( backpack ), false ) );
    REQUIRE( !dummy.wear_item( item::spawn( duffel_bag ), false ) );

    const auto &worn_items = dummy.worn;
    auto backpack_iter = std::ranges::find_if( worn_items, [&]( const item * const it ) {
        return it->typeId() == backpack.typeId();
    } );
    REQUIRE( backpack_iter != worn_items.end() );

    constexpr int non_favorite_count = 20;
    for( auto i = 0; i < non_favorite_count; i++ ) {
        dummy.i_add( item::spawn( non_favorite_item ) );
    }
    while( dummy.can_pick_weight( favorite_item, true ) && dummy.can_pick_volume( favorite_item ) ) {
        auto det = item::spawn( favorite_item );
        det->set_favorite( true );
        dummy.i_add( std::move( det ) );
    }

    auto drop = drop_locations{};
    drop.push_back( drop_location( **backpack_iter, 1 ) );
    auto drop_list = pickup::reorder_for_dropping( dummy, drop );

    const auto dropped_favorites = std::count_if( drop_list.begin(), drop_list.end(),
    [&]( const pickup::act_item & ait ) {
        return !dummy.is_worn( *ait.loc ) && ait.loc->is_favorite;
    } );
    const auto dropped_non_favorites = std::count_if( drop_list.begin(), drop_list.end(),
    [&]( const pickup::act_item & ait ) {
        return !dummy.is_worn( *ait.loc ) && !ait.loc->is_favorite;
    } );

    CHECK( dropped_favorites > 0 );
    CHECK( dropped_non_favorites == non_favorite_count );
}

TEST_CASE( "backpack drop keeps favorites when non-favorites are sufficient",
           "[activity][drop_token][favorite]" )
{
    clear_all_state();
    avatar dummy;
    item &non_favorite_item = *item::spawn_temporary( "bottle_plastic" );
    item &favorite_item = *item::spawn_temporary( "bottle_glass" );
    item &backpack = *item::spawn_temporary( "backpack" );
    item &duffel_bag = *item::spawn_temporary( "duffelbag" );

    REQUIRE( !dummy.wear_item( item::spawn( backpack ), false ) );
    REQUIRE( !dummy.wear_item( item::spawn( duffel_bag ), false ) );

    const auto &worn_items = dummy.worn;
    auto backpack_iter = std::ranges::find_if( worn_items, [&]( const item * const it ) {
        return it->typeId() == backpack.typeId();
    } );
    REQUIRE( backpack_iter != worn_items.end() );

    constexpr int favorite_count = 3;
    for( auto i = 0; i < favorite_count; i++ ) {
        auto det = item::spawn( favorite_item );
        det->set_favorite( true );
        dummy.i_add( std::move( det ) );
    }

    while( dummy.can_pick_weight( non_favorite_item, true ) &&
           dummy.can_pick_volume( non_favorite_item ) ) {
        dummy.i_add( item::spawn( non_favorite_item ) );
    }

    auto drop = drop_locations{};
    drop.push_back( drop_location( **backpack_iter, 1 ) );
    auto drop_list = pickup::reorder_for_dropping( dummy, drop );

    const auto dropped_favorites = std::count_if( drop_list.begin(), drop_list.end(),
    [&]( const pickup::act_item & ait ) {
        return !dummy.is_worn( *ait.loc ) && ait.loc->is_favorite;
    } );
    const auto dropped_non_favorites = std::count_if( drop_list.begin(), drop_list.end(),
    [&]( const pickup::act_item & ait ) {
        return !dummy.is_worn( *ait.loc ) && !ait.loc->is_favorite;
    } );

    CHECK( dropped_non_favorites > 0 );
    CHECK( dropped_favorites == 0 );
}

static std::vector<item_stack::iterator> iterators_in_vector( item_stack &the_stack )
{
    std::vector<item_stack::iterator> unstacked;

    for( auto iter = the_stack.begin(); iter != the_stack.end(); iter++ ) {
        unstacked.emplace_back( iter );
    }

    return unstacked;
}

TEST_CASE( "pickup_ui_stacking", "[activity][drop_token]" )
{
    clear_all_state();
    item &an_item = *item::spawn_temporary( "bottle_glass" );
    item &backpack = *item::spawn_temporary( "backpack" );
    item &duffel_bag = *item::spawn_temporary( "duffelbag" );
    const size_t per_backpack = backpack.get_total_capacity() / an_item.volume();

    drop_token_provider &token_provider = drop_token::get_provider();

    GIVEN( "A mix of items on the ground, with some of them being in a backpack" ) {
        testing_stack the_stack;
        the_stack.insert( item::spawn( an_item ) );
        auto insert_parent_iter = the_stack.insert_with_return( item::spawn( backpack ) );
        item_drop_token parent_token = token_provider.make_next( calendar::turn );
        *( *insert_parent_iter )->drop_token = parent_token;
        for( size_t i = 0; i < per_backpack; i++ ) {
            auto child_iter = the_stack.insert_with_return( item::spawn( an_item ) );
            *( *child_iter )->drop_token = token_provider.make_next( calendar::turn );
            ( *child_iter )->drop_token->parent_number = parent_token.drop_number;
        }

        the_stack.insert( item::spawn( backpack ) );
        the_stack.insert( item::spawn( duffel_bag ) );
        the_stack.insert( item::spawn( backpack ) );

        // Result should be sorted, so scrambling it a bit should help
        std::reverse( the_stack.begin(), the_stack.end() );

        std::vector<item_stack::iterator> unstacked = iterators_in_vector( the_stack );

        auto post_scramble_parent_iter = std::find_if( unstacked.begin(), unstacked.end(),
        [&parent_token]( const item_stack::iterator & it ) {
            return *( *it )->drop_token == parent_token;
        } );
        REQUIRE( post_scramble_parent_iter != unstacked.end() );
        auto parent_iter = *post_scramble_parent_iter;
        WHEN( "The items are restacked in preparation for pickup display" ) {
            std::vector<pickup::stacked_items> stacked_nonflat = pickup::stack_for_pickup_ui( unstacked );
            std::vector<std::list<item_stack::iterator>> stacked = pickup::flatten( stacked_nonflat );
            THEN( "Backpack with children doesn't stack with empty ones" ) {
                std::list<item_stack::iterator> list_with_just_parent = {parent_iter};
                auto stacked_parent_iter = std::find( stacked.begin(), stacked.end(), list_with_just_parent );
                REQUIRE( stacked_parent_iter != stacked.end() );
                CHECK( stacked_parent_iter->size() == 1 );
            }

            THEN( "Backpack is immediately followed by all of its children" ) {
                std::list<item_stack::iterator> list_with_just_parent = {parent_iter};
                auto stacked_parent_iter = std::find( stacked.begin(), stacked.end(), list_with_just_parent );
                REQUIRE( stacked_parent_iter != stacked.end() );
                CHECK( stacked_parent_iter->size() == 1 );
                CHECK( ( *stacked_parent_iter->front() )->typeId() == backpack.typeId() );
                auto child_stack_iter = std::next( stacked_parent_iter );
                REQUIRE( child_stack_iter != stacked.end() );
                CHECK( ( *child_stack_iter->front() )->typeId() == an_item.typeId() );
                CHECK( child_stack_iter->size() == per_backpack );
            }
        }
    }

    GIVEN( "Two backpacks with set tokens, but no contents" ) {
        testing_stack the_stack;
        auto first_backpack_iter = the_stack.insert_with_return( item::spawn( backpack ) );
        *( *first_backpack_iter )->drop_token = token_provider.make_next( calendar::turn );
        auto second_backpack_iter = the_stack.insert_with_return( item::spawn( backpack ) );
        *( *second_backpack_iter )->drop_token = token_provider.make_next( calendar::turn );

        std::vector<item_stack::iterator> unstacked = iterators_in_vector( the_stack );

        WHEN( "The items are restacked in preparation for pickup display" ) {
            std::vector<pickup::stacked_items> stacked_nonflat = pickup::stack_for_pickup_ui( unstacked );
            std::vector<std::list<item_stack::iterator>> stacked = pickup::flatten( stacked_nonflat );
            THEN( "The two backpacks stack" ) {
                CHECK( stacked.size() == 1 );
            }
        }
    }

    GIVEN( "Multiple identical items with tokens with different turns and parents set, but with no parents on the item stack" ) {
        testing_stack the_stack;

        item_drop_token first_parent_token = token_provider.make_next( calendar::turn );
        for( size_t i = 0; i < per_backpack; i++ ) {
            auto child_iter = the_stack.insert_with_return( item::spawn( an_item ) );
            *( *child_iter )->drop_token = token_provider.make_next( calendar::turn );
            ( *child_iter )->drop_token->parent_number = first_parent_token.drop_number;
        }

        item_drop_token second_parent_token = token_provider.make_next( calendar::turn );
        for( size_t i = 0; i < per_backpack; i++ ) {
            auto child_iter = the_stack.insert_with_return( item::spawn( an_item ) );
            *( *child_iter )->drop_token = token_provider.make_next( calendar::turn );
            ( *child_iter )->drop_token->parent_number = second_parent_token.drop_number;
        }

        std::vector<item_stack::iterator> unstacked = iterators_in_vector( the_stack );
        WHEN( "The items are restacked in preparation for pickup display" ) {
            std::vector<pickup::stacked_items> stacked_nonflat = pickup::stack_for_pickup_ui( unstacked );
            std::vector<std::list<item_stack::iterator>> stacked = pickup::flatten( stacked_nonflat );
            THEN( "All of those items stack" ) {
                CHECK( stacked.size() == 1 );
            }
        }
    }
}
