#include "engine_client_wire.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <nlohmann/json.hpp>
#include <set>

namespace engine_client
{
namespace
{
using json = nlohmann::json;

auto require( const bool condition ) -> void
{
    if( !condition ) { throw decode_error::invalid_params; }
}
/// Length in Unicode scalar values, which is what the schema's maxLength counts.
auto scalar_count( const std::string &text ) -> std::size_t
{
    return static_cast<std::size_t>( std::ranges::count_if( text, []( const char byte ) {
        return ( static_cast<unsigned char>( byte ) & 0xC0 ) != 0x80;
    } ) );
}
/// The value must be an object whose members are all listed; `required` ones must be present.
auto members( const json &value, const std::initializer_list<const char *> required,
const std::initializer_list<const char *> optional = {} ) -> void {
    require( value.is_object() );
    for( const auto &entry : value.items() )
    {
        require( std::ranges::find( required, entry.key() ) != required.end() ||
                 std::ranges::find( optional, entry.key() ) != optional.end() );
    }
    for( const auto *name : required ) { require( value.contains( name ) ); }
}
auto text( const json &object, const char *name ) -> std::string
{
    const auto &value = object.at( name );
    require( value.is_string() );
    return value.get<std::string>();
}
auto id( const json &object, const char *name ) -> std::string
{
    auto value = text( object, name );
    require( !value.empty() && scalar_count( value ) <= maximum_id_bytes );
    return value;
}
/// An integer, or an integral float such as 1.0 (JSON Schema `integer`), within [low, high].
auto integer( const json &object, const char *name, const std::int64_t low,
              const std::int64_t high )
-> std::int64_t
{
    const auto &value = object.at( name );
    require( value.is_number() );
    if( value.is_number_unsigned() ) {
        require( value.get<std::uint64_t>() <= static_cast<std::uint64_t>( high ) &&
                 low <= static_cast<std::int64_t>( value.get<std::uint64_t>() ) );
        return static_cast<std::int64_t>( value.get<std::uint64_t>() );
    }
    if( value.is_number_integer() ) {
        const auto result = value.get<std::int64_t>();
        require( result >= low && result <= high );
        return result;
    }
    const auto real = value.get<double>();
    require( std::isfinite( real ) && std::floor( real ) == real &&
             std::fabs( real ) <= 9007199254740991.0 );
    require( real >= static_cast<double>( low ) && real <= static_cast<double>( high ) );
    return static_cast<std::int64_t>( real );
}
auto counter_of( const json &object, const char *name ) -> counter
{
    const auto value = text( object, name );
    require( !value.empty() && value.size() <= 20 && ( value.size() == 1 || value.front() != '0' ) &&
    std::ranges::all_of( value, []( const char byte ) { return byte >= '0' && byte <= '9'; } ) );
    auto result = counter{0};
    const auto parsed = std::from_chars( value.data(), value.data() + value.size(), result );
    require( parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size() );
    return result;
}
auto read_position( const json &value ) -> position
{
    members( value, {"dim", "x", "y", "z"} );
    return {
        .dim = text( value, "dim" ),
        .x = static_cast<int>( integer( value, "x", INT32_MIN, INT32_MAX ) ),
        .y = static_cast<int>( integer( value, "y", INT32_MIN, INT32_MAX ) ),
        .z = static_cast<int>( integer( value, "z", INT32_MIN, INT32_MAX ) ),
    };
}
auto read_operation( const json &value ) -> std::variant<semantic_operation, registered_action>
{
    require( value.is_object() && value.contains( "kind" ) );
    const auto kind = text( value, "kind" );
    if( kind == "action" ) {
        members( value, {"kind", "action_id"} );
        return registered_action{ .id = id( value, "action_id" ) };
    }
    const auto parsed = game_client::parse_interaction_operation( kind );
    require( parsed.has_value() );
    auto semantic = semantic_operation{};
    semantic.command.operation = *parsed;
    switch( *parsed ) {
        case game_client::interaction_operation::choose:
            members( value, {"kind", "choice_id"} );
            semantic.command.target_id = id( value, "choice_id" );
            break;
        case game_client::interaction_operation::fill:
            members( value, {"kind", "field_id", "value", "submit"} );
            semantic.command.target_id = id( value, "field_id" );
            semantic.command.value = text( value, "value" );
            require( value.at( "submit" ).is_boolean() );
            semantic.command.submit = value.at( "submit" ).get<bool>();
            break;
        case game_client::interaction_operation::set_count:
            members( value, {"kind", "choice_id", "count"} );
            semantic.command.target_id = id( value, "choice_id" );
            semantic.command.count = static_cast<std::uint64_t>(
                                         integer( value, "count", 0, static_cast<std::int64_t>( maximum_safe_integer ) ) );
            break;
        case game_client::interaction_operation::set_target:
            members( value, {"kind", "pos"}, {"candidate_id"} );
            if( value.contains( "candidate_id" ) ) { semantic.command.target_id = id( value, "candidate_id" ); }
            semantic.target = read_position( value.at( "pos" ) );
            break;
        case game_client::interaction_operation::cancel:
            members( value, {"kind"} );
            break;
    }
    return semantic;
}

struct empty_request {};
auto read_request( const json &value, hello_request &result ) -> void
{
    members( value, {"versions", "client"} );
    const auto &versions = value.at( "versions" );
    require( versions.is_array() && !versions.empty() );
    auto seen = std::set<std::string> {};
    for( const auto &entry : versions ) {
        require( entry.is_string() && seen.insert( entry.get<std::string>() ).second );
        result.versions.push_back( entry.get<std::string>() );
    }
    const auto &client = value.at( "client" );
    members( client, {"name", "version"} );
    result.client_name = text( client, "name" );
    result.client_version = text( client, "version" );
}
auto read_request( const json &value, empty_request & /*result*/ ) -> void { members( value, {} ); }
auto read_request( const json &value, choices_request &result ) -> void
{
    members( value, {"epoch", "boundary_id", "offset", "limit"} );
    result.epoch = id( value, "epoch" );
    result.boundary_id = id( value, "boundary_id" );
    result.offset = static_cast<std::size_t>( integer( value, "offset", 0,
                    static_cast<std::int64_t>( maximum_safe_integer ) ) );
    result.limit = static_cast<std::size_t>( integer( value, "limit", 1,
                   static_cast<std::int64_t>( maximum_rows ) ) );
}
auto read_request( const json &value, command_request &result ) -> void
{
    members( value, {"epoch", "expect", "operation"} );
    result.epoch = id( value, "epoch" );
    const auto &expect = value.at( "expect" );
    members( expect, {"revision", "boundary_id", "schema_id"} );
    result.expect.revision = counter_of( expect, "revision" );
    result.expect.boundary_id = id( expect, "boundary_id" );
    if( !expect.at( "schema_id" ).is_null() ) { result.expect.schema_id = id( expect, "schema_id" ); }
    result.operation = read_operation( value.at( "operation" ) );
}
auto read_request( const json &value, result_request &result ) -> void
{
    members( value, {"epoch", "command_id"} );
    result.epoch = id( value, "epoch" );
    result.command_id = id( value, "command_id" );
}
template<typename Request>
auto decode( const std::string_view input ) -> std::expected<Request, decode_error>
{
    if( input.size() > maximum_inline_bytes ) { return std::unexpected( decode_error::resource_limit ); }
    // A raw NUL ends the parser's input and would hide a suffix.
    auto value = json( json::value_t::discarded );
    if( input.find( '\0' ) == std::string_view::npos ) { value = json::parse( input, nullptr, false ); }
    if( value.is_discarded() ) { return std::unexpected( decode_error::invalid_json ); }
    try {
        auto result = Request{};
        read_request( value, result );
        return result;
    } catch( const decode_error failure ) { return std::unexpected( failure ); }
}
auto action_name( const required_action value ) -> std::optional<std::string_view>
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
auto valid_id( const std::string &value ) -> bool
{
    return !value.empty() && scalar_count( value ) <= maximum_id_bytes;
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
    using ordered = nlohmann::ordered_json;
    return ordered{{"version", contract_version}, {"epoch", epoch},
        {"engine", {{"build", engine.build}, {"mods", engine.mods}}},
        {
            "limits", {{"frame_bytes", maximum_frame_bytes}, {"cells_per_part", cells_per_part},
                {"cells_per_query", cells_per_query}
            }
        }}.dump();
}
auto serialize_receipt( const receipt &value ) -> std::expected<std::string, error>
{
    if( !valid_id( value.epoch ) || !valid_id( value.command_id ) ) {
        return std::unexpected( error::validation_failed );
    }
    return nlohmann::ordered_json{{"epoch", value.epoch}, {"command_id", value.command_id},
        {"stage", "received"}}.dump();
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
    auto result = nlohmann::ordered_json{{"kind", error_name( value.kind )}};
    if( !action->empty() ) { result["action"] = std::string( *action ); }
    if( value.at ) { result["at"] = nlohmann::ordered_json::parse( serialize_clock( *value.at ) ); }
    return result.dump();
}
} // namespace engine_client
