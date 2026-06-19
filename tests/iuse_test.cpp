#include "catch/catch.hpp"

#include <array>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "avatar.h"
#include "bodypart.h"
#include "calendar.h"
#include "cata_utility.h"
#include "character_id.h"
#include "flag.h"
#include "game.h"
#include "item.h"
#include "itype.h"
#include "map.h"
#include "map_helpers.h"
#include "morale_types.h"
#include "player_helpers.h"
#include "type_id.h"
#include "value_ptr.h"

TEST_CASE( "bionic_scanner_on_ground_marks_corpses_with_cbms", "[iuse][bionic_scanner]" )
{
    const auto restore_turn = restore_on_out_of_scope<time_point>( calendar::turn );
    clear_map();
    clear_avatar();

    auto &you = get_avatar();
    you.setID( character_id( 1 ), true );
    auto &here = get_map();
    g->place_player( tripoint_bub_ms( 60, 60, 0 ) );
    set_time( calendar::turn_zero + 12_hours );
    you.recalc_sight_limits();

    const auto corpse_pos = you.bub_pos() + tripoint_east;
    REQUIRE( you.sees( corpse_pos ) );
    auto corpse = item::make_corpse( mtype_id( "mon_zombie_soldier" ), calendar::turn, "" );
    corpse->add_component( item::spawn( "bio_power_storage", calendar::turn ) );
    REQUIRE_FALSE( here.add_item_or_charges( corpse_pos, std::move( corpse ), false ) );
    const auto corpse_stack = here.i_at( corpse_pos );
    REQUIRE( corpse_stack.size() == 1 );
    auto *const corpse_ptr = *corpse_stack.begin();
    REQUIRE( corpse_ptr->get_components().size() == 1 );
    REQUIRE( ( *corpse_ptr->get_components().begin() )->is_bionic() );

    auto scanner = item::spawn( "bionic_scanner_on", calendar::turn );
    scanner->ammo_set( itype_id( "battery" ), 10 );
    scanner->activate();
    const auto *const scanner_ptr = scanner.get();
    const auto charges_before = scanner_ptr->ammo_remaining();
    REQUIRE( charges_before > 0 );
    REQUIRE_FALSE( here.add_item_or_charges( you.bub_pos(), std::move( scanner ), false ) );

    here.process_items();

    CHECK( corpse_ptr->get_var( "bionics_scanned_by", -1 ) == you.getID().get_value() );
    CHECK( corpse_ptr->has_flag( flag_CBM_SCANNED ) );
    CHECK( scanner_ptr->ammo_remaining() == charges_before - 1 );
}

TEST_CASE( "bionic_scanner_inside_ground_container_marks_corpses_with_cbms",
           "[iuse][bionic_scanner]" )
{
    const auto restore_turn = restore_on_out_of_scope<time_point>( calendar::turn );
    clear_map();
    clear_avatar();

    auto &you = get_avatar();
    you.setID( character_id( 1 ), true );
    auto &here = get_map();
    g->place_player( tripoint_bub_ms( 60, 60, 0 ) );
    set_time( calendar::turn_zero + 12_hours );
    you.recalc_sight_limits();

    const auto corpse_pos = you.bub_pos() + tripoint_east;
    REQUIRE( you.sees( corpse_pos ) );
    auto corpse = item::make_corpse( mtype_id( "mon_zombie_soldier" ), calendar::turn, "" );
    corpse->add_component( item::spawn( "bio_power_storage", calendar::turn ) );
    REQUIRE_FALSE( here.add_item_or_charges( corpse_pos, std::move( corpse ), false ) );
    const auto corpse_stack = here.i_at( corpse_pos );
    REQUIRE( corpse_stack.size() == 1 );
    auto *const corpse_ptr = *corpse_stack.begin();

    auto backpack = item::spawn( "backpack", calendar::turn );
    backpack->put_in( item::spawn( "rock", calendar::turn ) );
    auto scanner = item::spawn( "bionic_scanner_on", calendar::turn );
    scanner->ammo_set( itype_id( "battery" ), 10 );
    scanner->activate();
    const auto *const scanner_ptr = scanner.get();
    const auto charges_before = scanner_ptr->ammo_remaining();
    backpack->put_in( std::move( scanner ) );
    REQUIRE( backpack->needs_processing() );
    REQUIRE_FALSE( here.add_item_or_charges( you.bub_pos(), std::move( backpack ), false ) );

    here.process_items();

    CHECK( corpse_ptr->get_var( "bionics_scanned_by", -1 ) == you.getID().get_value() );
    CHECK( corpse_ptr->has_flag( flag_CBM_SCANNED ) );
    CHECK( scanner_ptr->ammo_remaining() == charges_before - 1 );
}

