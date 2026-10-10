#include "engine_client_wire.h"
#include "engine_client_utf8.h"
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
auto valid_id( std::string_view value ) -> bool
{
    if( value.empty() || value.size() > maximum_id_bytes ) { return false; }
    auto offset = std::size_t{0};
    while( offset < value.size() ) {
        if( !utf8_scalar( value, offset ) ) { return false; }
    }
    return true;
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
        auto optional_id() -> std::optional<std::string> {
            whitespace();
            if( consume( "null" ) ) { return std::nullopt; }
            return id();
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

auto read_position( request_reader &reader ) -> position
{
    auto value = position{};
    const auto fields = reader.object( {"dim", "x", "y", "z"}, [&]( auto index ) {
        switch( index ) {
            case 0:
                value.dim = reader.string( maximum_id_bytes );
                break;
            case 1:
                value.x = static_cast<int>( reader.integer( INT32_MIN, INT32_MAX ) );
                break;
            case 2:
                value.y = static_cast<int>( reader.integer( INT32_MIN, INT32_MAX ) );
                break;
            case 3:
                value.z = static_cast<int>( reader.integer( INT32_MIN, INT32_MAX ) );
                break;
        }
    } );
    require( fields == 15 );
    return value;
}
auto read_operation( request_reader &reader ) -> std::variant<semantic_operation, registered_action>
{
    auto kind = std::string{};
    auto semantic = semantic_operation{};
    auto action_id = std::string{};
    const auto fields = reader.object( {"kind", "choice_id", "field_id", "value", "submit",
                                        "count", "candidate_id", "pos", "action_id"},
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
    if( kind == "action" ) {
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
auto read_request( request_reader &reader, hello_request &value ) -> void
{
    const auto fields = reader.object( {"versions", "client"}, [&]( auto index ) {
        if( index == 0 ) {
            value.versions = reader.strings();
        } else {
            const auto client = reader.object( {"name", "version"}, [&]( auto client_index ) {
                ( client_index == 0 ? value.client_name : value.client_version ) = reader.string(
                            maximum_id_bytes );
            } );
            require( client == 3 );
        }
    } );
    require( fields == 3 && !value.versions.empty() );
}
auto read_request( request_reader &reader, choices_request &value ) -> void
{
    const auto fields = reader.object( {"epoch", "boundary_id", "offset", "limit"}, [&]( auto index ) {
        switch( index ) {
            case 0:
                value.epoch = reader.id();
                break;
            case 1:
                value.boundary_id = reader.id();
                break;
            case 2: {
                const auto offset = reader.integer( 0, maximum_safe_integer );
                require( static_cast<std::uint64_t>( offset ) <= std::numeric_limits<std::size_t>::max() );
                value.offset = static_cast<std::size_t>( offset );
                break;
            }
            case 3:
                value.limit = static_cast<std::size_t>( reader.integer( 1, maximum_rows ) );
                break;
        }
    } );
    require( fields == 15 );
}
auto read_request( request_reader &reader, command_request &value ) -> void
{
    const auto fields = reader.object( {"epoch", "expect", "operation"}, [&]( auto index ) {
        switch( index ) {
            case 0:
                value.epoch = reader.id();
                break;
            case 1: {
                const auto expect = reader.object( {"revision", "boundary_id", "schema_id"},
                [&]( auto expect_index ) {
                    switch( expect_index ) {
                        case 0:
                            value.expect.revision = reader.counter_value();
                            break;
                        case 1:
                            value.expect.boundary_id = reader.id();
                            break;
                        case 2:
                            value.expect.schema_id = reader.optional_id();
                            break;
                    }
                } );
                require( expect == 7 );
                break;
            }
            case 2:
                value.operation = read_operation( reader );
                break;
        }
    } );
    require( fields == 7 );
}
auto read_request( request_reader &reader, result_request &value ) -> void
{
    const auto fields = reader.object( {"epoch", "command_id"}, [&]( auto index ) {
        if( index == 0 ) { value.epoch = reader.id(); }
        else { value.command_id = reader.id(); }
    } );
    require( fields == 3 );
}
struct empty_request {};
auto read_request( request_reader &reader, empty_request & /*value*/ ) -> void
{
    require( reader.object( {}, []( auto /*index*/ ) {} ) == 0 );
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
auto action_name( required_action value ) -> std::optional<std::string_view>
{
    switch( value ) {
        case required_action::none:
            return std::string_view{};
        case required_action::hello:
            return "hello";
        case required_action::subscribe:
            return "subscribe";
        case required_action::retry:
            return "retry";
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

auto decode_hello_request( std::string_view input ) -> std::expected<hello_request, decode_error>
{ return decode<hello_request>( input ); }
auto decode_empty_request( std::string_view input ) -> std::expected<void, decode_error>
{
    const auto decoded = decode<empty_request>( input );
    if( !decoded ) { return std::unexpected( decoded.error() ); }
    return {};
}
auto decode_choices_request( std::string_view input ) ->
std::expected<choices_request, decode_error>
{ return decode<choices_request>( input ); }
auto decode_command_request( std::string_view input ) ->
std::expected<command_request, decode_error>
{ return decode<command_request>( input ); }
auto decode_result_request( std::string_view input ) -> std::expected<result_request, decode_error>
{ return decode<result_request>( input ); }

auto serialize_hello( const std::string &epoch, const engine_info &engine ) -> std::string
{
    auto output = std::ostringstream{};
    auto out = JsonOut{output};
    out.start_object();
    out.member( "version", contract_version );
    out.member( "epoch", epoch );
    out.member( "engine" );
    out.start_object();
    out.member( "build", engine.build );
    out.member( "mods", engine.mods );
    out.end_object();
    out.member( "limits" );
    out.start_object();
    out.member( "frame_bytes", maximum_frame_bytes );
    out.member( "cells_per_part", cells_per_part );
    out.member( "cells_per_query", cells_per_query );
    out.end_object();
    out.end_object();
    return output.str();
}
auto serialize_receipt( const receipt &value ) -> std::expected<std::string, error>
{
    if( !valid_id( value.epoch ) || !valid_id( value.command_id ) ) {
        return std::unexpected( error::validation_failed );
    }
    auto output = std::ostringstream{};
    auto out = JsonOut{output};
    out.start_object();
    out.member( "epoch", value.epoch );
    out.member( "command_id", value.command_id );
    out.member( "stage", "received" );
    out.end_object();
    return bounded_output( output );
}
auto serialize_application_error( const application_error &value ) ->
std::expected<std::string, error>
{
    const auto action = action_name( value.action );
    // error is a contiguous closed enum; reject invalid native enum casts.
    if( !action || value.kind < error::negotiation_failed || value.kind > error::resource_limit ||
        ( value.at && !valid_id( value.at->epoch ) ) ) {
        return std::unexpected( error::validation_failed );
    }
    auto output = std::ostringstream{};
    auto out = JsonOut{output};
    out.start_object();
    out.member( "kind", error_name( value.kind ) );
    if( !action->empty() ) { out.member( "action", std::string( *action ) ); }
    if( value.at ) {
        out.member( "at" );
        *out.get_stream() << serialize_clock( *value.at );
        out.set_need_separator();
    }
    out.end_object();
    return bounded_output( output );
}
} // namespace engine_client
