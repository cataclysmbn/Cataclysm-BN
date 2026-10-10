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
struct hello_request {
    std::vector<std::string> versions = {};
    std::string client_name = {};
    std::string client_version = {};
    std::optional<viewport> view = std::nullopt;
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
auto decode_choices_request( std::string_view input )
-> std::expected<choices_request, decode_error>;
auto decode_describe_request( std::string_view input )
-> std::expected<describe_request, decode_error>;
auto decode_command_request( std::string_view input )
-> std::expected<command_request, decode_error>;
auto decode_result_request( std::string_view input )
-> std::expected<result_request, decode_error>;

auto serialize_description( const tile_description &value ) -> std::string;

/// What the client should do after an application error.
enum class required_action { none, hello, subscribe, retry };
/// Closed application_error data, with no free-form diagnostics or arbitrary payloads.
struct application_error {
    error kind = error::validation_failed;
    required_action action = required_action::none;
    std::optional<clock_point> at = std::nullopt;
};

struct engine_info {
    std::string build = {};
    std::vector<std::string> mods = {};
};
/// Validate IDs/UTF-8 and the output bound before returning a complete serialized value.
auto serialize_hello( const std::string &epoch, const engine_info &engine ) -> std::string;
auto serialize_receipt( const receipt &value ) -> std::expected<std::string, error>;
auto serialize_application_error( const application_error &value )
-> std::expected<std::string, error>;

} // namespace engine_client
