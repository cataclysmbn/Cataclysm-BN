#include "note_label_utils.h"

#include <string_view>

#include "string_utils.h"

namespace
{
constexpr auto label_prefix = std::string_view { "LABEL:" };
} // namespace

namespace note_label_utils
{
auto strip_label_commands( std::string note_text ) -> std::string
{
    auto pos = note_text.find( label_prefix );
    while( pos != std::string::npos ) {
        auto start = pos;
        while( start > 0 && ( note_text[start - 1] == ' ' || note_text[start - 1] == ';' ) ) {
            --start;
        }
        auto end = pos + label_prefix.size();
        while( end < note_text.size() && note_text[end] == ' ' ) {
            ++end;
        }
        while( end < note_text.size() && note_text[end] != ';' ) {
            ++end;
        }
        if( end < note_text.size() && note_text[end] == ';' ) {
            ++end;
        }
        note_text.erase( start, end - start );
        pos = note_text.find( label_prefix, start );
    }

    while( !note_text.empty() && note_text.back() == ';' ) {
        note_text.pop_back();
    }
    while( !note_text.empty() && note_text.front() == ';' ) {
        note_text.erase( note_text.begin() );
    }

    return trim_whitespaces( note_text );
}
} // namespace note_label_utils
