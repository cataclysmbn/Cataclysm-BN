#pragma once

#include "engine_client_contract.h"

#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace engine_client
{

struct snapshot_request {
    std::string session_epoch = {};
    projection page = {};
};
struct result_request {
    std::string session_epoch = {};
    std::string command_id = {};
};
/// A receipt is not a validation/execution result.
struct receipt {
    std::string session_epoch = {};
    std::string command_id = {};
};

/// Safe local categories only, never parser text or native resolver diagnostics.
/// Not a JSON-RPC envelope classifier; framing must validate its complete frame separately.
enum class decode_error { invalid_json, invalid_params, resource_limit };
/// Decode one complete application value, at most maximum_inline_bytes UTF-8 bytes.
/// Pure, owned results: no native provider, authority, receipt allocation or gameplay RNG access.
auto decode_negotiation_request( std::string_view input )
-> std::expected<negotiation_request, decode_error>;
auto decode_snapshot_request( std::string_view input )
-> std::expected<snapshot_request, decode_error>;
auto decode_command_request( std::string_view input )
-> std::expected<command_request, decode_error>;
auto decode_result_request( std::string_view input )
-> std::expected<result_request, decode_error>;

enum class application_error_stage { negotiation, receipt, validation, execution, completion };
enum class required_action { negotiate, read_snapshot, retry, none };
struct error_current {
    std::string session_epoch = {};
    counter state_revision = 0;
    counter through_public_sequence = 0;
};
/// Closed application_error data, with no free-form diagnostics or arbitrary payloads.
struct application_error {
    error kind = error::validation_failed;
    application_error_stage stage = application_error_stage::receipt;
    bool retryable = false;
    required_action action = required_action::none;
    std::optional<error_current> current = std::nullopt;
};
/// Parent-selected JSON-RPC application failure mapping; the envelope is adapter-owned.
inline constexpr auto application_error_code = 1000;
inline constexpr auto application_error_message = "Engine contract error";

/// Validate IDs/UTF-8 and the output bound before returning a complete serialized value.
auto serialize_receipt( const receipt &value ) -> std::expected<std::string, error>;
auto serialize_application_error( const application_error &value )
-> std::expected<std::string, error>;

} // namespace engine_client