TEST_CASE( "bionic_scanner_inside_container_marks_corpses_with_cbms", "[iuse][bionic_scanner]" )
{
    const auto restore_turn = restore_on_out_of_scope<time_point>( calendar::turn );
    clear_map();
    clear_avatar();

    auto &you = get_avatar();
    you.setID( character_id( 1 ), true );
    auto &here = get_map();
    g->place_player( tripoint_bub_ms( 60, 60, 0 ) );
    set_time( calendar::turn_zero + 12_hours );
    you.recalc_sight_limits();

    const auto corpse_pos = you.bub_pos() + tripoint_east;
    REQUIRE( you.sees( corpse_pos ) );
    auto corpse = item::make_corpse( mtype_id( "mon_zombie_soldier" ), calendar::turn, "" );
    corpse->add_component( item::spawn( "bio_power_storage", calendar::turn ) );
    REQUIRE_FALSE( here.add_item_or_charges( corpse_pos, std::move( corpse ), false ) );
    const auto corpse_stack = here.i_at( corpse_pos );
    REQUIRE( corpse_stack.size() == 1 );
    auto *const corpse_ptr = *corpse_stack.begin();
    REQUIRE( corpse_ptr->get_components().size() == 1 );
    REQUIRE( ( *corpse_ptr->get_components().begin() )->is_bionic() );

    auto backpack = item::spawn( "backpack", calendar::turn );
    backpack->put_in( item::spawn( "rock", calendar::turn ) );
    auto scanner = item::spawn( "bionic_scanner_on", calendar::turn );
    scanner->ammo_set( itype_id( "battery" ), 10 );
    scanner->activate();
    const auto *const scanner_ptr = scanner.get();
    const auto charges_before = scanner_ptr->ammo_remaining();
    REQUIRE( charges_before > 0 );
    backpack->put_in( std::move( scanner ) );
    you.i_add( std::move( backpack ) );

    you.process_items();

    CHECK( corpse_ptr->get_var( "bionics_scanned_by", -1 ) == you.getID().get_value() );
    CHECK( corpse_ptr->has_flag( flag_CBM_SCANNED ) );
    CHECK( scanner_ptr->ammo_remaining() == charges_before - 1 );
}

TEST_CASE( "bionic_scanner_inside_worn_container_marks_corpse_stack", "[iuse][bionic_scanner]" )
{
    const auto restore_turn = restore_on_out_of_scope<time_point>( calendar::turn );
    clear_map();
    clear_avatar();

    auto &you = get_avatar();
    you.setID( character_id( 1 ), true );
    auto &here = get_map();
    g->place_player( tripoint_bub_ms( 60, 60, 0 ) );
    set_time( calendar::turn_zero + 12_hours );
    you.recalc_sight_limits();

    const auto corpse_pos = you.bub_pos() + tripoint_east;
    REQUIRE( you.sees( corpse_pos ) );
    auto corpse_ptrs = std::vector<item *> {};
    for( auto i = 0; i < 10; ++i ) {
        auto corpse = item::make_corpse( mtype_id( "mon_zombie_soldier" ), calendar::turn, "" );
        corpse->add_component( item::spawn( "bio_power_storage", calendar::turn ) );
        auto *const corpse_ptr = corpse.get();
        REQUIRE_FALSE( here.add_item_or_charges( corpse_pos, std::move( corpse ), false ) );
        corpse_ptrs.push_back( corpse_ptr );
    }

    auto backpack = item::spawn( "backpack", calendar::turn );
    auto *const backpack_ptr = backpack.get();
    auto scanner = item::spawn( "bionic_scanner", calendar::turn );
    scanner->ammo_set( itype_id( "battery" ), 100 );
    auto *const scanner_ptr = scanner.get();
    backpack->put_in( std::move( scanner ) );
    REQUIRE_FALSE( you.wear_item( std::move( backpack ), false ) );

    REQUIRE( scanner_ptr->type->invoke( you, *scanner_ptr, you.bub_pos() ) == 0 );
    REQUIRE( scanner_ptr->typeId() == itype_id( "bionic_scanner_on" ) );
    REQUIRE( scanner_ptr->is_active() );
    REQUIRE( scanner_ptr->needs_processing() );
    REQUIRE( backpack_ptr->needs_processing() );
    here.build_map_cache( you.bub_pos().z() );
    here.update_visibility_cache( you.bub_pos().z() );
    REQUIRE( you.sees( corpse_pos ) );

    you.process_items();

    for( const item *const corpse_ptr : corpse_ptrs ) {
        CHECK( corpse_ptr->get_var( "bionics_scanned_by", -1 ) == you.getID().get_value() );
        CHECK( corpse_ptr->has_flag( flag_CBM_SCANNED ) );
    }
    CHECK( scanner_ptr->ammo_remaining() == 90 );
}

