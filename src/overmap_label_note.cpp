#include "overmap_label_note.h"

#include <string_view>

namespace
{
constexpr auto label_prefix = std::string_view { "LABEL:" };
} // namespace

auto overmap_label_note::make_note( const std::string &label ) -> std::string
{
    return std::string( label_prefix ) + label;
}

auto overmap_label_note::extract_label( const std::string &note ) -> std::optional<std::string>
{
    auto search_start = std::string::size_type { 0 };
    while( search_start < note.size() ) {
        const auto pos = note.find( label_prefix, search_start );
        if( pos == std::string::npos ) {
            return std::nullopt;
        }

        if( pos > 0 ) {
            const char prev = note[pos - 1];
            if( prev != ' ' && prev != ';' && prev != ':' ) {
                search_start = pos + 1;
                continue;
            }
        }

        auto start = pos + label_prefix.size();
        while( start < note.size() && note[start] == ' ' ) {
            start++;
        }

        if( start >= note.size() ) {
            return std::nullopt;
        }

        auto end = start;
        while( end < note.size() && note[end] != ';' ) {
            end++;
        }

        while( end > start && note[end - 1] == ' ' ) {
            end--;
        }

        if( end == start ) {
            search_start = start;
            continue;
        }

        return note.substr( start, end - start );
    }

    return std::nullopt;
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
