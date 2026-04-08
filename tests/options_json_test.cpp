#include "catch/catch.hpp"

#include <algorithm>
#include <sstream>

#include "json.h"
#define private public
#include "options.h"
#undef private

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

    auto &enable_nested_categories = get_options().get_option( "ENABLE_NESTED_CATEGORIES" );
    CHECK( enable_nested_categories.getType() == "bool" );

    auto &fov_3d_z_range = get_options().get_option( "FOV_3D_Z_RANGE" );
    CHECK( fov_3d_z_range.getType() == "int" );
#if defined(__ANDROID__)
    CHECK( fov_3d_z_range.iDefault == 3 );
#else
    CHECK( fov_3d_z_range.iDefault == 5 );
#endif

    auto &world_end = get_options().get_option( "WORLD_END" );
    CHECK( world_end.getType() == "string_select" );
    CHECK( world_end.getPage() == "world_default" );
}

TEST_CASE( "general_sound_options_stay_before_jsonized_general_options", "[options][json]" )
{
    const auto page_iter = std::ranges::find_if( get_options().pages_, [](
    const options_manager::Page & page ) {
        return page.id_ == "general";
    } );
    REQUIRE( page_iter != get_options().pages_.end() );

    const auto &items = page_iter->items_;
    const auto sound_enabled_iter = std::ranges::find_if( items, []( const options_manager::PageItem &
    item ) {
        return item.type == options_manager::ItemType::Option && item.data == "SOUND_ENABLED";
    } );
    REQUIRE( sound_enabled_iter != items.end() );

    const auto prompt_on_death_iter = std::ranges::find_if( items, []( const options_manager::PageItem &
    item ) {
        return item.type == options_manager::ItemType::Option && item.data == "PROMPT_ON_CHARACTER_DEATH";
    } );
    REQUIRE( prompt_on_death_iter != items.end() );

    CHECK( std::ranges::distance( items.begin(), sound_enabled_iter ) <
           std::ranges::distance( items.begin(), prompt_on_death_iter ) );
}

TEST_CASE( "json_option_groups_keep_headers_and_options_contiguous", "[options][json]" )
{
    const auto page_iter = std::ranges::find_if( get_options().pages_, [](
    const options_manager::Page & page ) {
        return page.id_ == "general";
    } );
    REQUIRE( page_iter != get_options().pages_.end() );

    const auto &items = page_iter->items_;
    const auto first_auto_pickup_item = std::ranges::find_if( items, []( const options_manager::PageItem
    & item ) {
        return item.type == options_manager::ItemType::Option && item.data == "AUTO_PICKUP";
    } );
    REQUIRE( first_auto_pickup_item != items.end() );

    const auto first_auto_pickup_index = std::ranges::distance( items.begin(), first_auto_pickup_item );
    REQUIRE( first_auto_pickup_index >= 1 );
    CHECK( items[first_auto_pickup_index - 1].type == options_manager::ItemType::GroupHeader );
    CHECK( items[first_auto_pickup_index - 1].data == "auto_pickup" );

    const auto first_auto_feature_index = std::ranges::find_if( items, [](
    const options_manager::PageItem & item ) {
        return item.type == options_manager::ItemType::Option && item.data == "AUTO_FEATURES";
    } ) - items.begin();
    REQUIRE( first_auto_feature_index > first_auto_pickup_index );

    const auto auto_pickup_names = std::vector<std::string> {
        "AUTO_PICKUP",
        "AUTO_PICKUP_ADJACENT",
        "AUTO_PICKUP_WEIGHT_LIMIT",
        "AUTO_PICKUP_VOL_LIMIT",
        "AUTO_PICKUP_SAFEMODE",
        "NO_AUTO_PICKUP_ZONES_LIST_ITEMS"
    };
    for( size_t i = 0; i < auto_pickup_names.size(); ++i ) {
        CHECK( items[first_auto_pickup_index + static_cast<int>( i )].type ==
               options_manager::ItemType::Option );
        CHECK( items[first_auto_pickup_index + static_cast<int>( i )].data == auto_pickup_names[i] );
        CHECK( items[first_auto_pickup_index + static_cast<int>( i )].group == "auto_pickup" );
    }
}

TEST_CASE( "jsonized_options_keep_existing_saved_values", "[options][json]" )
{
    auto &default_character_name = get_options().get_option( "DEF_CHAR_NAME" );
    auto &sound_enabled = get_options().get_option( "SOUND_ENABLED" );

    const auto original_default_character_name = default_character_name.getValue( true );
    const auto original_sound_enabled = sound_enabled.getValue( true );

    auto saved_options = std::stringstream();
    saved_options << R"([
      {"name":"DEF_CHAR_NAME","value":"Maribel Hearn"},
      {"name":"SOUND_ENABLED","value":"false"}
    ])";
    JsonIn saved_options_input( saved_options );
    get_options().deserialize( saved_options_input );

    CHECK( default_character_name.getValue( true ) == "Maribel Hearn" );
    CHECK( sound_enabled.getValue( true ) == "false" );

    default_character_name.setValue( original_default_character_name );
    sound_enabled.setValue( original_sound_enabled );
}