TEST_CASE( "eyedrops", "[iuse][eyedrops]" )
{
    avatar dummy;
    detached_ptr<item> det = item::spawn( "eyedrops", calendar::start_of_cataclysm,
                                          item::default_charges_tag{} );
    item &eyedrops = *det;
    dummy.i_add( std::move( det ) );
    int charges_before = eyedrops.charges;
    REQUIRE( charges_before > 0 );

    GIVEN( "avatar is boomered" ) {
        dummy.add_effect( efftype_id( "boomered" ), 1_hours );
        REQUIRE( dummy.has_effect( efftype_id( "boomered" ) ) );

        WHEN( "they use eye drops" ) {
            dummy.invoke_item( &eyedrops );

            THEN( "one dose is depleted" ) {
                CHECK( eyedrops.charges == charges_before - 1 );

                AND_THEN( "it removes the boomered effect" ) {
                    CHECK_FALSE( dummy.has_effect( efftype_id( "boomered" ) ) );
                }
            }
        }
    }

    GIVEN( "avatar is underwater" ) {
        dummy.set_underwater( true );

        WHEN( "they try to use eye drops" ) {
            bool used = dummy.invoke_item( &eyedrops );

            THEN( "it can't be used underwater" ) {
                CHECK_FALSE( used );
            }
        }
    }
}

TEST_CASE( "antifungal", "[iuse][antifungal]" )
{
    avatar dummy;
    detached_ptr<item> det = item::spawn( "antifungal", calendar::start_of_cataclysm,
                                          item::default_charges_tag{} );
    item &antifungal = *det;
    dummy.i_add( std::move( det ) );
    int charges_before = antifungal.charges;
    REQUIRE( charges_before > 0 );

    GIVEN( "avatar has a fungal infection" ) {
        dummy.add_effect( efftype_id( "fungus" ), 1_hours );
        REQUIRE( dummy.has_effect( efftype_id( "fungus" ) ) );

        WHEN( "they take an antifungal drug" ) {
            dummy.invoke_item( &antifungal );

            THEN( "one dose is depleted" ) {
                CHECK( antifungal.charges == charges_before - 1 );

                AND_THEN( "it cures the fungal infection" ) {
                    CHECK_FALSE( dummy.has_effect( efftype_id( "fungus" ) ) );
                }
            }
        }
    }

    GIVEN( "avatar has fungal spores" ) {
        dummy.add_effect( efftype_id( "spores" ), 1_hours );
        REQUIRE( dummy.has_effect( efftype_id( "spores" ) ) );

        WHEN( "they take an antifungal drug" ) {
            dummy.invoke_item( &antifungal );

            THEN( "one dose is depleted" ) {
                CHECK( antifungal.charges == charges_before - 1 );

                AND_THEN( "it has no effect on the spores" ) {
                    CHECK( dummy.has_effect( efftype_id( "spores" ) ) );
                }
            }
        }
    }
}

