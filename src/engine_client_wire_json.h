#pragma once

#include <cstdint>
#include <initializer_list>

#include <nlohmann/json.hpp>

/// The readers the wire decoders use, for other readers of client JSON. Each throws
/// `decode_error::invalid_params` when the value breaks its rule.
namespace engine_client
{

auto require( bool condition ) -> void;
/// The value must be an object whose members are all listed; `required` ones must be present.
auto members( const nlohmann::json &value, std::initializer_list<const char *> required,
std::initializer_list<const char *> optional = {} ) -> void;
/// An integer, or an integral float such as 1.0 (JSON Schema `integer`), within [low, high].
auto integer( const nlohmann::json &object, const char *name, std::int64_t low, std::int64_t high )
-> std::int64_t;

} // namespace engine_client
