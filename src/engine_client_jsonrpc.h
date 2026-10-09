#pragma once

#include <cstddef>
#include <expected>
#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace engine_client::jsonrpc
{

/// Transport bytes excluding the newline; independent of the 262144-byte application limit.
inline constexpr auto maximum_frame_bytes = std::size_t {1024 * 1024};

/// An absent ID is a notification. Present IDs retain their exact validated JSON spelling,
/// including null, arbitrary decimal numbers, escapes and supplementary Unicode.
struct request_id {
    std::optional<std::string> json = std::nullopt;
};
struct request {
    request_id id;
    std::string method;
    /// Original value bytes, without surrounding whitespace. No application decoding here.
    std::optional<std::string> params = std::nullopt;
};
enum class error_code : int {
    parse_error = -32700,
    invalid_request = -32600,
    method_not_found = -32601,
    invalid_params = -32602,
    internal_error = -32603,
    not_initialized = -32002,
    application_error = 1000,
};
using request_entry = std::variant<request, error_code>;
enum class parse_error { invalid_json, resource_limit };

/// Inspection is syntax-only: even invalid envelopes retain their complete member identity.
/// Every key/string is strict Unicode; keys_unique compares decoded envelope keys.
/// Missing, null and wrong-type values stay distinct. Extras never become params.
enum class value_kind { missing, null_value, boolean, number, string, array, object };
struct inspected_value {
    value_kind type = value_kind::missing;
    std::optional<std::string> json = std::nullopt;
    std::optional<std::string> decoded = std::nullopt;
};
struct inspected_envelope {
    value_kind type = value_kind::missing;
    bool keys_unique = true;
    inspected_value version = {};
    inspected_value method = {};
    inspected_value id = {};
    inspected_value params = {};
};
/// Parser working-allocation budget for one active frame/cursor (requested heap bytes,
/// excluding allocator bookkeeping and values deliberately retained by callers).
/// One immutable input, one envelope's fields/keys, and iterative lexical scratch only:
/// 8B character/stack/copy headroom + 8 bytes per possible object key + 4096 control bytes.
/// A JSON object with K members requires at least 5K+1 bytes. No batch-member/depth cap.
inline constexpr auto maximum_parser_working_bytes = 8 * maximum_frame_bytes +
        8 * ( ( maximum_frame_bytes - 1 ) / 5 ) + 4096;

class inspected_frame;
class parsed_frame;
/// Copyable position over immutable OWNED validated input; no per-member vector/index.
/// A cursor pins input independently of its frame, other cursors, and the caller's string.
class envelope_cursor
{
    public:
        envelope_cursor() = default;
        /// Returns one OWNED envelope, which may itself outlive this cursor/frame.
        auto next() -> std::optional<inspected_envelope>;
        auto remaining() const -> std::size_t;
    private:
        struct position {
            std::size_t offset = 0;
            std::size_t count = 0;
            bool batch = false;
        };
        envelope_cursor( std::shared_ptr<const std::string> bytes, position pos );
        std::shared_ptr<const std::string> bytes_;
        position pos_;
        friend auto inspect_frame( std::string_view ) -> std::expected<inspected_frame, parse_error>;
};
class inspected_frame
{
    public:
        bool batch = false;
        /// Empty batch remains batch=true with size zero for policy to reject explicitly.
        auto size() const -> std::size_t;
        auto cursor() const -> envelope_cursor;
    private:
        inspected_frame( envelope_cursor origin, bool batch );
        envelope_cursor origin_;
        friend auto inspect_frame( std::string_view ) -> std::expected<inspected_frame, parse_error>;
};
/// Validate the WHOLE immutable owned frame before exposing any cursor. Policies are
/// applied one envelope at a time; there is never a second complete representation.
auto inspect_frame( std::string_view input ) -> std::expected<inspected_frame, parse_error>;
auto apply_strict_policy( const inspected_envelope &input ) -> request_entry;

class request_cursor
{
    public:
        request_cursor() = default;
        /// Strict materialization is lazy but syntax has already been fully validated.
        /// Returned requests/IDs/params are owned, never dangling views into the frame.
        auto next() -> std::optional<request_entry>;
        auto remaining() const -> std::size_t;
    private:
        request_cursor( envelope_cursor origin, bool empty_batch_error );
        envelope_cursor origin_;
        bool empty_batch_error_ = false;
        friend auto parse_frame( std::string_view ) -> std::expected<parsed_frame, parse_error>;
};
class parsed_frame
{
    public:
        bool batch = false;
        auto size() const -> std::size_t;
        auto cursor() const -> request_cursor;
    private:
        parsed_frame( request_cursor origin, bool batch );
        request_cursor origin_;
        friend auto parse_frame( std::string_view ) -> std::expected<parsed_frame, parse_error>;
};

/// Established legacy protocol messages ONLY, never native/host/rejection diagnostics.
enum class legacy_reason {
    parse_error, invalid_request, invalid_parameters, invalid_jsonrpc_version,
    invalid_jsonrpc_request, method_required, not_initialized, unknown_resource,
    structured_state_unavailable, subscriptions_not_supported,
    structured_interaction_unavailable, unknown_tool, method_not_found,
};
struct legacy_error {
    request_id id = {};
    legacy_reason reason = legacy_reason::invalid_request;
};
using legacy_request_entry = std::variant<request, legacy_error>;
/// Historical legacy envelope/ID policy AFTER full strict syntax validation: only signed
/// int64 numeric IDs without decimal/exponent spelling, null error ID on invalid IDs,
/// and no params-type restriction (valid legacy notifications remain silent).
/// Never use this policy for direct bn methods; selection belongs to the server.
auto apply_legacy_policy( const inspected_envelope &input ) -> legacy_request_entry;

/// Validates ALL syntax before returning ANY owned requests. Empty batches become one
/// non-batch invalid_request. Envelope extras are allowed but never added to params.
/// Decoded duplicate envelope keys are rejected; params semantics belong to the wire decoder.
auto parse_frame( std::string_view input ) -> std::expected<parsed_frame, parse_error>;

enum class read_status { complete, eof, partial_eof, too_large, io_error };
struct read_result {
    read_status status = read_status::eof;
    std::string bytes;
};
/// All non-complete/non-eof statuses are terminal: do not dispatch or retry a partial frame.
/// Effective limit is min(max_bytes, maximum_frame_bytes); oversized settings never
/// relax the absolute cap. The terminating newline is excluded, as in legacy MCP.
auto read_frame( std::istream &input, std::size_t max_bytes = maximum_frame_bytes ) -> read_result;

enum class output_error { invalid_value, resource_limit, io_error };
/// Only the factories can create response bytes; exactly one of result/error is emitted.
class response
{
    public:
        auto bytes() const -> std::string_view;
    private:
        explicit response( std::string bytes );
        std::string bytes_;
        friend auto make_result( const request &, std::string_view ) ->
        std::expected<std::optional<response>, output_error>;
        friend auto make_error( const struct error_options & ) ->
        std::expected<std::optional<response>, output_error>;
        friend auto make_legacy_error( const legacy_error & ) ->
        std::expected<std::optional<response>, output_error>;
};
/// Missing IDs suppress all results/errors. A present null ID still receives a response.
auto make_result( const request &input, std::string_view result ) ->
std::expected<std::optional<response>, output_error>;
struct error_options {
    request_id id;
    error_code code = error_code::internal_error;
    /// Already serialized closed application error only; the framing seam validates syntax,
    /// not application policy. No native exception/diagnostic message parameter exists.
    std::optional<std::string_view> data = std::nullopt;
};
/// Fixed safe messages, including the unchanged legacy initialization error. For protocol
/// parse/invalid-request errors pass an explicit ID of "null", not a missing notification ID.
auto make_error( const error_options &options ) ->
std::expected<std::optional<response>, output_error>;
/// Closed trusted legacy code/message pairs; omitted IDs still suppress responses.
auto make_legacy_error( const legacy_error &options ) ->
std::expected<std::optional<response>, output_error>;

/// Incrementally stores ONLY bounded response bytes, not an unbounded response vector.
/// Failure poisons the assembler; finish never returns an otherwise valid partial prefix.
class response_frame
{
    public:
        explicit response_frame( bool batch );
        response_frame( const response_frame & ) = default;
        auto operator=( const response_frame & ) -> response_frame & = default; // *NOPAD*
        /// Ownership transfer preserves the destination and poisons the moved-from source.
        response_frame( response_frame &&other ) noexcept;
        auto operator=( response_frame &&other ) noexcept -> response_frame &; // *NOPAD*
        auto append( const std::optional<response> &value ) -> std::expected<void, output_error>;
        /// No responses (e.g. all notifications) returns nullopt, not an empty JSON array.
        auto finish() && -> std::expected<std::optional<std::string>, output_error>; // *NOPAD*
    private:
        bool batch_;
        std::size_t count_ = 0;
        std::string bytes_;
        std::optional<output_error> failure_ = std::nullopt;
};

/// Validates and bounds the complete frame before any stream write. On stream failure,
/// return io_error and close the connection: OS-level partial writes cannot be rolled back.
/// Nullopt means no write/flush. JSON whitespace is compacted for newline-delimited stdio.
auto write_frame( std::ostream &output, const std::optional<std::string> &frame ) ->
std::expected<void, output_error>;

} // namespace engine_client::jsonrpc