TEST_CASE( "antiparasitic", "[iuse][antiparasitic]" )
{
    avatar dummy;
    detached_ptr<item> det = item::spawn( "antiparasitic", calendar::start_of_cataclysm,
                                          item::default_charges_tag{} );
    item &antiparasitic = *det;
    dummy.i_add( std::move( det ) );

    int charges_before = antiparasitic.charges;
    REQUIRE( charges_before > 0 );

    GIVEN( "avatar has parasite infections" ) {
        dummy.add_effect( efftype_id( "dermatik" ), 1_hours );
        dummy.add_effect( efftype_id( "tapeworm" ), 1_hours );
        dummy.add_effect( efftype_id( "bloodworms" ), 1_hours );
        dummy.add_effect( efftype_id( "brainworms" ), 1_hours );
        dummy.add_effect( efftype_id( "paincysts" ), 1_hours );

        REQUIRE( dummy.has_effect( efftype_id( "dermatik" ) ) );
        REQUIRE( dummy.has_effect( efftype_id( "tapeworm" ) ) );
        REQUIRE( dummy.has_effect( efftype_id( "bloodworms" ) ) );
        REQUIRE( dummy.has_effect( efftype_id( "brainworms" ) ) );
        REQUIRE( dummy.has_effect( efftype_id( "paincysts" ) ) );

        WHEN( "they use an antiparasitic drug" ) {
            dummy.invoke_item( &antiparasitic );

            THEN( "one dose is depleted" ) {
                CHECK( antiparasitic.charges == charges_before - 1 );

                AND_THEN( "it cures all parasite infections" ) {
                    CHECK_FALSE( dummy.has_effect( efftype_id( "dermatik" ) ) );
                    CHECK_FALSE( dummy.has_effect( efftype_id( "tapeworm" ) ) );
                    CHECK_FALSE( dummy.has_effect( efftype_id( "bloodworms" ) ) );
                    CHECK_FALSE( dummy.has_effect( efftype_id( "brainworms" ) ) );
                    CHECK_FALSE( dummy.has_effect( efftype_id( "paincysts" ) ) );
                }
            }
        }
    }

    GIVEN( "avatar has a fungal infection" ) {
        dummy.add_effect( efftype_id( "fungus" ), 1_hours );
        REQUIRE( dummy.has_effect( efftype_id( "fungus" ) ) );

        WHEN( "they use an antiparasitic drug" ) {
            dummy.invoke_item( &antiparasitic );

            THEN( "one dose is depleted" ) {
                CHECK( antiparasitic.charges == charges_before - 1 );

                AND_THEN( "it has no effect on the fungal infection" ) {
                    CHECK( dummy.has_effect( efftype_id( "fungus" ) ) );
                }
            }
        }
    }
}

TEST_CASE( "anticonvulsant", "[iuse][anticonvulsant]" )
{
    avatar dummy;
    detached_ptr<item> det = item::spawn( "diazepam", calendar::start_of_cataclysm,
                                          item::default_charges_tag{} );
    item &anticonvulsant = *det;
    dummy.i_add( std::move( det ) );

    int charges_before = anticonvulsant.charges;
    REQUIRE( charges_before > 0 );

    GIVEN( "avatar has the shakes" ) {
        dummy.add_effect( efftype_id( "shakes" ), 1_hours );
        REQUIRE( dummy.has_effect( efftype_id( "shakes" ) ) );

        WHEN( "they use an anticonvulsant drug" ) {
            dummy.invoke_item( &anticonvulsant );

            THEN( "one dose is depleted" ) {
                CHECK( anticonvulsant.charges == charges_before - 1 );

                AND_THEN( "it cures the shakes" ) {
                    CHECK_FALSE( dummy.has_effect( efftype_id( "shakes" ) ) );

                    AND_THEN( "it has side-effects" ) {
                        CHECK( dummy.has_effect( efftype_id( "valium" ) ) );
                    }
                }
            }
        }
    }
}

