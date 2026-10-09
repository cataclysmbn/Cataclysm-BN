#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace engine_client
{

/// Consume one Unicode scalar, rejecting overlong UTF-8, surrogates and values above U+10FFFF.
/// Shared by the JSON-RPC scanner and the wire reader, not the legacy game JSON reader.
inline auto utf8_scalar( const std::string_view text, std::size_t &offset ) -> bool
{
    if( offset == text.size() ) { return false; }
    const auto lead = static_cast<unsigned char>( text[offset++] );
    if( lead < 0x80 ) { return true; }
    auto remaining = 0;
    auto scalar = std::uint32_t{0};
    auto minimum = std::uint32_t{0};
    if( lead >= 0xc2 && lead <= 0xdf ) {
        remaining = 1;
        scalar = lead & 0x1f;
        minimum = 0x80;
    } else if( lead >= 0xe0 && lead <= 0xef ) {
        remaining = 2;
        scalar = lead & 0x0f;
        minimum = 0x800;
    } else if( lead >= 0xf0 && lead <= 0xf4 ) {
        remaining = 3;
        scalar = lead & 0x07;
        minimum = 0x10000;
    } else { return false; }
    while( remaining-- > 0 ) {
        if( offset == text.size() ) { return false; }
        const auto byte = static_cast<unsigned char>( text[offset++] );
        if( ( byte & 0xc0 ) != 0x80 ) { return false; }
        scalar = ( scalar << 6 ) | ( byte & 0x3f );
    }
    return scalar >= minimum && scalar <= 0x10ffff &&
           !( scalar >= 0xd800 && scalar <= 0xdfff );
}

/// Append the UTF-8 encoding of an already validated scalar.
inline auto append_scalar( std::string &text, const std::uint32_t scalar ) -> void
{
    if( scalar < 0x80 ) { text.push_back( static_cast<char>( scalar ) ); }
    else if( scalar < 0x800 ) {
        text.push_back( static_cast<char>( 0xc0 | ( scalar >> 6 ) ) );
        text.push_back( static_cast<char>( 0x80 | ( scalar & 0x3f ) ) );
    } else if( scalar < 0x10000 ) {
        text.push_back( static_cast<char>( 0xe0 | ( scalar >> 12 ) ) );
        text.push_back( static_cast<char>( 0x80 | ( ( scalar >> 6 ) & 0x3f ) ) );
        text.push_back( static_cast<char>( 0x80 | ( scalar & 0x3f ) ) );
    } else {
        text.push_back( static_cast<char>( 0xf0 | ( scalar >> 18 ) ) );
        text.push_back( static_cast<char>( 0x80 | ( ( scalar >> 12 ) & 0x3f ) ) );
        text.push_back( static_cast<char>( 0x80 | ( ( scalar >> 6 ) & 0x3f ) ) );
        text.push_back( static_cast<char>( 0x80 | ( scalar & 0x3f ) ) );
    }
}

} // namespace engine_client
