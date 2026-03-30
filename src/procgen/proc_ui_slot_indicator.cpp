#include "procgen/proc_ui_slot_indicator.h"

#include <algorithm>
#include <iterator>
#include <ranges>
#include <string>
#include <vector>

#include "color.h"
#include "string_formatter.h"

namespace
{

inline constexpr char filled_slot = '#';
inline constexpr char required_empty_slot = '!';
inline constexpr char optional_empty_slot = '.';

auto cell_color( const bool filled, const bool required, const bool selected ) -> nc_color
{
    if( filled ) {
        return selected ? c_white_green : c_black_green;
    }
    if( required ) {
        return selected ? c_white_red : c_light_gray_red;
    }
    return selected ? c_white_white : c_black_white;
}

} // namespace

auto proc::slot_indicator_cells( const slot_data &slot, const int picked,
                                 const bool selected ) -> std::vector<slot_indicator_cell>
{
    auto cells = std::vector<slot_indicator_cell> {};
    cells.reserve( std::max( slot.max, 0 ) );
    std::ranges::transform( std::views::iota( 0, std::max( slot.max, 0 ) ), std::back_inserter( cells ),
    [&]( const int idx ) {
        const auto filled = idx < picked;
        const auto required = idx < slot.min;
        return slot_indicator_cell{
            .glyph = filled ? filled_slot : required ? required_empty_slot : optional_empty_slot,
            .color = cell_color( filled, required, selected ),
        };
    } );
    return cells;
}

auto proc::slot_indicator( const slot_data &slot, const int picked ) -> std::string
{
    auto cells = std::string {};
    std::ranges::for_each( slot_indicator_cells( slot, picked,
    false ), [&]( const slot_indicator_cell & cell ) {
        if( !cells.empty() ) {
            cells += ' ';
        }
        cells += cell.glyph;
    } );
    return string_format( "[%s]", cells );
}