// test the `iuse::oxygen_bottle` function
TEST_CASE( "oxygen tank", "[iuse][oxygen_bottle]" )
{
    avatar dummy;
    detached_ptr<item> det = item::spawn( "oxygen_tank", calendar::start_of_cataclysm,
                                          item::default_charges_tag{} );
    item &oxygen = *det;
    dummy.i_add( std::move( det ) );

    int charges_before = oxygen.charges;
    REQUIRE( charges_before > 0 );

    // Ensure baseline painkiller value to measure painkiller effects
    dummy.set_painkiller( 0 );
    REQUIRE( dummy.get_painkiller() == 0 );

    GIVEN( "avatar is suffering from smoke inhalation" ) {
        dummy.add_effect( efftype_id( "smoke" ), 1_hours );
        REQUIRE( dummy.has_effect( efftype_id( "smoke" ) ) );

        THEN( "a dose of oxygen relieves the smoke inhalation" ) {
            dummy.invoke_item( &oxygen );
            CHECK( oxygen.charges == charges_before - 1 );
            CHECK_FALSE( dummy.has_effect( efftype_id( "smoke" ) ) );

            AND_THEN( "it acts as a mild painkiller" ) {
                CHECK( dummy.get_painkiller() == 2 );
            }
        }
    }

    GIVEN( "avatar is suffering from tear gas" ) {
        dummy.add_effect( efftype_id( "teargas" ), 1_hours );
        REQUIRE( dummy.has_effect( efftype_id( "teargas" ) ) );

        THEN( "a dose of oxygen relieves the effects of tear gas" ) {
            dummy.invoke_item( &oxygen );
            CHECK( oxygen.charges == charges_before - 1 );
            CHECK_FALSE( dummy.has_effect( efftype_id( "teargas" ) ) );

            AND_THEN( "it acts as a mild painkiller" ) {
                CHECK( dummy.get_painkiller() == 2 );
            }
        }
    }

    GIVEN( "avatar is suffering from asthma" ) {
        dummy.add_effect( efftype_id( "asthma" ), 1_hours );
        REQUIRE( dummy.has_effect( efftype_id( "asthma" ) ) );

        THEN( "a dose of oxygen relieves the effects of asthma" ) {
            dummy.invoke_item( &oxygen );
            CHECK( oxygen.charges == charges_before - 1 );
            CHECK_FALSE( dummy.has_effect( efftype_id( "asthma" ) ) );

            AND_THEN( "it acts as a mild painkiller" ) {
                CHECK( dummy.get_painkiller() == 2 );
            }
        }
    }

    GIVEN( "avatar has no ill effects for the oxygen to treat" ) {
        REQUIRE_FALSE( dummy.has_effect( efftype_id( "smoke" ) ) );
        REQUIRE_FALSE( dummy.has_effect( efftype_id( "teargas" ) ) );
        REQUIRE_FALSE( dummy.has_effect( efftype_id( "asthma" ) ) );

        WHEN( "they are not already stimulated" ) {
            dummy.set_stim( 0 );
            REQUIRE( dummy.get_stim() == 0 );

            THEN( "a dose of oxygen is stimulating" ) {
                dummy.invoke_item( &oxygen );
                CHECK( oxygen.charges == charges_before - 1 );
                // values should match iuse function `oxygen_bottle`
                CHECK( dummy.get_stim() == 8 );

                AND_THEN( "it acts as a stronger painkiller" ) {
                    CHECK( dummy.get_painkiller() == 4 );
                }
            }
        }

        WHEN( "they are already quite stimulated" ) {
            // "quite stimulated" meaning the max-stimulation cutoff defined in
            // iuse function `oxygen_bottle`, which should match `max_stim` here:
            int max_stim = 16;

            dummy.set_stim( max_stim );
            REQUIRE( dummy.get_stim() == max_stim );

            THEN( "a dose of oxygen has no additional stimulation effects" ) {
                dummy.invoke_item( &oxygen );
                CHECK( oxygen.charges == charges_before - 1 );
                CHECK( dummy.get_stim() == max_stim );

                AND_THEN( "it acts as a mild painkiller" ) {
                    CHECK( dummy.get_painkiller() == 2 );
                }
            }
        }
    }
}

