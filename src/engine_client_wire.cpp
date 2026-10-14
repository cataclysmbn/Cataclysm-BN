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
auto read_invoke_args( const json &value ) -> invoke_args
{
    members( value, {}, {"row", "rows", "count", "text", "pos", "pane"} );
    auto result = invoke_args{};
    if( value.contains( "row" ) ) { result.row = id( value, "row" ); }
    if( value.contains( "rows" ) ) {
        const auto &rows = value.at( "rows" );
        require( rows.is_array() && !rows.empty() );
        result.rows.emplace();
        for( const auto &entry : rows ) {
            members( entry, {"id"}, {"count"} );
            result.rows->push_back( { .id = id( entry, "id" ), .count = entry.contains( "count" ) ?
                                      std::optional{ static_cast<std::uint64_t>( integer( entry, "count", 0, static_cast<std::int64_t>( maximum_safe_integer ) ) ) } :
                                      std::nullopt } );
        }
    }
    if( value.contains( "count" ) ) {
        constexpr auto limit = static_cast<std::int64_t>( maximum_safe_integer );
        result.count = integer( value, "count", -limit, limit );
    }
    if( value.contains( "text" ) ) { result.text = text( value, "text" ); }
    if( value.contains( "pos" ) ) { result.pos = read_position( value.at( "pos" ) ); }
    if( value.contains( "pane" ) ) { result.pane = id( value, "pane" ); }
    return result;
}
auto read_operation( const json &value ) -> command_operation
{
    require( value.is_object() && value.contains( "kind" ) );
    const auto kind = text( value, "kind" );
    if( kind == "invoke" ) {
        members( value, {"kind", "affordance_id"}, {"args"} );
        return invoke_operation{ .affordance_id = id( value, "affordance_id" ),
                                 .args = value.contains( "args" ) ? read_invoke_args( value.at( "args" ) ) : invoke_args{} };
    }
    if( kind == "action" ) {
        members( value, {"kind", "action_id"} );
        return registered_action{ .id = id( value, "action_id" ) };
    }
    if( kind == "travel" || kind == "context" ) {
        members( value, {"kind", "pos"} );
        return travel_operation{ .target = read_position( value.at( "pos" ) ),
                                 .button = kind == "travel" ? "left" : "right" };
    }
    if( kind == "pickup" || kind == "drop" ) {
        members( value, {"kind", "item_id", "count"} );
        return item_operation{ .drop = kind == "drop", .item_id = id( value, "item_id" ),
                               .count = static_cast<std::uint64_t>( integer( value, "count", 1, static_cast<std::int64_t>( maximum_safe_integer ) ) ) };
    }
    const auto parsed = game_client::parse_interaction_operation( kind );
    // `invoke` is read above.
    require( parsed.has_value() && *parsed != game_client::interaction_operation::invoke );
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
        case game_client::interaction_operation::invoke:
            break;
    }
    return semantic;
}

