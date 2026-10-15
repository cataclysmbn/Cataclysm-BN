#include "item_serial.h"

#include <algorithm>
#include <mutex>
#include <unordered_map>

#include "page_serial.h"

namespace game_client
{
namespace
{
/// Items are destroyed on worker threads while mapgen runs, so the table is guarded.
auto table_mutex = std::mutex {};
auto serials = std::unordered_map<const item *, std::uint64_t> {};
auto last_serial = std::uint64_t { 0 };
} // namespace

auto item_serial( const item &it ) -> std::uint64_t
{
    const auto guard = std::lock_guard( table_mutex );
    return serials.try_emplace( &it, last_serial + 1 ).second ? ++last_serial : serials.at( &it );
}

auto item_row_id( const item &it ) -> std::string { return "item:" + std::to_string( item_serial( it ) ); }

auto stack_row_id( const std::span<const item *const> members ) -> std::string
{
    if( members.size() == 1 ) { return item_row_id( *members.front() ); }
    auto lowest = std::uint64_t{ 0 };
    for( const auto *const member : members ) {
        const auto serial = item_serial( *member );
        lowest = lowest == 0 ? serial : std::min( lowest, serial );
    }
    return "stack:" + std::to_string( lowest );
}

auto forget_item_serial( const item *const it ) -> void
{
    const auto guard = std::lock_guard( table_mutex );
    serials.erase( it );
}

auto reset_item_serials() -> void
{
    const auto guard = std::lock_guard( table_mutex );
    serials.clear();
    last_serial = 0;
    reset_diary_page_serials();
}

auto item_serial_count() -> std::size_t
{
    const auto guard = std::lock_guard( table_mutex );
    return serials.size();
}

} // namespace game_client