// test the `iuse::caff` and `iuse::atomic_caff` functions
TEST_CASE( "caffeine and atomic caffeine", "[iuse][caff][atomic_caff]" )
{
    avatar dummy;

    // Baseline fatigue level before caffeinating
    int fatigue_before = 200;
    dummy.set_fatigue( fatigue_before );

    // No stimulants or radiation
    dummy.set_stim( 0 );
    dummy.set_rad( 0 );
    REQUIRE( dummy.get_stim() == 0 );
    REQUIRE( dummy.get_rad() == 0 );

    // Thirsty
    dummy.set_thirst( 100 );

    SECTION( "coffee reduces fatigue" ) {

        detached_ptr<item> det = item::spawn( "coffee", calendar::start_of_cataclysm,
                                              item::default_charges_tag{} );
        item &coffee = *det;
        dummy.i_add( std::move( det ) );
        dummy.consume_item( item::spawn( coffee ) );
        CHECK( dummy.get_fatigue() == fatigue_before - coffee.get_comestible()->fatigue_mod );
    }

    SECTION( "atomic caffeine greatly reduces fatigue" ) {

        detached_ptr<item> det = item::spawn( "atomic_coffee", calendar::start_of_cataclysm,
                                              item::default_charges_tag{} );
        item &atomic_coffee = *det;
        dummy.i_add( std::move( det ) );
        dummy.consume_item( item::spawn( atomic_coffee ) );
        CHECK( dummy.get_fatigue() == fatigue_before - atomic_coffee.get_comestible()->fatigue_mod );
        CHECK( dummy.get_stim() == atomic_coffee.get_comestible()->stim );
    }
}

TEST_CASE( "towel", "[iuse][towel]" )
{
    avatar dummy;
    dummy.set_body();
    detached_ptr<item> det = item::spawn( "towel", calendar::start_of_cataclysm,
                                          item::default_charges_tag{} );
    item &towel = *det;
    dummy.i_add( std::move( det ) );

    GIVEN( "avatar is wet" ) {
        REQUIRE( body_part_torso->drench_capacity > 0 );
        REQUIRE( body_part_head->drench_capacity > 0 );
        REQUIRE( body_part_arm_l->drench_capacity > 0 );
        REQUIRE( body_part_arm_r->drench_capacity > 0 );
        // Saturate torso, head, and both arms
        dummy.drench( 100, { bodypart_str_id( "torso" ), bodypart_str_id( "head" ), bodypart_str_id( "arm_l" ), bodypart_str_id( "arm_r" ) },
                      false );
        REQUIRE( dummy.get_part( body_part_torso ).get_wetness() > 0 );
        REQUIRE( dummy.get_part( body_part_head ).get_wetness() > 0 );
        REQUIRE( dummy.get_part( body_part_arm_l ).get_wetness() > 0 );
        REQUIRE( dummy.get_part( body_part_arm_r ).get_wetness() > 0 );

        // FIXME: Morale alone is the trigger for drying off!
        // Without the morale modifier, towel_common thinks you're dry
        dummy.add_morale( MORALE_WET, -10, -10, 1_hours, 1_hours );

        WHEN( "they use a dry towel" ) {
            REQUIRE_FALSE( towel.has_flag( flag_WET ) );
            dummy.invoke_item( &towel );

            THEN( "it dries them off" ) {
                CHECK( dummy.get_part( body_part_torso ).get_wetness() == 0 );
                CHECK( dummy.get_part( body_part_head ).get_wetness() == 0 );
                CHECK( dummy.get_part( body_part_arm_l ).get_wetness() == 0 );
                CHECK( dummy.get_part( body_part_arm_r ).get_wetness() == 0 );

                AND_THEN( "the towel becomes wet" ) {
                    CHECK( towel.typeId().str() == "towel_wet" );
                }
            }
        }

        WHEN( "they use a wet towel" ) {
            towel.convert( itype_id( "towel_wet" ) );
            REQUIRE( towel.has_flag( flag_WET ) );
            dummy.invoke_item( &towel );

            THEN( "it does not dry them off" ) {
                CHECK( dummy.get_part( body_part_torso ).get_wetness() > 0 );
                CHECK( dummy.get_part( body_part_head ).get_wetness() > 0 );
                CHECK( dummy.get_part( body_part_arm_l ).get_wetness() > 0 );
                CHECK( dummy.get_part( body_part_arm_r ).get_wetness() > 0 );
            }
        }
    }

    GIVEN( "avatar has poor morale due to being wet" ) {
        dummy.add_morale( MORALE_WET, -10, -10, 1_hours, 1_hours );
        REQUIRE( dummy.get_morale( MORALE_WET ) == -10 );

        WHEN( "they use a wet towel" ) {
            towel.convert( itype_id( "towel_wet" ) );
            REQUIRE( towel.has_flag( flag_WET ) );
            dummy.invoke_item( &towel );

            THEN( "it does not improve their morale" ) {
                CHECK( dummy.get_morale( MORALE_WET ) == -10 );
            }
        }

        WHEN( "they use a dry towel" ) {
            REQUIRE_FALSE( towel.has_flag( flag_WET ) );
            dummy.invoke_item( &towel );

            THEN( "it improves their morale" ) {
                CHECK( dummy.get_morale( MORALE_WET ) == 0 );

                AND_THEN( "the towel becomes wet" ) {
                    CHECK( towel.typeId() == itype_id( "towel_wet" ) );
                }
            }
        }
    }

    GIVEN( "avatar is slimed, boomered, and glowing" ) {
        dummy.add_effect( efftype_id( "slimed" ), 1_hours );
        dummy.add_effect( efftype_id( "boomered" ), 1_hours );
        dummy.add_effect( efftype_id( "glowing" ), 1_hours );
        REQUIRE( dummy.has_effect( efftype_id( "slimed" ) ) );
        REQUIRE( dummy.has_effect( efftype_id( "boomered" ) ) );
        REQUIRE( dummy.has_effect( efftype_id( "glowing" ) ) );

        WHEN( "they use a dry towel" ) {
            REQUIRE_FALSE( towel.has_flag( flag_WET ) );
            dummy.invoke_item( &towel );

            THEN( "it removes all those effects at once" ) {
                CHECK_FALSE( dummy.has_effect( efftype_id( "slimed" ) ) );
                CHECK_FALSE( dummy.has_effect( efftype_id( "boomered" ) ) );
                CHECK_FALSE( dummy.has_effect( efftype_id( "glowing" ) ) );

                AND_THEN( "the towel becomes soiled" ) {
                    CHECK( towel.typeId() == itype_id( "towel_soiled" ) );
                }
            }
        }
    }

    GIVEN( "avatar is boomered and wet" ) {
        dummy.add_effect( efftype_id( "boomered" ), 1_hours );
        dummy.add_morale( MORALE_WET, -10, -10, 1_hours, 1_hours );
        REQUIRE( dummy.has_morale( MORALE_WET ) );

        WHEN( "they use a dry towel" ) {
            REQUIRE_FALSE( towel.has_flag( flag_WET ) );
            dummy.invoke_item( &towel );

            THEN( "it removes the boomered effect, but not the wetness" ) {
                CHECK_FALSE( dummy.has_effect( efftype_id( "boomered" ) ) );
                CHECK( dummy.has_morale( MORALE_WET ) );

                AND_THEN( "the towel becomes soiled" ) {
                    CHECK( towel.typeId() == itype_id( "towel_soiled" ) );
                }
            }
        }
    }
}

