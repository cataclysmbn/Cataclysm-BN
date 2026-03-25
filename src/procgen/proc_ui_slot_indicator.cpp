#include "procgen/proc_ui_slot_indicator.h"

#include <algorithm>
#include <ranges>
#include <string>

#include "string_formatter.h"

namespace
{

inline constexpr char filled_slot = '#';
inline constexpr char required_empty_slot = '!';
inline constexpr char optional_empty_slot = '.';

} // namespace

auto proc::slot_indicator( const slot_data &slot, const int picked ) -> std::string
{
    auto cells = std::string {};
    std::ranges::for_each( std::views::iota( 0, std::max( slot.max, 0 ) ), [&]( const int idx ) {
        if( !cells.empty() ) {
            cells += ' ';
        }
        if( idx < picked ) {
            cells += filled_slot;
        } else if( idx < slot.min ) {
            cells += required_empty_slot;
        } else {
            cells += optional_empty_slot;
        }
    } );
    return string_format( "[%s]", cells );
}
