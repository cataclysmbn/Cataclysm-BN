#include "item_serial.h"

#include <algorithm>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "page_serial.h"

namespace game_client
{
namespace
{
/// Items are destroyed on worker threads while mapgen runs, so the table is guarded.
auto table_mutex = std::mutex {};
auto serials = std::unordered_map<const item *, std::uint64_t> {};
auto last_serial = std::uint64_t { 0 };
/// The serials behind each published `stack:<lowest>` row.
auto stacks = std::unordered_map<std::uint64_t, std::vector<std::uint64_t>> {};
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
    auto member_serials = std::vector<std::uint64_t> {};
    for( const auto *const member : members ) { member_serials.push_back( item_serial( *member ) ); }
    const auto lowest = std::ranges::min( member_serials );
    const auto guard = std::lock_guard( table_mutex );
    stacks.insert_or_assign( lowest, std::move( member_serials ) );
    return "stack:" + std::to_string( lowest );
}

auto find_item_serial( const item &it ) -> std::optional<std::uint64_t>
{
    const auto guard = std::lock_guard( table_mutex );
    const auto found = serials.find( &it );
    return found == serials.end() ? std::nullopt : std::optional( found->second );
}

auto item_with_serial( const std::uint64_t serial ) -> const item *
{
    const auto guard = std::lock_guard( table_mutex );
    const auto found = std::ranges::find( serials, serial, &decltype( serials )::value_type::second );
    return found == serials.end() ? nullptr : found->first;
}

auto stack_member_serials( const std::uint64_t lowest ) -> std::vector<std::uint64_t>
{
    const auto guard = std::lock_guard( table_mutex );
    const auto found = stacks.find( lowest );
    if( found == stacks.end() ) { return {}; }
    auto alive = std::vector<std::uint64_t> {};
    for( const auto serial : found->second ) {
        if( std::ranges::contains( serials, serial, &decltype( serials )::value_type::second ) ) { alive.push_back( serial ); }
    }
    return alive;
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
    stacks.clear();
    last_serial = 0;
    reset_diary_page_serials();
}

auto item_serial_count() -> std::size_t
{
    const auto guard = std::lock_guard( table_mutex );
    return serials.size();
}

} // namespace game_client
