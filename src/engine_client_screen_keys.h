#pragma once

#include <string>
#include <vector>

class input_context;

namespace engine_client
{

/// A key the open screen of an input context answers to beyond its registered actions.
struct screen_key {
    std::string label = {};
    std::vector<std::string> keys = {};
};

/// The keys the screen open on `context` gives its own affordances and tabs, in the order it offers
/// them; empty when no screen is open on it.
auto screen_keys( const input_context &context ) -> std::vector<screen_key>;

} // namespace engine_client
