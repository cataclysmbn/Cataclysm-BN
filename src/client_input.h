#pragma once

#include <string_view>

class input_context;

namespace game_client
{

/// The context currently waiting for input, including nested menus and prompts.
/// References remain valid only during the synchronous input backend call.
struct input_context_view {
    const input_context *context = nullptr;
    std::string_view category;
    int timeout_ms = -1;
};

auto active_input_context() -> input_context_view;

/// Restores the enclosing context when a nested input request returns or throws.
class input_context_scope
{
    public:
        input_context_scope( const input_context &context, std::string_view category,
                             int timeout_ms = -1 );
        ~input_context_scope();
        input_context_scope( const input_context_scope & ) = delete;
        auto operator=( const input_context_scope & ) -> input_context_scope & = delete; // *NOPAD*

    private:
        input_context_view previous;
};

} // namespace game_client
