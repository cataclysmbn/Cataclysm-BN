#include "catch/catch.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <latch>
#include <string>
#include <thread>
#include <vector>

#include "input.h"
#include "random/rng.h"
#include "replay/replay.h"
#include "thread_pool.h"

namespace
{

auto test_replay_path() -> std::filesystem::path
{
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() /
           ( "cataclysm-bn-replay-test-" + std::to_string( now ) + ".jsonl" );
}

auto read_file( const std::filesystem::path &path ) -> std::string
{
    auto file = std::ifstream( path, std::ios::binary );
    REQUIRE( file.good() );
    return { std::istreambuf_iterator<char>( file ), std::istreambuf_iterator<char>() };
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
    auto result = std::vector<int> {};
    rng_set_deterministic_seed( 8675309 );
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
                    if( i % 3 == 0 ) {
                        std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
                    }
                    values[i] = input + rng( 1, 1000000 );
                    done.count_down();
                } );
            }
            done.wait();
            result.insert( result.end(), values.begin(), values.end() );
        }
    }

    replay::stop();
    rng_clear_deterministic_seed();
    return result;
}

} // namespace

TEST_CASE( "replay_records_and_plays_user_input_jsonl", "[replay][input]" )
{
    const auto path = test_replay_path();
    replay::stop();

    replay::configure_recording( path.string() );
    replay::start();
    replay::record_input_event( input_event{} );
    auto recorded = input_event( 'x', input_event_t::keyboard );
    recorded.text = "x";
    replay::record_input_event( recorded );
    replay::stop();

    const auto contents = read_file( path );
    CHECK( contents.contains( "keyboard" ) );
    CHECK( !contents.contains( "error" ) );

    replay::configure_playback( path.string() );
    replay::start();
    const auto played = replay::next_input_event();
    REQUIRE( played );
    CHECK( *played == recorded );
    CHECK( played->text == recorded.text );
    CHECK_THROWS( replay::next_input_event() );
    replay::stop();

    std::filesystem::remove( path );
}

TEST_CASE( "replay_playback_is_deterministic_with_multithreaded_tasks",
           "[replay][rng][thread_pool]" )
{
    const auto path = test_replay_path();
    auto first = input_event( 'a', input_event_t::keyboard );
    first.text = "a";
    auto second = input_event( 'b', input_event_t::keyboard );
    second.text = "b";
    write_replay_inputs( path, { first, second } );

    CHECK( run_multithreaded_replay_sample( path ) == run_multithreaded_replay_sample( path ) );

    std::filesystem::remove( path );
}