TEST_CASE( "thorazine", "[iuse][thorazine]" )
{
    avatar dummy;
    dummy.set_fatigue( 0 );
    detached_ptr<item> det = item::spawn( "thorazine", calendar::start_of_cataclysm,
                                          item::default_charges_tag{} );
    item &thorazine = *det;
    dummy.i_add( std::move( det ) );
    int charges_before = thorazine.charges;
    REQUIRE( charges_before >= 2 );

    GIVEN( "avatar has hallucination, and visuals effects" ) {
        dummy.add_effect( efftype_id( "hallu" ), 1_hours );
        dummy.add_effect( efftype_id( "visuals" ), 1_hours );
        REQUIRE( dummy.has_effect( efftype_id( "hallu" ) ) );
        REQUIRE( dummy.has_effect( efftype_id( "visuals" ) ) );

        WHEN( "they take some thorazine" ) {
            dummy.invoke_item( &thorazine );

            THEN( "it relieves both of those effects with a single dose" ) {
                CHECK( thorazine.charges == charges_before - 1 );
                REQUIRE_FALSE( dummy.has_effect( efftype_id( "hallu" ) ) );
                REQUIRE_FALSE( dummy.has_effect( efftype_id( "visuals" ) ) );

                AND_THEN( "it causes some fatigue" ) {
                    CHECK( dummy.get_fatigue() >= 5 );
                }
            }
        }
    }

    GIVEN( "avatar has already taken some thorazine" ) {
        dummy.invoke_item( &thorazine );
        REQUIRE( thorazine.charges == charges_before - 1 );
        REQUIRE( dummy.has_effect( efftype_id( "took_thorazine" ) ) );

        WHEN( "they take more thorazine" ) {
            dummy.invoke_item( &thorazine );

            THEN( "it only causes more fatigue" ) {
                CHECK( thorazine.charges == charges_before - 2 );
                CHECK( dummy.get_fatigue() >= 20 );
            }
        }
    }
}