struct empty_request {};
auto read_request( const json &value, viewport &result ) -> void
{
    members( value, {"cols", "rows"} );
    result.cols = static_cast<int>( integer( value, "cols", 1, maximum_viewport_cells ) );
    result.rows = static_cast<int>( integer( value, "rows", 1, maximum_viewport_cells ) );
}
/// What the client can do: the hello's loose members, or the same members under `capabilities`.
auto read_capabilities( const json &source, hello_request &result ) -> void
{
    if( source.contains( "delivery" ) ) {
        const auto &delivery = source.at( "delivery" );
        members( delivery, {"credits"}, {"max_hz"} );
        if( delivery.contains( "max_hz" ) ) {
            result.max_hz = static_cast<std::size_t>( integer( delivery, "max_hz", 1,
                            static_cast<std::int64_t>( maximum_frame_rate ) ) );
        }
        result.credits = static_cast<std::size_t>( integer( delivery, "credits", 1,
                         static_cast<std::int64_t>( maximum_credits ) ) );
    }
    if( source.contains( "tint_ids" ) ) {
        const auto &ids = source.at( "tint_ids" );
        require( ids.is_array() && ids.size() <= maximum_tint_ids );
        for( const auto &id : ids ) {
            require( id.is_string() && !id.get<std::string>().empty() && id.get<std::string>().size() <= 128 );
            result.tint_ids.push_back( id.get<std::string>() );
        }
    }
    if( source.contains( "viewport" ) ) { read_request( source.at( "viewport" ), result.view.emplace() ); }
    if( source.contains( "tiles" ) ) {
        require( source.at( "tiles" ).is_boolean() );
        result.tiles = source.at( "tiles" ).get<bool>();
    }
}
/// `sha256:` and 16 lowercase hex digits.
auto read_schema_hash( const json &object, const char *name ) -> std::string
{
    auto value = text( object, name );
    require( value.size() == 23 && value.starts_with( "sha256:" ) &&
    std::ranges::all_of( value.substr( 7 ), []( const char digit ) {
        return ( digit >= '0' && digit <= '9' ) || ( digit >= 'a' && digit <= 'f' );
    } ) );
    return value;
}
auto read_request( const json &value, hello_request &result ) -> void
{
    // Only a client that offers the version sends (and must send) the schema; any other hello is refused by the negotiation.
    const auto offers_02 = value.is_object() && value.contains( "versions" ) &&
                           value.at( "versions" ).is_array() &&
    std::ranges::any_of( value.at( "versions" ), []( const json & entry ) { return entry == contract_version; } );
    if( offers_02 ) {
        members( value, {"versions", "client", "schema"}, {"locale", "capabilities", "viewport", "tiles", "tint_ids", "delivery"} );
        result.schema = read_schema_hash( value, "schema" );
        if( value.contains( "locale" ) ) { result.locale = id( value, "locale" ); }
    } else {
        members( value, {"versions", "client"}, {"viewport", "tiles", "tint_ids", "delivery"} );
    }
    if( value.contains( "capabilities" ) ) {
        const auto &capabilities = value.at( "capabilities" );
        // The same member is not said twice.
        require( !value.contains( "viewport" ) && !value.contains( "tiles" ) &&
                 !value.contains( "tint_ids" ) &&
                 !value.contains( "delivery" ) );
        members( capabilities, {}, {"tiles", "tint_ids", "viewport", "delivery"} );
        read_capabilities( capabilities, result );
    } else {
        read_capabilities( value, result );
    }
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
auto read_request( const json &value, credit_request &result ) -> void
{
    members( value, {"frames"} );
    result.frames = static_cast<std::size_t>( integer( value, "frames", 1,
                    static_cast<std::int64_t>( maximum_credits ) ) );
}
auto read_request( const json &value, hold_request &result ) -> void
{
    members( value, {"epoch", "screen_id", "affordance_id"} );
    result.epoch = id( value, "epoch" );
    result.screen_id = id( value, "screen_id" );
    result.affordance_id = id( value, "affordance_id" );
}
auto read_request( const json &value, choices_request &result ) -> void
{
    members( value, {"epoch", "boundary_id", "offset", "limit"}, {"component_id"} );
    result.epoch = id( value, "epoch" );
    result.boundary_id = id( value, "boundary_id" );
    if( value.contains( "component_id" ) ) { result.component_id = id( value, "component_id" ); }
    result.offset = static_cast<std::size_t>( integer( value, "offset", 0,
                    static_cast<std::int64_t>( maximum_safe_integer ) ) );
    result.limit = static_cast<std::size_t>( integer( value, "limit", 1,
                   static_cast<std::int64_t>( maximum_rows ) ) );
}
auto read_request( const json &value, describe_request &result ) -> void
{
    members( value, {"epoch", "boundary_id", "pos"} );
    result.epoch = id( value, "epoch" );
    result.boundary_id = id( value, "boundary_id" );
    result.target = read_position( value.at( "pos" ) );
}
auto read_request( const json &value, cells_request &result ) -> void
{
    members( value, {"epoch", "min", "max"} );
    result.epoch = id( value, "epoch" );
    result.min = read_position( value.at( "min" ) );
    result.max = read_position( value.at( "max" ) );
}
auto read_request( const json &value, command_request &result ) -> void
{
    members( value, {"epoch", "expect", "operation"} );
    result.epoch = id( value, "epoch" );
    result.operation = read_operation( value.at( "operation" ) );
    const auto &expect = value.at( "expect" );
    if( !std::holds_alternative<invoke_operation>( result.operation ) ) {
        members( expect, {"revision", "boundary_id", "schema_id"} );
        result.expect.revision = counter_of( expect, "revision" );
        result.expect.boundary_id = id( expect, "boundary_id" );
        if( !expect.at( "schema_id" ).is_null() ) { result.expect.schema_id = id( expect, "schema_id" ); }
    } else if( expect.is_object() && expect.contains( "screen_id" ) ) {
        // An invoke expects its screen, or the boundary and its revision, and names no schema.
        members( expect, {"screen_id"} );
        result.expect.screen_id = id( expect, "screen_id" );
    } else {
        members( expect, {"boundary_id", "revision"} );
        result.expect.revision = counter_of( expect, "revision" );
        result.expect.boundary_id = id( expect, "boundary_id" );
    }
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
auto decode_viewport_request( std::string_view input ) -> std::expected<viewport, decode_error>
{ return decode<viewport>( input ); }
auto decode_empty_request( std::string_view input ) -> std::expected<void, decode_error>
{
    const auto decoded = decode<empty_request>( input );
    if( !decoded ) { return std::unexpected( decoded.error() ); }
    return {};
}
auto decode_credit_request( std::string_view input ) -> std::expected<credit_request, decode_error>
{ return decode<credit_request>( input ); }
auto decode_hold_request( std::string_view input ) -> std::expected<hold_request, decode_error>
{ return decode<hold_request>( input ); }
auto decode_choices_request( std::string_view input ) ->
std::expected<choices_request, decode_error>
{ return decode<choices_request>( input ); }
auto decode_describe_request( std::string_view input ) ->
std::expected<describe_request, decode_error>
{ return decode<describe_request>( input ); }
auto decode_cells_request( std::string_view input ) ->
std::expected<cells_request, decode_error>
{ return decode<cells_request>( input ); }
auto decode_command_request( std::string_view input ) ->
std::expected<command_request, decode_error>
{ return decode<command_request>( input ); }
auto decode_result_request( std::string_view input ) -> std::expected<result_request, decode_error>
{ return decode<result_request>( input ); }

auto serialize_hello( const hello_reply &reply ) -> std::string
{
    using ordered = nlohmann::ordered_json;
    const auto &[epoch, engine, delivery, identity] = reply;
    auto result = ordered{{"version", contract_version}, {"epoch", epoch},
        {
            "engine", {
                {"build", engine.build}, {"mods", engine.mods},
                {"display", {{"tiles", engine.use_tiles}, {"tileset", engine.tileset}}}
            }
        },
        {
            "limits", {{"frame_bytes", maximum_frame_bytes}, {"cells_per_part", cells_per_part},
                {"cells_per_query", cells_per_query}
            }
        }};
    result["limits"]["screen_queue"] = maximum_queued_commands;
    if( delivery ) {
        result["delivery"] = {{"credits", delivery->credits}, {"held_frames", maximum_held_frames},
            {"held_bytes", maximum_held_bytes}
        };
        if( delivery->max_hz ) { result["delivery"]["max_hz"] = *delivery->max_hz; }
    }
    result["schema"] = identity.schema;
    result["locale"] = identity.locale;
    result["engine_locale"] = identity.engine_locale;
    result["role"] = identity.role;
    return result.dump();
}
auto serialize_description( const tile_description &value ) -> std::string
{
    using ordered = nlohmann::ordered_json;
    auto parts = ordered::array();
    for( const auto &part : value.parts ) { parts.push_back( to_json( part ) ); }
    return ordered{{"parts", std::move( parts )}}.dump();
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
    if( !action || value.kind < error::negotiation_failed || value.kind > error::stale_screen ||
        ( value.at && !valid_id( value.at->epoch ) ) ) {
        return std::unexpected( error::validation_failed );
    }
    auto result = nlohmann::ordered_json{{"kind", error_name( value.kind )}};
    result["message"] = error_text( value.kind );
    if( !action->empty() ) { result["action"] = std::string( *action ); }
    if( value.at ) { result["at"] = nlohmann::ordered_json::parse( serialize_clock( *value.at ) ); }
    if( value.schemas ) { result["schema"] = {{"engine", value.schemas->engine}, {"client", value.schemas->client}}; }
    return result.dump();
}
} // namespace engine_client
