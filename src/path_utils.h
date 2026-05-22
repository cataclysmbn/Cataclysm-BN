#pragma once

#include <filesystem>
#include <string>
#include <type_traits>

namespace fs = std::filesystem;

namespace cata_files
{

template<typename PathString>
auto path_string_to_utf8( const PathString &path_string ) -> std::string
{
    using char_type = typename PathString::value_type;
    if constexpr( std::is_same_v<char_type, char> ) {
        return path_string;
    } else if constexpr( std::is_same_v<char_type, char8_t> ) {
        return { reinterpret_cast<const char *>( path_string.data() ), path_string.size() };
    } else {
        static_assert( std::is_same_v<char_type, char>, "path string must be UTF-8" );
    }
}

inline auto path_to_utf8( const fs::path &path ) -> std::string
{
    return path_string_to_utf8( path.u8string() );
}

inline auto path_to_generic_utf8( const fs::path &path ) -> std::string
{
    return path_string_to_utf8( path.generic_u8string() );
}

} // namespace cata_files
