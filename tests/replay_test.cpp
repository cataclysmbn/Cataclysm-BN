#include "catch/catch.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <latch>
#include <stdexcept>
#include <string>
#include <vector>

#include "cata_utility.h"
#include "input.h"
#include "random/rng.h"
#include "replay/replay.h"
#include "thread_pool.h"

namespace
{

auto test_replay_path() -> std::filesystem::path
{
    static auto counter = std::atomic_uint64_t{ 0 };
    const auto id = counter.fetch_add( 1, std::memory_order_relaxed );
    const auto process_token = reinterpret_cast<std::uintptr_t>( &counter );
    return std::filesystem::temp_directory_path() /
           ( "cataclysm-bn-replay-test-" + std::to_string( process_token ) + "-" +
             std::to_string( id ) + ".jsonl" );
}

auto cleanup_replay_file( const std::filesystem::path &path ) -> void
{
    replay::stop();
    std::filesystem::remove( path );
}

auto read_file( const std::filesystem::path &path ) -> std::string
{
    auto file = std::ifstream( path, std::ios::binary );
    REQUIRE( file.good() );
    return { std::istreambuf_iterator<char>( file ), std::istreambuf_iterator<char>() };
}

auto replay_line_count( const std::filesystem::path &path ) -> std::size_t
{
    const auto contents = read_file( path );
    return static_cast<std::size_t>( std::ranges::count( contents, '\n' ) );
}

auto write_replay_inputs( const std::filesystem::path &path,
                          const std::vector<input_event> &events ) -> void
{
    replay::stop();
    replay::configure_recording( path.string() );
    replay::start();
    for( const auto &event : events ) {
        replay::record_input_event( event );
    }
    replay::stop();
}

auto run_multithreaded_replay_sample( const std::filesystem::path &path ) -> std::vector<int>
{
    rng_set_deterministic_seed( 8675309 );
    const auto cleanup = on_out_of_scope( []() {
        replay::stop();
        rng_clear_deterministic_seed();
    } );

    auto result = std::vector<int> {};
    replay::stop();
    replay::configure_playback( path.string() );
    replay::start();

    {
        auto pool = cata_thread_pool( 4 );
        for( auto event_index = std::uint64_t{ 0 }; event_index < 2; ++event_index ) {
            const auto event = replay::next_input_event();
            REQUIRE( event );
            auto values = std::vector<int>( 32 );
            auto done = std::latch( static_cast<std::ptrdiff_t>( values.size() ) );
            for( auto i = std::size_t{ 0 }; i < values.size(); ++i ) {
                pool.submit( { .stream = 0x7265706c61795f5fULL, .id = ( event_index << 32 ) | i },
                [&values, &done, i, input = event->get_first_input()]() {
                    values[i] = input + rng( 1, 1000000 );
                    done.count_down();
                } );
            }
            done.wait();
            result.insert( result.end(), values.begin(), values.end() );
        }
    }

    return result;
}

} // namespace

TEST_CASE( "replay_configuration_is_single_mode_until_stopped", "[replay][input]" )
{
    replay::stop();
    const auto cleanup = on_out_of_scope( []() { replay::stop(); } );

    CHECK( replay::configured_mode() == replay::mode::none );
    CHECK( !replay::is_enabled() );

    replay::configure_recording( "record.jsonl" );
    CHECK( replay::configured_mode() == replay::mode::record );
    CHECK( replay::is_recording() );
    CHECK_THROWS_AS( replay::configure_playback( "play.jsonl" ), std::runtime_error );

    replay::stop();
    CHECK( replay::configured_mode() == replay::mode::none );

    replay::configure_playback( "play.jsonl" );
    CHECK( replay::configured_mode() == replay::mode::playback );
    CHECK( replay::is_playing() );
}

TEST_CASE( "replay_start_reports_missing_paths", "[replay][input]" )
{
    const auto path = test_replay_path();
    const auto cleanup = on_out_of_scope( [path]() { cleanup_replay_file( path ); } );

    replay::stop();
    replay::configure_recording( "" );
    CHECK_THROWS_AS( replay::start(), std::runtime_error );

    replay::stop();
    replay::configure_playback( path.string() );
    CHECK_THROWS_AS( replay::start(), std::runtime_error );
}

