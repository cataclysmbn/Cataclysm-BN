#include "engine_client_jsonrpc.h"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <istream>
#include <ostream>
#include <span>
#include <utility>
#include <vector>

namespace engine_client::jsonrpc
{
namespace
{

auto digit( char byte ) -> bool { return byte >= '0' && byte <= '9'; }
auto space( char byte ) -> bool
{
    return byte == ' ' || byte == '\t' || byte == '\r' || byte == '\n';
}
auto require( bool condition ) -> void
{
    if( !condition ) { throw parse_error::invalid_json; }
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

/// JsonIn's substr/tell/skip_value are not strict lexical seams: skip_value recurses,
/// skip_number is permissive, and string decoding loses NUL and surrogate pairs while
/// accepting non-scalar/overlong UTF-8. The wire reader's strict scalar primitives are
/// private and its closed grammar cannot skip arbitrary JSON. This small lexical scanner
/// therefore handles syntax/raw spans only, never the application grammar or a DOM.
/// An explicit one-byte-per-container state stack permits all nesting within the byte
/// bound without native-stack recursion. Numbers are scanned, never converted.
class scanner
{
    public:
        explicit scanner( std::string_view input ) : input_( input ) {}
        auto whitespace() -> void { while( space( peek() ) ) { ++offset_; } }
        auto peek() const -> char { return offset_ == input_.size() ? '\0' : input_[offset_]; }
        auto accept( char byte ) -> bool {
            if( offset_ < input_.size() && input_[offset_] == byte ) { ++offset_; return true; }
            return false;
        }
        auto expect( char byte ) -> void { require( accept( byte ) ); }
        auto finish() -> void { whitespace(); require( offset_ == input_.size() ); }
        auto offset() const -> std::size_t { return offset_; }
        auto string( std::string *decoded = nullptr ) -> void {
            expect( '"' );
            while( true ) {
                const auto start = offset_;
                const auto byte = take();
                if( byte == '"' ) { return; }
                require( static_cast<unsigned char>( byte ) >= 0x20 );
                if( byte == '\\' ) {
                    const auto escape = take();
                    auto value = char{};
                    switch( escape ) {
                        case '"':
                            value = '"';
                            break;
                        case '\\':
                            value = '\\';
                            break;
                        case '/':
                            value = '/';
                            break;
                        case 'b':
                            value = '\b';
                            break;
                        case 'f':
                            value = '\f';
                            break;
                        case 'n':
                            value = '\n';
                            break;
                        case 'r':
                            value = '\r';
                            break;
                        case 't':
                            value = '\t';
                            break;
                        case 'u': {
                            auto scalar = hex_unit();
                            if( scalar >= 0xd800 && scalar <= 0xdbff ) {
                                expect( '\\' );
                                expect( 'u' );
                                const auto low = hex_unit();
                                require( low >= 0xdc00 && low <= 0xdfff );
                                scalar = 0x10000 + ( ( scalar - 0xd800 ) << 10 ) + low - 0xdc00;
                            } else { require( scalar < 0xdc00 || scalar > 0xdfff ); }
                            if( decoded ) { append_scalar( *decoded, scalar ); }
                            continue;
                        }
                        default:
                            throw parse_error::invalid_json;
                    }
                    if( decoded ) { decoded->push_back( value ); }
                } else {
                    utf8_tail( static_cast<unsigned char>( byte ) );
                    if( decoded ) { decoded->append( input_.substr( start, offset_ - start ) ); }
                }
            }
        }
        auto decoded_string() -> std::string {
            // Called with a complete string-token view. Decoding cannot expand its bytes.
            auto result = std::string{};
            result.reserve( input_.size() );
            string( &result );
            return result;
        }
        auto value( std::size_t *array_members = nullptr ) -> std::string_view {
            whitespace();
            const auto start = offset_;
            if( array_members ) { *array_members = 0; }
            auto states = std::vector<state> {};
            atom( states );
            while( !states.empty() ) {
                whitespace();
                switch( states.back() ) {
                    case state::array_first:
                        if( accept( ']' ) ) { states.pop_back(); break; }
                        [[fallthrough]];
                    case state::array_value:
                        if( array_members && states.size() == 1 ) { ++*array_members; }
                        states.back() = state::array_after;
                        atom( states );
                        break;
                    case state::array_after:
                        if( accept( ']' ) ) { states.pop_back(); }
                        else { expect( ',' ); states.back() = state::array_value; }
                        break;
                    case state::object_first:
                        if( accept( '}' ) ) { states.pop_back(); break; }
                        [[fallthrough]];
                    case state::object_key:
                        string();
                        states.back() = state::object_colon;
                        break;
                    case state::object_colon:
                        expect( ':' );
                        states.back() = state::object_value;
                        break;
                    case state::object_value:
                        states.back() = state::object_after;
                        atom( states );
                        break;
                    case state::object_after:
                        if( accept( '}' ) ) { states.pop_back(); }
                        else { expect( ',' ); states.back() = state::object_key; }
                        break;
                }
            }
            return input_.substr( start, offset_ - start );
        }
    private:
        enum class state : unsigned char {
            array_first, array_value, array_after, object_first, object_key,
            object_colon, object_value, object_after,
        };
        std::string_view input_;
        std::size_t offset_ = 0;
        auto take() -> char { require( offset_ < input_.size() ); return input_[offset_++]; }
        auto hex_unit() -> std::uint32_t {
            auto value = std::uint32_t{0};
            for( auto count = 0; count < 4; ++count ) {
                const auto byte = take();
                auto hex = 0;
                if( digit( byte ) ) { hex = byte - '0'; }
                else if( byte >= 'a' && byte <= 'f' ) { hex = byte - 'a' + 10; }
                else if( byte >= 'A' && byte <= 'F' ) { hex = byte - 'A' + 10; }
                else { throw parse_error::invalid_json; }
                value = ( value << 4 ) | hex;
            }
            return value;
        }
        auto utf8_tail( unsigned char lead ) -> void {
            if( lead < 0x80 ) { return; }
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
            } else { throw parse_error::invalid_json; }
            while( remaining-- > 0 ) {
                const auto byte = static_cast<unsigned char>( take() );
                require( ( byte & 0xc0 ) == 0x80 );
                scalar = ( scalar << 6 ) | ( byte & 0x3f );
            }
            require( scalar >= minimum && scalar <= 0x10ffff &&
                     !( scalar >= 0xd800 && scalar <= 0xdfff ) );
        }
        auto number() -> void {
            accept( '-' );
            require( digit( peek() ) );
            if( !accept( '0' ) ) { while( digit( peek() ) ) { ++offset_; } }
            if( accept( '.' ) ) {
                require( digit( peek() ) );
                while( digit( peek() ) ) { ++offset_; }
            }
            if( accept( 'e' ) || accept( 'E' ) ) {
                if( !accept( '-' ) ) { accept( '+' ); }
                require( digit( peek() ) );
                while( digit( peek() ) ) { ++offset_; }
            }
        }
        auto literal( std::string_view text ) -> void {
            require( input_.substr( offset_, text.size() ) == text );
            offset_ += text.size();
        }
        auto atom( std::vector<state> &states ) -> void {
            switch( peek() ) {
                case '{':
                    ++offset_;
                    states.push_back( state::object_first );
                    break;
                case '[':
                    ++offset_;
                    states.push_back( state::array_first );
                    break;
                case '"':
                    string();
                    break;
                case 't':
                    literal( "true" );
                    break;
                case 'f':
                    literal( "false" );
                    break;
                case 'n':
                    literal( "null" );
                    break;
                default:
                    number();
                    break;
            }
        }
};

auto valid_json( std::string_view text ) -> bool
{
    try {
        auto reader = scanner( text );
        reader.value();
        reader.finish();
        return true;
    } catch( parse_error ) { return false; }
}
auto id_type( std::string_view text ) -> bool
{
    return !text.empty() && ( text.front() == '"' || text == "null" ||
                              text.front() == '-' || digit( text.front() ) );
}
auto kind( std::string_view text ) -> value_kind
{
    switch( text.front() ) {
        case '{':
            return value_kind::object;
        case '[':
            return value_kind::array;
        case '"':
            return value_kind::string;
        case 'n':
            return value_kind::null_value;
        case 't':
        case 'f':
            return value_kind::boolean;
        default:
            return value_kind::number;
    }
}
auto inspect_value( std::string_view text ) -> inspected_value
{
    auto result = inspected_value{ .type = kind( text ), .json = std::string( text ) };
    if( result.type == value_kind::string ) { result.decoded = scanner( text ).decoded_string(); }
    return result;
}
struct key_shape {
    std::size_t count = 0;
    std::size_t bytes = 0;
};
/// Count before allocating: exactly K spans and at most the original key-token bytes.
/// No set nodes, vector-growth copies, per-key string allocations or batch metadata.
auto count_keys( std::string_view text ) -> key_shape
{
    auto result = key_shape{};
    auto reader = scanner( text );
    reader.expect( '{' );
    reader.whitespace();
    if( reader.accept( '}' ) ) { return result; }
    while( true ) {
        result.bytes += reader.value().size();
        ++result.count;
        reader.whitespace();
        reader.expect( ':' );
        reader.value();
        reader.whitespace();
        if( reader.accept( '}' ) ) { return result; }
        reader.expect( ',' );
        reader.whitespace();
    }
}
struct key_span {
    std::uint32_t offset = 0;
    std::uint32_t size = 0;
};
static_assert( sizeof( key_span ) == 8 );
auto inspect_envelope( std::string_view text ) -> inspected_envelope
{
    namespace ranges = std::ranges;
    auto result = inspected_envelope{ .type = kind( text ) };
    if( result.type != value_kind::object ) { return result; }
    const auto shape = count_keys( text );
    if( shape.count == 0 ) { return result; }
    auto key_bytes = std::make_unique<char[]>( shape.bytes );
    auto key_spans = std::make_unique<key_span[]>( shape.count );
    auto byte_offset = std::size_t{0};
    auto key_index = std::size_t{0};
    auto reader = scanner( text );
    reader.expect( '{' );
    reader.whitespace();
    while( true ) {
        const auto key = scanner( reader.value() ).decoded_string();
        key_spans[key_index++] = { .offset = static_cast<std::uint32_t>( byte_offset ),
                                   .size = static_cast<std::uint32_t>( key.size() )
                                 };
        ranges::copy( key, key_bytes.get() + byte_offset );
        byte_offset += key.size();
        reader.whitespace();
        reader.expect( ':' );
        const auto value = reader.value();
        if( key == "jsonrpc" ) { result.version = inspect_value( value ); }
        else if( key == "method" ) { result.method = inspect_value( value ); }
        else if( key == "id" ) { result.id = inspect_value( value ); }
        else if( key == "params" ) { result.params = inspect_value( value ); }
        reader.whitespace();
        if( reader.accept( '}' ) ) { break; }
        reader.expect( ',' );
        reader.whitespace();
    }
    const auto key_view = [&]( const auto & span ) { return std::string_view( key_bytes.get() + span.offset, span.size ); };
    auto spans = std::span( key_spans.get(), shape.count );
    ranges::sort( spans, [&]( const auto & lhs, const auto & rhs ) { return key_view( lhs ) < key_view( rhs ); } );
    result.keys_unique = ranges::adjacent_find( spans,
    [&]( const auto & lhs, const auto & rhs ) { return key_view( lhs ) == key_view( rhs ); } ) ==
    spans.end();
    return result;
}
auto inspected_request( const inspected_envelope &input ) -> request
{
    return { .id = { .json = input.id.json }, .method = *input.method.decoded,
             .params = input.params.json };
}
auto legacy_bad_shape( const request_id &id ) -> legacy_error
{
    return { .id = { .json = id.json.value_or( "null" ) },
             .reason = id.json ? legacy_reason::invalid_parameters : legacy_reason::invalid_request };
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
auto checked_id( const request_id &id ) -> std::expected<std::string_view, output_error>
{
    const auto text = std::string_view( *id.json );
    if( text.size() > maximum_frame_bytes ) { return std::unexpected( output_error::resource_limit ); }
    if( !id_type( text ) || !valid_json( text ) ) {
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

envelope_cursor::envelope_cursor( std::shared_ptr<const std::string> bytes, position pos ) :
    bytes_( std::move( bytes ) ), pos_( pos ) {}
auto envelope_cursor::remaining() const -> std::size_t { return bytes_ ? pos_.count : 0; }
auto envelope_cursor::next() -> std::optional<inspected_envelope>
{
    if( remaining() == 0 ) { return std::nullopt; }
    auto reader = scanner( std::string_view( *bytes_ ).substr( pos_.offset ) );
    const auto value = reader.value();
    reader.whitespace();
    if( pos_.batch && pos_.count > 1 ) { reader.expect( ',' ); }
    pos_.offset += reader.offset();
    --pos_.count;
    return inspect_envelope( value );
}
inspected_frame::inspected_frame( envelope_cursor origin, bool is_batch ) :
    batch( is_batch ), origin_( std::move( origin ) ) {}
auto inspected_frame::size() const -> std::size_t { return origin_.remaining(); }
auto inspected_frame::cursor() const -> envelope_cursor { return origin_; }
auto inspect_frame( std::string_view input ) -> std::expected<inspected_frame, parse_error>
{
    if( input.size() > maximum_frame_bytes ) { return std::unexpected( parse_error::resource_limit ); }
    try {
        auto bytes = std::make_shared<const std::string>( input );
        auto reader = scanner( *bytes );
        reader.whitespace();
        const auto start = reader.offset();
        auto count = std::size_t{0};
        const auto root = reader.value( &count );
        reader.finish();
        const auto batch = root.front() == '[';
        auto origin = envelope_cursor( std::move( bytes ),
        { .offset = start + ( batch ? 1u : 0u ), .count = batch ? count : 1u, .batch = batch } );
        return inspected_frame( std::move( origin ), batch );
    } catch( parse_error error ) { return std::unexpected( error ); }
}

auto apply_strict_policy( const inspected_envelope &input ) -> request_entry
{
    const auto id_valid = input.id.type == value_kind::missing ||
                          input.id.type == value_kind::null_value ||
                          input.id.type == value_kind::number || input.id.type == value_kind::string;
    const auto params_valid = input.params.type == value_kind::missing ||
                              input.params.type == value_kind::object || input.params.type == value_kind::array;
    if( input.type != value_kind::object || !input.keys_unique ||
        input.version.decoded != "2.0" || input.method.type != value_kind::string ||
        !id_valid || !params_valid ) {
        return error_code::invalid_request;
    }
    return inspected_request( input );
}
auto apply_legacy_policy( const inspected_envelope &input ) -> legacy_request_entry
{
    if( input.type != value_kind::object || !input.keys_unique ) {
        return legacy_error{ .id = { .json = "null" }, .reason = legacy_reason::parse_error };
    }
    if( input.id.type == value_kind::number ) {
        const auto &text = *input.id.json;
        auto number = std::int64_t{};
        const auto parsed = std::from_chars( text.data(), text.data() + text.size(), number );
        if( parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() ) {
            return legacy_error{ .id = { .json = "null" }, .reason = legacy_reason::invalid_request };
        }
    } else if( input.id.type != value_kind::missing && input.id.type != value_kind::string &&
               input.id.type != value_kind::null_value ) {
        return legacy_error{ .id = { .json = "null" }, .reason = legacy_reason::invalid_request };
    }
    const auto id = request_id{ .json = input.id.json };
    if( input.version.type != value_kind::missing && input.version.type != value_kind::string ) {
        return legacy_bad_shape( id );
    }
    if( input.version.decoded != "2.0" ) {
        return legacy_error{ .id = { .json = id.json.value_or( "null" ) },
                             .reason = id.json ? legacy_reason::invalid_jsonrpc_version :
                                       legacy_reason::invalid_jsonrpc_request };
    }
    if( input.method.type == value_kind::missing ) {
        return legacy_error{ .id = { .json = id.json.value_or( "null" ) }, .reason = legacy_reason::method_required };
    }
    if( input.method.type != value_kind::string ) { return legacy_bad_shape( id ); }
    // Legacy dispatch establishes notification identity BEFORE inspecting application payloads.
    // A params:null (or otherwise wrong-type) notification must therefore remain a notification.
    return inspected_request( input );
}
request_cursor::request_cursor( envelope_cursor origin, bool empty_batch_error ) :
    origin_( std::move( origin ) ), empty_batch_error_( empty_batch_error ) {}
auto request_cursor::remaining() const -> std::size_t
{
    return origin_.remaining() + ( empty_batch_error_ ? 1u : 0u );
}
auto request_cursor::next() -> std::optional<request_entry>
{
    if( std::exchange( empty_batch_error_, false ) ) { return error_code::invalid_request; }
    const auto input = origin_.next();
    if( !input ) { return std::nullopt; }
    return apply_strict_policy( *input );
}
parsed_frame::parsed_frame( request_cursor origin, bool is_batch ) :
    batch( is_batch ), origin_( std::move( origin ) ) {}
auto parsed_frame::size() const -> std::size_t { return origin_.remaining(); }
auto parsed_frame::cursor() const -> request_cursor { return origin_; }
auto parse_frame( std::string_view input ) -> std::expected<parsed_frame, parse_error>
{
    const auto inspected = inspect_frame( input );
    if( !inspected ) { return std::unexpected( inspected.error() ); }
    const auto empty_batch = inspected->batch && inspected->size() == 0;
    return parsed_frame( request_cursor( inspected->cursor(), empty_batch ),
                         inspected->batch && !empty_batch );
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
