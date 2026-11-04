#include "page_serial.h"

#include <unordered_map>

namespace game_client
{
namespace
{
auto serials = std::unordered_map<const diary_page *, std::uint64_t> {};
auto last_serial = std::uint64_t { 0 };
} // namespace

auto diary_page_serial( const diary_page &page ) -> std::uint64_t
{
    return serials.try_emplace( &page, last_serial + 1 ).second ? ++last_serial : serials.at( &page );
}

auto diary_page_row_id( const diary_page &page ) -> std::string { return "page:" + std::to_string( diary_page_serial( page ) ); }

auto find_diary_page_serial( const diary_page &page ) -> std::optional<std::uint64_t>
{
    const auto found = serials.find( &page );
    return found == serials.end() ? std::nullopt : std::optional( found->second );
}

auto forget_diary_page_serial( const diary_page *const page ) -> void { serials.erase( page ); }

auto reset_diary_page_serials() -> void
{
    serials.clear();
    last_serial = 0;
}

} // namespace game_client