TEST_CASE( "replay_records_and_plays_user_input_jsonl", "[replay][input]" )
{
    const auto path = test_replay_path();
    const auto cleanup = on_out_of_scope( [path]() { cleanup_replay_file( path ); } );

    replay::stop();
    replay::configure_recording( path.string() );
    replay::start();
    replay::record_input_event( input_event{} );
    auto timeout = input_event{};
    timeout.type = input_event_t::timeout;
    replay::record_input_event( timeout );
    auto recorded = input_event( 'x', input_event_t::keyboard );
    recorded.modifiers = { KEY_ESCAPE, KEY_ENTER };
    recorded.text = "x";
    recorded.edit = "composition";
    recorded.edit_refresh = true;
    replay::record_input_event( recorded );
    replay::stop();

    const auto contents = read_file( path );
    CHECK( replay_line_count( path ) == 2 );
    CHECK( contents.contains( "keyboard" ) );
    CHECK( contents.contains( "timeout" ) );
    CHECK( !contents.contains( "error" ) );

    replay::configure_playback( path.string() );
    replay::start();
    const auto played_timeout = replay::next_input_event();
    const auto played = replay::next_input_event();
    REQUIRE( played_timeout );
    REQUIRE( played );
    CHECK( *played_timeout == timeout );
    CHECK( *played == recorded );
    CHECK( played->text == recorded.text );
    CHECK( played->edit == recorded.edit );
    CHECK( played->edit_refresh == recorded.edit_refresh );
    CHECK_THROWS_AS( replay::next_input_event(), std::runtime_error );
}

TEST_CASE( "replay_preserves_gamepad_and_mouse_events_in_order", "[replay][input]" )
{
    const auto path = test_replay_path();
    const auto cleanup = on_out_of_scope( [path]() { cleanup_replay_file( path ); } );

    auto gamepad = input_event( JOY_1, input_event_t::gamepad );
    gamepad.modifiers = { JOY_LEFT, JOY_RIGHT };
    auto mouse = input_event( MOUSE_BUTTON_LEFT, input_event_t::mouse );
    mouse.mouse_pos = point( 17, 23 );
    write_replay_inputs( path, { gamepad, mouse } );

    CHECK( replay_line_count( path ) == 2 );

    replay::configure_playback( path.string() );
    replay::start();
    const auto played_gamepad = replay::next_input_event();
    const auto played_mouse = replay::next_input_event();
    REQUIRE( played_gamepad );
    REQUIRE( played_mouse );
    CHECK( *played_gamepad == gamepad );
    CHECK( *played_mouse == mouse );
    CHECK( played_mouse->mouse_pos == mouse.mouse_pos );
    CHECK_THROWS_AS( replay::next_input_event(), std::runtime_error );
}

TEST_CASE( "replay_playback_skips_blank_jsonl_lines", "[replay][input]" )
{
    const auto path = test_replay_path();
    const auto cleanup = on_out_of_scope( [path]() { cleanup_replay_file( path ); } );

    const auto recorded = input_event( 'z', input_event_t::keyboard );
    write_replay_inputs( path, { recorded } );
    auto output = std::ofstream( path, std::ios::app | std::ios::binary );
    output << "\n\n";
    output.close();

    replay::configure_playback( path.string() );
    replay::start();
    const auto played = replay::next_input_event();
    REQUIRE( played );
    CHECK( *played == recorded );
    CHECK_THROWS_AS( replay::next_input_event(), std::runtime_error );
}

TEST_CASE( "replay_playback_is_deterministic_with_multithreaded_tasks",
           "[replay][rng][thread_pool]" )
{
    const auto path = test_replay_path();
    const auto cleanup = on_out_of_scope( [path]() { cleanup_replay_file( path ); } );

    auto first = input_event( 'a', input_event_t::keyboard );
    first.text = "a";
    auto second = input_event( 'b', input_event_t::keyboard );
    second.text = "b";
    write_replay_inputs( path, { first, second } );

    CHECK( run_multithreaded_replay_sample( path ) == run_multithreaded_replay_sample( path ) );
}
