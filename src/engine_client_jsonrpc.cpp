#include "engine_client_jsonrpc.h"

#include <algorithm>
#include <cstdint>
#include <istream>
#include <limits>
#include <ostream>
#include <utility>
#include <vector>

namespace engine_client::jsonrpc
{
namespace
{
constexpr auto maximum_nesting = 64;

auto space( char byte ) -> bool
{
    return byte == ' ' || byte == '\t' || byte == '\r' || byte == '\n';
}
auto request_of( const envelope &input ) -> request
{
    const auto id = input.find( "id" );
    const auto params = input.find( "params" );
    return { .id = { .json = id == input.end() ? std::nullopt : std::optional{ id->dump() } },
             .method = input["method"].get<std::string>(),
             .params = params == input.end() ? std::nullopt : std::optional{ params->dump() } };
}
auto legacy_bad_shape( const request_id &id ) -> legacy_error
{
    return { .id = { .json = id.json.value_or( "null" ) },
             .reason = id.json ? legacy_reason::invalid_parameters : legacy_reason::invalid_request };
}
auto member( const envelope &input, const char *name ) -> const envelope *
{
    const auto found = input.find( name );
    return found == input.end() ? nullptr : &*found;
}

/// Called only after validation; removes insignificant whitespace, never string bytes.
auto compact( std::string_view text ) -> std::string
{
    auto result = std::string{};
    result.reserve( text.size() );
    auto quoted = false;
    auto escaped = false;
    for( const auto byte : text ) {
        if( quoted || !space( byte ) ) { result.push_back( byte ); }
        if( escaped ) { escaped = false; }
        else if( quoted && byte == '\\' ) { escaped = true; }
        else if( byte == '"' ) { quoted = !quoted; }
    }
    return result;
}
auto valid_json( std::string_view text ) -> bool { return envelope::accept( text ); }
auto checked_id( const request_id &id ) -> std::expected<std::string_view, output_error>
{
    const auto text = std::string_view( *id.json );
    if( text.size() > maximum_frame_bytes ) { return std::unexpected( output_error::resource_limit ); }
    const auto parsed = envelope::parse( text, nullptr, false );
    if( parsed.is_discarded() || !( parsed.is_string() || parsed.is_number() || parsed.is_null() ) ) {
        return std::unexpected( output_error::invalid_value );
    }
    return text;
}
auto append_bounded( std::string &target, std::string_view value ) -> bool
{
    if( value.size() > maximum_frame_bytes - target.size() ) { return false; }
    target.append( value );
    return true;
}
auto error_message( error_code code ) -> std::optional<std::string_view>
{
    switch( code ) {
        case error_code::parse_error:
            return "Parse error";
        case error_code::invalid_request:
            return "Invalid Request";
        case error_code::method_not_found:
            return "Method not found";
        case error_code::invalid_params:
            return "Invalid params";
        case error_code::internal_error:
            return "Internal error";
        case error_code::not_initialized:
            return "Server is not initialized";
        case error_code::application_error:
            return "Engine contract error";
    }
    return std::nullopt;
}
struct trusted_error {
    error_code code;
    std::string_view message;
};
auto legacy_message( legacy_reason reason ) -> std::optional<trusted_error>
{
    switch( reason ) {
        case legacy_reason::parse_error:
            return trusted_error{ .code = error_code::parse_error, .message = "Parse error" };
        case legacy_reason::invalid_request:
            return trusted_error{ .code = error_code::invalid_request, .message = "Invalid request" };
        case legacy_reason::invalid_parameters:
            return trusted_error{ .code = error_code::invalid_params, .message = "Invalid parameters" };
        case legacy_reason::invalid_jsonrpc_version:
            return trusted_error{ .code = error_code::invalid_request, .message = "Invalid JSON-RPC version" };
        case legacy_reason::invalid_jsonrpc_request:
            return trusted_error{ .code = error_code::invalid_request, .message = "Invalid JSON-RPC request" };
        case legacy_reason::method_required:
            return trusted_error{ .code = error_code::invalid_request, .message = "JSON-RPC method is required" };
        case legacy_reason::not_initialized:
            return trusted_error{ .code = error_code::not_initialized, .message = "Server is not initialized" };
        case legacy_reason::unknown_resource:
            return trusted_error{ .code = error_code::invalid_params, .message = "Unknown resource" };
        case legacy_reason::structured_state_unavailable:
            return trusted_error{ .code = error_code::internal_error, .message = "Structured state is unavailable" };
        case legacy_reason::subscriptions_not_supported:
            return trusted_error{ .code = error_code::method_not_found, .message = "Resource subscriptions are not supported" };
        case legacy_reason::structured_interaction_unavailable:
            return trusted_error{ .code = error_code::internal_error, .message = "Structured interaction is unavailable" };
        case legacy_reason::unknown_tool:
            return trusted_error{ .code = error_code::invalid_params, .message = "Unknown tool" };
        case legacy_reason::method_not_found:
            return trusted_error{ .code = error_code::method_not_found, .message = "Method not found" };
    }
    return std::nullopt;
}
/// Only the closed tables above supply messages; there is no public arbitrary-string seam.
auto assemble_error( const error_options &options, std::string_view message ) ->
std::expected<std::string, output_error>
{
    const auto id = checked_id( options.id );
    if( !id ) { return std::unexpected( id.error() ); }
    if( options.data && options.code != error_code::application_error ) {
        return std::unexpected( output_error::invalid_value );
    }
    if( options.data ) {
        if( options.data->size() > maximum_frame_bytes ) {
            return std::unexpected( output_error::resource_limit );
        }
        if( !valid_json( *options.data ) ) { return std::unexpected( output_error::invalid_value ); }
    }
    auto bytes = std::string{R"({"jsonrpc":"2.0","id":)"};
    if( !append_bounded( bytes, compact( *id ) ) ||
        !append_bounded( bytes, R"(,"error":{"code":)" ) ||
        !append_bounded( bytes, std::to_string( static_cast<int>( options.code ) ) ) ||
        !append_bounded( bytes, R"(,"message":")" ) ||
        !append_bounded( bytes, message ) || !append_bounded( bytes, "\"" ) ||
        ( options.data && ( !append_bounded( bytes, R"(,"data":)" ) ||
                            !append_bounded( bytes, compact( *options.data ) ) ) ) ||
        !append_bounded( bytes, "}}" ) ) {
        return std::unexpected( output_error::resource_limit );
    }
    return bytes;
}
} // namespace

auto envelope_cursor::remaining() const -> std::size_t
{
    return entries_ ? entries_->size() - next_ : 0;
}
auto envelope_cursor::next() -> std::optional<envelope>
{
    if( !entries_ || next_ == entries_->size() ) { return std::nullopt; }
    return ( *entries_ )[next_++];
}
auto inspected_frame::size() const -> std::size_t { return origin_.remaining(); }
auto inspected_frame::cursor() const -> envelope_cursor { return origin_; }
auto inspect_frame( std::string_view input ) -> std::expected<inspected_frame, parse_error>
{
    if( input.size() > maximum_frame_bytes ) { return std::unexpected( parse_error::resource_limit ); }
    // The parser treats a raw NUL as the end of input, which would hide a suffix.
    if( input.find( '\0' ) != std::string_view::npos ) { return std::unexpected( parse_error::invalid_json ); }
    auto too_deep = false;
    auto root = envelope::parse( input, [&]( const int depth, const envelope::parse_event_t,
    envelope & ) {
        too_deep = too_deep || depth > maximum_nesting;
        return true;
    }, false );
    if( root.is_discarded() || too_deep ) { return std::unexpected( parse_error::invalid_json ); }
    auto result = inspected_frame{};
    result.batch = root.is_array();
    auto entries = std::vector<envelope> {};
    if( result.batch ) { entries = std::move( root ).get<std::vector<envelope>>(); }
    else { entries.push_back( std::move( root ) ); }
    result.origin_ = envelope_cursor{ std::make_shared<const std::vector<envelope>>( std::move( entries ) ) };
    return result;
}
auto method_of( const envelope &input ) -> std::string
{
    const auto method = input.is_object() ? member( input, "method" ) : nullptr;
    return method && method->is_string() ? method->get<std::string>() : std::string{};
}

auto apply_strict_policy( const envelope &input ) -> request_entry
{
    if( !input.is_object() ) { return error_code::invalid_request; }
    const auto version = member( input, "jsonrpc" );
    const auto method = member( input, "method" );
    const auto id = member( input, "id" );
    const auto params = member( input, "params" );
    if( !version || *version != "2.0" || !method || !method->is_string() ||
        ( id && !( id->is_null() || id->is_number() || id->is_string() ) ) ||
        ( params && !( params->is_object() || params->is_array() ) ) ) {
        return error_code::invalid_request;
    }
    return request_of( input );
}
auto apply_legacy_policy( const envelope &input ) -> legacy_request_entry
{
    if( !input.is_object() ) {
        return legacy_error{ .id = { .json = "null" }, .reason = legacy_reason::parse_error };
    }
    const auto bad_id = legacy_error{ .id = { .json = "null" }, .reason = legacy_reason::invalid_request };
    const auto id_value = member( input, "id" );
    if( id_value && id_value->is_number() ) {
        // Historical clients use signed 64-bit integers only.
        if( !id_value->is_number_integer() ||
            ( id_value->is_number_unsigned() &&
              id_value->get<std::uint64_t>() > std::numeric_limits<std::int64_t>::max() ) ) {
            return bad_id;
        }
    } else if( id_value && !id_value->is_string() && !id_value->is_null() ) { return bad_id; }
    const auto id = request_id{ .json = id_value ? std::optional{ id_value->dump() } : std::nullopt };
    const auto version = member( input, "jsonrpc" );
    if( version && !version->is_string() ) { return legacy_bad_shape( id ); }
    if( !version || *version != "2.0" ) {
        return legacy_error{ .id = { .json = id.json.value_or( "null" ) },
                             .reason = id.json ? legacy_reason::invalid_jsonrpc_version :
                                       legacy_reason::invalid_jsonrpc_request };
    }
    const auto method = member( input, "method" );
    if( !method ) {
        return legacy_error{ .id = { .json = id.json.value_or( "null" ) }, .reason = legacy_reason::method_required };
    }
    if( !method->is_string() ) { return legacy_bad_shape( id ); }
    // Legacy dispatch establishes notification identity BEFORE inspecting application payloads.
    // A params:null (or otherwise wrong-type) notification must therefore remain a notification.
    return request_of( input );
}

auto read_frame( std::istream &input, std::size_t max_bytes ) -> read_result
{
    const auto limit = std::min( max_bytes, maximum_frame_bytes );
    auto result = read_result{};
    result.bytes.reserve( std::min<std::size_t>( limit, 4096 ) );
    try {
        auto byte = char{};
        while( input.get( byte ) ) {
            if( byte == '\n' ) { result.status = read_status::complete; return result; }
            if( result.bytes.size() == limit ) {
                result.status = read_status::too_large;
                result.bytes.clear();
                return result;
            }
            result.bytes.push_back( byte );
        }
    } catch( ... ) {
        // A streambuf may rethrow a device-specific exception with badbit enabled.
        // Never export its diagnostic; inspect flags, including exception-masked EOF.
        if( !input.eof() ) {
            result.status = read_status::io_error;
            result.bytes.clear();
            return result;
        }
    }
    if( input.bad() || !input.eof() ) { result.status = read_status::io_error; }
    else if( !result.bytes.empty() ) { result.status = read_status::partial_eof; }
    // No partial bytes are offered to the dispatcher.
    result.bytes.clear();
    return result;
}

response::response( std::string bytes ) : bytes_( std::move( bytes ) ) {}
auto response::bytes() const -> std::string_view { return bytes_; }

auto make_result( const request &input, std::string_view result ) ->
std::expected<std::optional<response>, output_error>
{
    if( !input.id.json ) { return std::nullopt; }
    const auto id = checked_id( input.id );
    if( !id ) { return std::unexpected( id.error() ); }
    if( result.size() > maximum_frame_bytes ) { return std::unexpected( output_error::resource_limit ); }
    if( !valid_json( result ) ) { return std::unexpected( output_error::invalid_value ); }
    auto bytes = std::string{R"({"jsonrpc":"2.0","id":)"};
    if( !append_bounded( bytes, compact( *id ) ) ||
        !append_bounded( bytes, R"(,"result":)" ) ||
        !append_bounded( bytes, compact( result ) ) || !append_bounded( bytes, "}" ) ) {
        return std::unexpected( output_error::resource_limit );
    }
    return response( std::move( bytes ) );
}
auto make_error( const error_options &options ) ->
std::expected<std::optional<response>, output_error>
{
    if( !options.id.json ) { return std::nullopt; }
    const auto message = error_message( options.code );
    if( !message ) { return std::unexpected( output_error::invalid_value ); }
    auto bytes = assemble_error( options, *message );
    if( !bytes ) { return std::unexpected( bytes.error() ); }
    return response( std::move( *bytes ) );
}
auto make_legacy_error( const legacy_error &options ) ->
std::expected<std::optional<response>, output_error>
{
    if( !options.id.json ) { return std::nullopt; }
    const auto trusted = legacy_message( options.reason );
    if( !trusted ) { return std::unexpected( output_error::invalid_value ); }
    auto bytes = assemble_error( { .id = options.id, .code = trusted->code }, trusted->message );
    if( !bytes ) { return std::unexpected( bytes.error() ); }
    return response( std::move( *bytes ) );
}

response_frame::response_frame( bool batch ) : batch_( batch ) {}
response_frame::response_frame( response_frame &&other ) noexcept :
    batch_( other.batch_ ), count_( std::exchange( other.count_, 0 ) ),
    bytes_( std::move( other.bytes_ ) ),
    failure_( std::exchange( other.failure_, output_error::invalid_value ) ) {}
auto response_frame::operator=( response_frame &&other ) noexcept -> response_frame & // *NOPAD*
{
    if( this != &other ) {
        batch_ = other.batch_;
        count_ = std::exchange( other.count_, 0 );
        bytes_ = std::move( other.bytes_ );
        failure_ = std::exchange( other.failure_, output_error::invalid_value );
    }
    return *this;
}
auto response_frame::append( const std::optional<response> &value ) ->
std::expected<void, output_error>
{
    if( failure_ ) { return std::unexpected( *failure_ ); }
    if( !value ) { return {}; }
    if( value->bytes().empty() || ( !batch_ && count_ != 0 ) ) {
        failure_ = output_error::invalid_value;
    } else {
        // A batch always owes its closing bracket plus this response's opening '[' or ','.
        const auto overhead = batch_ ? size_t{ 2 } :
                              size_t{ 0 };
        if( bytes_.size() + overhead > maximum_frame_bytes ||
            value->bytes().size() > maximum_frame_bytes - bytes_.size() - overhead ) {
            failure_ = output_error::resource_limit;
        }
    }
    if( failure_ ) { bytes_.clear(); return std::unexpected( *failure_ ); }
    if( batch_ ) { bytes_.push_back( count_ == 0 ? '[' : ',' ); }
    bytes_.append( value->bytes() );
    ++count_;
    return {};
}
auto response_frame::finish() &&
-> std::expected<std::optional<std::string>, output_error> // *NOPAD*
{
    if( failure_ ) { return std::unexpected( *failure_ ); }
    // Consumed assemblers cannot be reused to publish a moved-from/malformed frame.
    failure_ = output_error::invalid_value;
    if( count_ == 0 ) { return std::nullopt; }
    if( batch_ ) { bytes_.push_back( ']' ); }
    return std::move( bytes_ );
}
auto write_frame( std::ostream &output, const std::optional<std::string> &frame ) ->
std::expected<void, output_error>
{
    if( !frame ) { return {}; }
    if( frame->size() > maximum_frame_bytes ) { return std::unexpected( output_error::resource_limit ); }
    if( !valid_json( *frame ) ) { return std::unexpected( output_error::invalid_value ); }
    auto bytes = compact( *frame );
    bytes.push_back( '\n' );
    try {
        output.write( bytes.data(), static_cast<std::streamsize>( bytes.size() ) );
        output.flush();
    } catch( ... ) {
        // Stream devices may throw non-ios exceptions; none cross the transport seam.
        return std::unexpected( output_error::io_error );
    }
    if( !output ) { return std::unexpected( output_error::io_error ); }
    return {};
}

} // namespace engine_client::jsonrpc
