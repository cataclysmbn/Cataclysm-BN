#include "engine_client_wire.h"
#include "json.h"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <set>
#include <sstream>
#include <utility>

namespace engine_client
{
namespace
{
/// Consume one Unicode scalar, rejecting overlong UTF-8, surrogates and values above U+10FFFF.
/// Shared by the reader and the small output DTO validators, not the legacy game JSON reader.
auto utf8_scalar( std::string_view text, std::size_t &offset ) -> bool
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
auto valid_id( std::string_view value ) -> bool
{
    if( value.empty() || value.size() > maximum_id_bytes ) { return false; }
    auto offset = std::size_t{0};
    while( offset < value.size() ) {
        if( !utf8_scalar( value, offset ) ) { return false; }
    }
    return true;
}
auto append_scalar( std::string &text, std::uint32_t scalar ) -> void
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
auto require( bool condition ) -> void
{
    if( !condition ) { throw decode_error::invalid_params; }
}
auto digit( char value ) -> bool { return value >= '0' && value <= '9'; }

/// Only the closed request grammar below is readable: no DOM, skip-value recursion, user
/// callbacks or arbitrary nested containers. Maximum nesting is three objects (target command).
/// Work/storage are bounded by maximum_inline_bytes; array uniqueness is O(n log n).
class request_reader
{
    public:
        explicit request_reader( std::string_view input ) : input_( input ) {
            if( input.size() > maximum_inline_bytes ) { throw decode_error::resource_limit; }
        }
        auto finish() -> void {
            whitespace();
            if( offset_ != input_.size() ) { throw decode_error::invalid_json; }
        }
        auto string( std::size_t maximum = maximum_inline_bytes ) -> std::string {
            whitespace();
            require( peek() == '"' );
            ++offset_;
            auto value = std::string{};
            while( true ) {
                const auto byte = take();
                if( byte == '"' ) { return value; }
                if( static_cast<unsigned char>( byte ) < 0x20 ) { throw decode_error::invalid_json; }
                if( byte == '\\' ) {
                    switch( take() ) {
                        case '"':
                            value.push_back( '"' );
                            break;
                        case '\\':
                            value.push_back( '\\' );
                            break;
                        case '/':
                            value.push_back( '/' );
                            break;
                        case 'b':
                            value.push_back( '\b' );
                            break;
                        case 'f':
                            value.push_back( '\f' );
                            break;
                        case 'n':
                            value.push_back( '\n' );
                            break;
                        case 'r':
                            value.push_back( '\r' );
                            break;
                        case 't':
                            value.push_back( '\t' );
                            break;
                        case 'u': {
                            auto scalar = hex_unit();
                            if( scalar >= 0xd800 && scalar <= 0xdbff ) {
                                expect( '\\' );
                                expect( 'u' );
                                const auto low = hex_unit();
                                if( low < 0xdc00 || low > 0xdfff ) { throw decode_error::invalid_json; }
                                scalar = 0x10000 + ( ( scalar - 0xd800 ) << 10 ) + low - 0xdc00;
                            } else if( scalar >= 0xdc00 && scalar <= 0xdfff ) { throw decode_error::invalid_json; }
                            append_scalar( value, scalar );
                            break;
                        }
                        default:
                            throw decode_error::invalid_json;
                    }
                } else {
                    const auto start = offset_ - 1;
                    offset_ = start;
                    if( !utf8_scalar( input_, offset_ ) ) { throw decode_error::invalid_json; }
                    value.append( input_.substr( start, offset_ - start ) );
                }
                require( value.size() <= maximum );
            }
        }
        auto id() -> std::string {
            auto value = string( maximum_id_bytes );
            require( !value.empty() );
            return value;
        }
        auto boolean() -> bool {
            whitespace();
            if( consume( "true" ) ) { return true; }
            if( consume( "false" ) ) { return false; }
            throw decode_error::invalid_params;
        }
        auto counter_value() -> counter {
            const auto value = string( 20 );
            require( !value.empty() && ( value.size() == 1 || value.front() != '0' ) );
            require( std::ranges::all_of( value, digit ) );
            auto result = counter{0};
            const auto parsed = std::from_chars( value.data(), value.data() + value.size(), result );
            require( parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size() );
            return result;
        }
        /// Exact decimal arithmetic, not strtod/f64. Saturate exponents before arithmetic;
        /// a nonzero bounded result needs at most 19 significant integral digits. Zero with
        /// any exponent is still zero. Fractional tails must be literally all zero.
        auto integer( std::int64_t minimum, std::int64_t maximum ) -> std::int64_t {
            whitespace();
            const auto negative = accept( '-' );
            require( digit( peek() ) );
            const auto start = offset_;
            if( accept( '0' ) ) {
                if( digit( peek() ) ) { throw decode_error::invalid_json; }
            } else { while( digit( peek() ) ) { ++offset_; } }
            auto fractional_digits = std::int64_t{0};
            if( accept( '.' ) ) {
                if( !digit( peek() ) ) { throw decode_error::invalid_json; }
                while( digit( peek() ) ) { ++fractional_digits; ++offset_; }
            }
            const auto coefficient = input_.substr( start, offset_ - start );
            auto exponent = std::int64_t{0};
            if( accept( 'e' ) || accept( 'E' ) ) {
                const auto exponent_negative = accept( '-' );
                if( !exponent_negative ) { accept( '+' ); }
                if( !digit( peek() ) ) { throw decode_error::invalid_json; }
                const auto saturation = static_cast<std::int64_t>( maximum_inline_bytes ) + 32;
                while( digit( peek() ) ) {
                    exponent = std::min( saturation, exponent * 10 + take() - '0' );
                }
                if( exponent_negative ) { exponent = -exponent; }
            }
            auto significant = std::int64_t{0};
            auto trailing_zeroes = std::int64_t{0};
            for( const auto byte : coefficient ) {
                if( byte == '.' ) { continue; }
                if( significant == 0 && byte == '0' ) { continue; }
                ++significant;
                trailing_zeroes = byte == '0' ? trailing_zeroes + 1 : 0;
            }
            if( significant == 0 ) { require( minimum <= 0 && maximum >= 0 ); return 0; }
            const auto scale = exponent - fractional_digits;
            require( scale >= -trailing_zeroes );
            const auto integral_digits = significant + scale;
            require( integral_digits > 0 && integral_digits <= 19 );
            auto magnitude = std::uint64_t{0};
            auto kept = std::int64_t{0};
            for( const auto byte : coefficient ) {
                if( byte == '.' || ( kept == 0 && byte == '0' ) ) { continue; }
                if( kept == integral_digits ) { break; }
                magnitude = magnitude * 10 + byte - '0';
                ++kept;
            }
            while( kept++ < integral_digits ) { magnitude *= 10; }
            if( negative ) {
                const auto bound = minimum < 0 ? static_cast<std::uint64_t>( -( minimum + 1 ) ) + 1 : 0;
                require( magnitude <= bound );
                // Supported request fields never use INT64_MIN.
                require( magnitude <= static_cast<std::uint64_t>( std::numeric_limits<std::int64_t>::max() ) );
                const auto result = -static_cast<std::int64_t>( magnitude );
                require( result <= maximum );
                return result;
            }
            require( maximum >= 0 && magnitude <= static_cast<std::uint64_t>( maximum ) );
            const auto result = static_cast<std::int64_t>( magnitude );
            require( result >= minimum );
            return result;
        }
        auto strings() -> std::vector<std::string> {
            whitespace();
            require( peek() == '[' );
            ++offset_;
            auto values = std::vector<std::string> {};
            auto seen = std::set<std::string> {};
            whitespace();
            if( accept( ']' ) ) { return values; }
            while( true ) {
                auto value = string();
                require( seen.insert( value ).second );
                values.push_back( std::move( value ) );
                whitespace();
                if( accept( ']' ) ) { return values; }
                expect( ',' );
            }
        }
        /// Bit positions correspond to the fixed names supplied by the typed grammar.
        template<typename ReadMember>
        auto object( std::initializer_list<std::string_view> names, ReadMember read_member ) -> unsigned {
            whitespace();
            require( peek() == '{' );
            ++offset_;
            auto seen = 0u;
            whitespace();
            if( accept( '}' ) ) { return seen; }
            while( true ) {
                const auto key = string( 32 );
                const auto found = std::ranges::find( names, key );
                require( found != names.end() );
                const auto index = static_cast<unsigned>( found - names.begin() );
                const auto bit = 1u << index;
                require( ( seen & bit ) == 0 );
                seen |= bit;
                whitespace();
                expect( ':' );
                read_member( index );
                whitespace();
                if( accept( '}' ) ) { return seen; }
                expect( ',' );
            }
        }
    private:
        std::string_view input_;
        std::size_t offset_ = 0;
        auto peek() const -> char { return offset_ == input_.size() ? '\0' : input_[offset_]; }
        auto take() -> char {
            if( offset_ == input_.size() ) { throw decode_error::invalid_json; }
            return input_[offset_++];
        }
        auto accept( char byte ) -> bool {
            if( offset_ < input_.size() && input_[offset_] == byte ) { ++offset_; return true; }
            return false;
        }
        auto expect( char byte ) -> void { if( !accept( byte ) ) { throw decode_error::invalid_json; } }
        auto consume( std::string_view token ) -> bool {
            if( input_.substr( offset_, token.size() ) != token ) { return false; }
            offset_ += token.size();
            return true;
        }
        auto whitespace() -> void {
            while( peek() == ' ' || peek() == '\t' || peek() == '\r' || peek() == '\n' ) { ++offset_; }
        }
        auto hex_unit() -> std::uint32_t {
            auto value = std::uint32_t{0};
            for( auto count = 0; count < 4; ++count ) {
                const auto byte = take();
                auto hex = 0;
                if( digit( byte ) ) { hex = byte - '0'; }
                else if( byte >= 'a' && byte <= 'f' ) { hex = byte - 'a' + 10; }
                else if( byte >= 'A' && byte <= 'F' ) { hex = byte - 'A' + 10; }
                else { throw decode_error::invalid_json; }
                value = ( value << 4 ) | hex;
            }
            return value;
        }
};

auto read_position( request_reader &reader ) -> coordinate
{
    auto value = coordinate{};
    const auto fields = reader.object( {"space", "frame_id", "x", "y", "z"}, [&]( auto index ) {
        switch( index ) {
            case 0:
                value.space = reader.string();
                require( value.space == "reality_bubble_map_square" );
                break;
            case 1:
                value.frame_id = reader.id();
                break;
            case 2:
                value.position.x = static_cast<int>( reader.integer( INT32_MIN, INT32_MAX ) );
                break;
            case 3:
                value.position.y = static_cast<int>( reader.integer( INT32_MIN, INT32_MAX ) );
                break;
            case 4:
                value.position.z = static_cast<int>( reader.integer( INT32_MIN, INT32_MAX ) );
                break;
        }
    } );
    require( fields == 31 );
    return value;
}
auto read_operation( request_reader &reader ) -> std::variant<semantic_operation, registered_action>
{
    auto kind = std::string{};
    auto semantic = semantic_operation{};
    auto action_id = std::string{};
    const auto fields = reader.object( {"kind", "choice_id", "field_id", "value", "submit",
                                        "count", "candidate_id", "position", "action_id"},
    [&]( auto index ) {
        switch( index ) {
            case 0:
                kind = reader.string();
                break;
            case 1:
            case 2:
            case 6:
                semantic.command.target_id = reader.id();
                break;
            case 3:
                semantic.command.value = reader.string();
                break;
            case 4:
                semantic.command.submit = reader.boolean();
                break;
            case 5:
                semantic.command.count = reader.integer( 0, maximum_safe_integer );
                break;
            case 7:
                semantic.target = read_position( reader );
                break;
            case 8:
                action_id = reader.id();
                break;
        }
    } );
    // Exact presence masks in the member order above, not truthiness of native defaults.
    // This rejects fields from other alternatives even when empty, false or zero.
    if( kind == "invoke_registered_action" ) {
        require( fields == ( 1u | 256u ) );
        return registered_action{ .id = std::move( action_id ) };
    }
    const auto parsed = game_client::parse_interaction_operation( kind );
    require( parsed.has_value() );
    semantic.command.operation = *parsed;
    switch( *parsed ) {
        case game_client::interaction_operation::choose:
            require( fields == ( 1u | 2u ) );
            break;
        case game_client::interaction_operation::fill:
            require( fields == ( 1u | 4u | 8u | 16u ) );
            break;
        case game_client::interaction_operation::set_count:
            require( fields == ( 1u | 2u | 32u ) );
            break;
        case game_client::interaction_operation::set_target:
            require( fields == ( 1u | 128u ) || fields == ( 1u | 64u | 128u ) );
            break;
        case game_client::interaction_operation::cancel:
            require( fields == 1u );
            break;
    }
    return semantic;
}
auto read_request( request_reader &reader, negotiation_request &value ) -> void
{
    const auto fields = reader.object( {"supported_versions", "required_capabilities", "optional_capabilities"},
    [&]( auto index ) {
        switch( index ) {
            case 0:
                value.supported_versions = reader.strings();
                break;
            case 1:
                value.required_capabilities = reader.strings();
                break;
            case 2:
                value.optional_capabilities = reader.strings();
                break;
        }
    } );
    require( fields == 7 && !value.supported_versions.empty() );
}
auto read_request( request_reader &reader, snapshot_request &value ) -> void
{
    const auto fields = reader.object( {"session_epoch", "page"}, [&]( auto index ) {
        if( index == 0 ) { value.session_epoch = reader.id(); }
        else {
            const auto page_fields = reader.object( {"offset", "limit"}, [&]( auto page_index ) {
                if( page_index == 0 ) {
                    const auto offset = reader.integer( 0, maximum_safe_integer );
                    require( static_cast<std::uint64_t>( offset ) <= std::numeric_limits<std::size_t>::max() );
                    value.page.offset = static_cast<std::size_t>( offset );
                } else { value.page.limit = static_cast<std::size_t>( reader.integer( 1, maximum_rows ) ); }
            } );
            require( page_fields == 3 );
        }
    } );
    require( fields == 3 );
}
auto read_request( request_reader &reader, command_request &value ) -> void
{
    const auto fields = reader.object( {"session_epoch", "based_on", "operation"}, [&]( auto index ) {
        switch( index ) {
            case 0:
                value.session_epoch = reader.id();
                break;
            case 1: {
                const auto basis = reader.object( {"state_revision", "input_boundary_id", "interaction_schema_id"},
                [&]( auto basis_index ) {
                    switch( basis_index ) {
                        case 0:
                            value.state_revision = reader.counter_value();
                            break;
                        case 1:
                            value.input_boundary_id = reader.id();
                            break;
                        case 2:
                            value.interaction_schema_id = reader.id();
                            break;
                    }
                } );
                require( basis == 3 || basis == 7 );
                break;
            }
            case 2:
                value.operation = read_operation( reader );
                break;
        }
    } );
    require( fields == 7 );
    require( std::holds_alternative<registered_action>( value.operation ) ||
             !value.interaction_schema_id.empty() );
}
auto read_request( request_reader &reader, result_request &value ) -> void
{
    const auto fields = reader.object( {"session_epoch", "command_id"}, [&]( auto index ) {
        if( index == 0 ) { value.session_epoch = reader.id(); }
        else { value.command_id = reader.id(); }
    } );
    require( fields == 3 );
}
template<typename Request>
auto decode( std::string_view input ) -> std::expected<Request, decode_error>
{
    try {
        auto reader = request_reader{input};
        auto result = Request{};
        read_request( reader, result );
        reader.finish();
        return result;
    } catch( const decode_error failure ) { return std::unexpected( failure ); }
}
auto stage_name( application_error_stage value ) -> std::optional<std::string_view>
{
    switch( value ) {
        case application_error_stage::negotiation:
            return "negotiation";
        case application_error_stage::receipt:
            return "receipt";
        case application_error_stage::validation:
            return "validation";
        case application_error_stage::execution:
            return "execution";
        case application_error_stage::completion:
            return "completion";
    }
    return std::nullopt;
}
auto action_name( required_action value ) -> std::optional<std::string_view>
{
    switch( value ) {
        case required_action::negotiate:
            return "negotiate";
        case required_action::read_snapshot:
            return "read_snapshot";
        case required_action::retry:
            return "retry";
        case required_action::none:
            return "none";
    }
    return std::nullopt;
}
auto bounded_output( std::ostringstream &output ) -> std::expected<std::string, error>
{
    auto result = output.str();
    if( result.size() > maximum_inline_bytes ) { return std::unexpected( error::resource_limit ); }
    return result;
}
} // namespace

auto decode_negotiation_request( std::string_view input ) ->
std::expected<negotiation_request, decode_error>
{ return decode<negotiation_request>( input ); }
auto decode_snapshot_request( std::string_view input ) ->
std::expected<snapshot_request, decode_error>
{ return decode<snapshot_request>( input ); }
auto decode_command_request( std::string_view input ) ->
std::expected<command_request, decode_error>
{ return decode<command_request>( input ); }
auto decode_result_request( std::string_view input ) -> std::expected<result_request, decode_error>
{ return decode<result_request>( input ); }

auto serialize_receipt( const receipt &value ) -> std::expected<std::string, error>
{
    if( !valid_id( value.session_epoch ) || !valid_id( value.command_id ) ) {
        return std::unexpected( error::validation_failed );
    }
    auto output = std::ostringstream{};
    auto out = JsonOut{output};
    out.start_object();
    out.member( "session_epoch", value.session_epoch );
    out.member( "command_id", value.command_id );
    out.member( "stage", "received" );
    out.end_object();
    return bounded_output( output );
}
auto serialize_application_error( const application_error &value ) ->
std::expected<std::string, error>
{
    const auto stage = stage_name( value.stage );
    const auto action = action_name( value.action );
    // error is a contiguous closed enum in the accepted core; reject invalid native enum casts.
    if( !stage || !action || value.kind < error::negotiation_failed ||
        value.kind > error::resource_limit ||
        ( value.current && !valid_id( value.current->session_epoch ) ) ) {
        return std::unexpected( error::validation_failed );
    }
    auto output = std::ostringstream{};
    auto out = JsonOut{output};
    out.start_object();
    out.member( "kind", error_name( value.kind ) );
    out.member( "stage", std::string( *stage ) );
    out.member( "retryable", value.retryable );
    out.member( "required_action", std::string( *action ) );
    if( value.current ) {
        out.member( "current" );
        out.start_object();
        out.member( "session_epoch", value.current->session_epoch );
        out.member( "state_revision", std::to_string( value.current->state_revision ) );
        out.member( "through_public_sequence", std::to_string( value.current->through_public_sequence ) );
        out.end_object();
    }
    out.end_object();
    return bounded_output( output );
}
} // namespace engine_client
