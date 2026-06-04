#include "catch/catch.hpp"

#include "item.h"
#include "procgen/proc_builder.h"
#include "procgen/proc_fact.h"
#include "procgen/proc_item.h"
#include "proc_test_utils.h"

TEST_CASE( "proc_validate_trail_mix_rejects_only_chocolate", "[proc][validate]" )
{
    auto sch = proc::schema{};
    sch.id = proc::schema_id( "trail_mix" );
    sch.cat = "food";
    sch.res = itype_id( "trail_mix_generic" );
    sch.lua.validate = "procgen.food.validate";

    auto chocolate_a = proc::normalize_part_fact( item( "chocolate" ), { .ix = 1 } );
    chocolate_a.tag.push_back( "trail_sweet" );
    auto chocolate_b = proc::normalize_part_fact( item( "candy2" ), { .ix = 2 } );
    chocolate_b.tag.push_back( "trail_sweet" );

    auto state = cata::lua_state {};
    proc_test::load_procgen_runtime( state );

    auto builder = proc::build_state( sch, { chocolate_a, chocolate_b } );
    proc::add_pick( builder, sch, proc::slot_id( "sweet" ), 1 );
    proc::add_pick( builder, sch, proc::slot_id( "sweet" ), 2 );

    const auto valid = proc::validate_selection( sch, proc::selected_facts( builder ), builder.fast,
    { .state = &state } );
    CHECK_FALSE( valid.has_value() );
    CHECK( valid.error() == "Trail mix can not be made from only chocolate." );
}
