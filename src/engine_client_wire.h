#pragma once

#include "engine_client_contract.h"

#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace engine_client
{

/// Map cells the client can show: the native terrain window it wants, in columns and rows.
struct viewport {
    int cols = 0;
    int rows = 0;
};
constexpr int maximum_viewport_cells = 512;
constexpr std::size_t maximum_tint_ids = 4096;
struct hello_request {
    std::vector<std::string> versions = {};
    std::string client_name = {};
    std::string client_version = {};
    /// 0.2 only, and then required: the hash of the schema the client was built with.
    std::optional<std::string> schema = std::nullopt;
    /// 0.2 only: the BN language id the client wants its text in; absent: the engine's.
    std::optional<std::string> locale = std::nullopt;
    std::optional<viewport> view = std::nullopt;
    /// The client draws tiles, so it wants tile ids where the engine has them (the overmap).
    bool tiles = false;
    /// The ids of the tints the client's tileset has; the engine publishes the keys among them that apply.
    std::vector<std::string> tint_ids = {};
    /// Opt-in credit delivery: the pushed frames the engine may send before the client grants more.
    std::optional<std::size_t> credits = std::nullopt;
    /// With credits: the most pushed frames per second of wall time a running activity may publish.
    std::optional<std::size_t> max_hz = std::nullopt;
};
/// `bn.credit`: more pushed frames the client is ready to read.
struct credit_request {
    std::size_t frames = 0;
};
/// `bn.hold`: an affordance of the screen on top that the client holds down.
struct hold_request {
    std::string epoch = {};
    std::string screen_id = {};
    std::string affordance_id = {};
};
struct result_request {
    std::string epoch = {};
    std::string command_id = {};
};
/// A receipt is not a validation/execution result.
struct receipt {
    std::string epoch = {};
    std::string command_id = {};
};

/// Safe local categories only, never parser text or native resolver diagnostics.
/// Not a JSON-RPC envelope classifier; framing must validate its complete frame separately.
enum class decode_error { invalid_json, invalid_params, resource_limit };
/// Decode one complete application value, at most maximum_inline_bytes UTF-8 bytes.
/// Pure, owned results: no native provider, authority, receipt allocation or gameplay RNG access.
auto decode_hello_request( std::string_view input ) -> std::expected<hello_request, decode_error>;
/// `{cols, rows}` for `bn.viewport`.
auto decode_viewport_request( std::string_view input ) -> std::expected<viewport, decode_error>;
/// `{}` for `bn.subscribe` and `bn.unsubscribe`.
auto decode_empty_request( std::string_view input ) -> std::expected<void, decode_error>;
/// `{frames}` for `bn.credit`.
auto decode_credit_request( std::string_view input ) -> std::expected<credit_request, decode_error>;
/// `{epoch, screen_id, affordance_id}` for `bn.hold`.
auto decode_hold_request( std::string_view input ) -> std::expected<hold_request, decode_error>;
auto decode_choices_request( std::string_view input )
-> std::expected<choices_request, decode_error>;
auto decode_describe_request( std::string_view input )
-> std::expected<describe_request, decode_error>;
auto decode_cells_request( std::string_view input )
-> std::expected<cells_request, decode_error>;
auto decode_command_request( std::string_view input )
-> std::expected<command_request, decode_error>;
auto decode_result_request( std::string_view input )
-> std::expected<result_request, decode_error>;

auto serialize_description( const tile_description &value ) -> std::string;

/// What the client should do after an application error.
enum class required_action { none, hello, subscribe, retry };
/// The schema hashes of a hello the engine refused, so that both sides can say what differs.
struct schema_hashes {
    std::string engine = {};
    std::string client = {};
};
/// Closed application_error data, with no free-form diagnostics or arbitrary payloads.
struct application_error {
    error kind = error::validation_failed;
    required_action action = required_action::none;
    std::optional<clock_point> at = std::nullopt;
    std::optional<schema_hashes> schemas = std::nullopt;
};

struct engine_info {
    std::string build = {};
    std::vector<std::string> mods = {};
    /// The `USE_TILES` and `TILES` options: how the native client would draw the map.
    bool use_tiles = true;
    std::string tileset = {};
    /// The BN language id the engine's text is in.
    std::string locale = {};
};
/// Validate IDs/UTF-8 and the output bound before returning a complete serialized value.
/// What the engine granted a client that asked for credit delivery.
struct delivery_grant {
    std::size_t credits = 0;
    std::optional<std::size_t> max_hz = std::nullopt;
};
/// What a hello answers besides the engine, limits and delivery.
struct hello_identity {
    std::string schema = {};
    /// The language the connection's text is in.
    std::string locale = {};
    std::string engine_locale = {};
    /// `controller` or `spectator`.
    std::string role = {};
};
struct hello_reply {
    std::string epoch = {};
    engine_info engine = {};
    std::optional<delivery_grant> delivery = std::nullopt;
    hello_identity identity = {};
};
auto serialize_hello( const hello_reply &reply ) -> std::string;
auto serialize_receipt( const receipt &value ) -> std::expected<std::string, error>;
auto serialize_application_error( const application_error &value )
-> std::expected<std::string, error>;

} // namespace engine_client
