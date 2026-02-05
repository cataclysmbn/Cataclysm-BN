#include "overmap_label_note.h"

#include <string_view>

namespace
{
constexpr auto label_prefix = std::string_view{ "LABEL:" };
} // namespace

auto overmap_label_note::make_note( const std::string &label ) -> std::string
{
    return std::string( label_prefix ) + label;
}

auto overmap_label_note::extract_label( const std::string &note ) -> std::optional<std::string>
{
    if( note.compare( 0, label_prefix.size(), label_prefix ) != 0 ) {
        return std::nullopt;
    }

    auto start = label_prefix.size();
    if( start < note.size() && note[start] == ' ' ) {
        start++;
    }

    if( start >= note.size() ) {
        return std::nullopt;
    }

    return note.substr( start );
}

auto overmap_label_note::is_label_only( const std::string &note ) -> bool
{
    const auto label = extract_label( note );
    if( !label.has_value() ) {
        return false;
    }

    if( note == make_note( *label ) ) {
        return true;
    }

    return note == std::string( label_prefix ) + " " + *label;
}