TEST_CASE( "prozac", "[iuse][prozac]" )
{
    avatar dummy;
    detached_ptr<item> det = item::spawn( "prozac", calendar::start_of_cataclysm,
                                          item::default_charges_tag{} );
    item &prozac = *det;
    dummy.i_add( std::move( det ) );

    SECTION( "prozac gives prozac effect" ) {
        REQUIRE_FALSE( dummy.has_effect( efftype_id( "took_prozac" ) ) );

        dummy.invoke_item( &prozac );
        CHECK( dummy.has_effect( efftype_id( "took_prozac" ) ) );
    }

    SECTION( "taking prozac twice gives a stimulant effect" ) {
        dummy.set_stim( 0 );

        dummy.invoke_item( &prozac );
        CHECK( dummy.get_stim() == 0 );
        dummy.invoke_item( &prozac );
        CHECK( dummy.get_stim() > 0 );
    }
}

TEST_CASE( "inhaler", "[iuse][inhaler]" )
{
    avatar dummy;
    detached_ptr<item> det = item::spawn( "inhaler", calendar::start_of_cataclysm,
                                          item::default_charges_tag{} );
    item &inhaler = *det;
    dummy.i_add( std::move( det ) );

    GIVEN( "avatar is suffering from smoke inhalation" ) {
        dummy.add_effect( efftype_id( "smoke" ), 1_hours );
        REQUIRE( dummy.has_effect( efftype_id( "smoke" ) ) );

        THEN( "inhaler relieves it" ) {
            dummy.invoke_item( &inhaler );
            CHECK_FALSE( dummy.has_effect( efftype_id( "smoke" ) ) );
        }
    }

    GIVEN( "avatar is suffering from asthma" ) {
        dummy.add_effect( efftype_id( "asthma" ), 1_hours );
        REQUIRE( dummy.has_effect( efftype_id( "asthma" ) ) );

        THEN( "inhaler relieves the effects of asthma" ) {
            dummy.invoke_item( &inhaler );
            CHECK_FALSE( dummy.has_effect( efftype_id( "asthma" ) ) );
        }
    }

    GIVEN( "avatar is not suffering from asthma" ) {
        REQUIRE_FALSE( dummy.has_effect( efftype_id( "asthma" ) ) );

        THEN( "inhaler reduces fatigue" ) {
            dummy.set_fatigue( 10 );
            dummy.invoke_item( &inhaler );
            CHECK( dummy.get_fatigue() < 10 );
        }
    }
}

TEST_CASE( "panacea", "[iuse][panacea]" )
{
    avatar dummy;
    detached_ptr<item> det = item::spawn( "panacea", calendar::start_of_cataclysm,
                                          item::default_charges_tag{} );
    item &panacea = *det;
    dummy.i_add( std::move( det ) );

    SECTION( "panacea gives cure-all effect" ) {
        REQUIRE_FALSE( dummy.has_effect( efftype_id( "cureall" ) ) );

        dummy.consume_item( item::spawn( panacea ) );
        CHECK( dummy.has_effect( efftype_id( "cureall" ) ) );
    }
}

TEST_CASE( "xanax", "[iuse][xanax]" )
{
    avatar dummy;
    detached_ptr<item> det = item::spawn( "xanax", calendar::start_of_cataclysm,
                                          item::default_charges_tag{} );
    item &xanax = *det;
    dummy.i_add( std::move( det ) );

    SECTION( "xanax gives xanax effect" ) {
        REQUIRE_FALSE( dummy.has_effect( efftype_id( "took_xanax" ) ) );

        dummy.invoke_item( &xanax );
        CHECK( dummy.has_effect( efftype_id( "took_xanax" ) ) );
    }
}
