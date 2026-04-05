#include "catch/catch.hpp"

#include "options.h"

TEST_CASE( "json_option_definitions_are_loaded", "[options][json]" )
{
    auto &prompt_on_death = get_options().get_option( "PROMPT_ON_CHARACTER_DEATH" );
    CHECK( prompt_on_death.getType() == "bool" );
    CHECK_FALSE( prompt_on_death.getDefaultText( false ).empty() );

    auto &default_character_name = get_options().get_option( "DEF_CHAR_NAME" );
    CHECK( default_character_name.getType() == "string_input" );
    CHECK( default_character_name.getMaxLength() == 30 );

    auto &default_character_gender = get_options().get_option( "DEF_CHAR_GENDER" );
    CHECK( default_character_gender.getType() == "string_select" );
    CHECK( default_character_gender.getItems().size() == 2 );

    auto &merge_threshold = get_options().get_option( "MERGE_COMESTIBLES_THRESHOLD" );
    CHECK( merge_threshold.getType() == "float" );
    CHECK( merge_threshold.hasPrerequisite() );
    CHECK( merge_threshold.getPrerequisite() == "MERGE_COMESTIBLES" );

    auto &auto_pulp_butcher = get_options().get_option( "AUTO_PULP_BUTCHER" );
    CHECK( auto_pulp_butcher.getType() == "string_select" );
    CHECK( auto_pulp_butcher.getItems().size() == 4 );

    auto &autosave_turns = get_options().get_option( "AUTOSAVE_TURNS" );
    CHECK( autosave_turns.hasPrerequisite() );
    CHECK( autosave_turns.getPrerequisite() == "AUTOSAVE" );

    auto &skill_rust = get_options().get_option( "SKILL_RUST" );
    CHECK( skill_rust.getType() == "string_select" );
    CHECK( skill_rust.getItems().size() == 5 );

    auto &multithreading_enabled = get_options().get_option( "MULTITHREADING_ENABLED" );
    CHECK( multithreading_enabled.getType() == "bool" );

    auto &thread_pool_workers = get_options().get_option( "THREAD_POOL_WORKERS" );
    CHECK( thread_pool_workers.hasPrerequisite() );
    CHECK( thread_pool_workers.getPrerequisite() == "MULTITHREADING_ENABLED" );

    auto &world_end = get_options().get_option( "WORLD_END" );
    CHECK( world_end.getType() == "string_select" );
    CHECK( world_end.getPage() == "world_default" );
}
