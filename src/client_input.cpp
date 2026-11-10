#include "client_input.h"

namespace game_client
{
namespace
{
thread_local auto current_context = input_context_view {};
} // namespace

auto active_input_context() -> input_context_view
{
    return current_context;
}

input_context_scope::input_context_scope( const input_context &context,
        const std::string_view category, const int timeout_ms ) : previous( current_context )
{
    current_context = { .context = &context, .category = category, .timeout_ms = timeout_ms };
}

input_context_scope::~input_context_scope()
{
    current_context = previous;
}

} // namespace game_client
