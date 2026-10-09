#include "client_command.h"

#include <algorithm>
#include <utility>

#include "catacharset.h"
#include "client_input.h"
#include "client_interaction.h"

namespace game_client
{
namespace
{

auto input_id = std::uint64_t { 0 };

auto normalized_key( std::string key ) -> std::string
{
    if( key == "ENTER" ) { return "RETURN"; }
    if( key == "ESCAPE" ) { return "ESC"; }
    return key;
}

auto key_name( const input_command &command ) -> std::string
{
    auto result = std::string{};
    for( const auto &modifier : command.modifiers ) {
        result += modifier + "+";
    }
    return result + normalized_key( command.key );
}

} // namespace

auto begin_input_boundary() -> void { ++input_id; }
auto current_input_id() -> std::uint64_t { return input_id; }

auto available_input_actions() -> std::vector<input_action>
{
    const auto active = active_input_context();
    auto result = std::vector<input_action> {};
    if( active.context == nullptr ) { return result; }
    for( const auto &action : active.context->get_registered_actions_copy() ) {
        result.push_back( {
            .id = action,
            .name = active.context->get_action_name( action ),
            .bindings = inp_mngr.get_input_for_action( action, std::string( active.category ) )
        } );
    }
    return result;
}

auto resolve_input_command( const input_command &command, const point screen_size )
-> std::expected<input_event, std::string>
{
    namespace ranges = std::ranges;
    if( command.interaction ) {
        if( !command.action.empty() || !command.key.empty() || !command.modifiers.empty() ||
            !command.text.empty() || command.mouse_position || command.input_id ) {
            return std::unexpected( "invalid: semantic interaction cannot be mixed with raw input" );
        }
        const auto resolved = resolve_interaction_command( *command.interaction );
        if( !resolved ) {
            return std::unexpected( resolved.error() );
        }
        auto result = input_event{};
        result.type = input_event_t::interaction;
        result.interaction = *resolved;
        return result;
    }
    if( command.input_id && *command.input_id != current_input_id() ) {
        return std::unexpected( "Input boundary changed; observe again before acting" );
    }
    const auto has_mouse = command.mouse_position.has_value();
    if( ( command.action.empty() && command.key.empty() && command.text.empty() && !has_mouse ) ||
        ( !command.action.empty() && ( !command.key.empty() || !command.text.empty() ) ) ||
        ( has_mouse && ( !command.action.empty() || !command.key.empty() || !command.text.empty() ) ) ||
        ( !command.modifiers.empty() && command.key.empty() ) ) {
        return std::unexpected( "Provide one input mode: action, key with optional text, text, or mouse" );
    }
    if( has_mouse ) {
        const auto position = *command.mouse_position;
        if( position.x < 0 || position.y < 0 || position.x >= screen_size.x ||
            position.y >= screen_size.y ) {
            return std::unexpected( "Mouse position is outside the client surface" );
        }
        const auto button = command.mouse_button == "left" ? MOUSE_BUTTON_LEFT :
                            command.mouse_button == "right" ? MOUSE_BUTTON_RIGHT :
                            command.mouse_button == "scroll_up" ? SCROLLWHEEL_UP :
                            command.mouse_button == "scroll_down" ? SCROLLWHEEL_DOWN :
                            command.mouse_button == "move" ? MOUSE_MOVE : 0;
        if( button == 0 ) { return std::unexpected( "Unknown mouse button" ); }
        auto result = input_event( button, input_event_t::mouse );
        result.mouse_pos = position;
        return result;
    }
    if( command.action.empty() && command.key == "IDLE" && command.text.empty() &&
        command.modifiers.empty() ) {
        const auto active = active_input_context();
        if( active.context == nullptr || active.timeout_ms != 0 ) {
            return std::unexpected( "IDLE is only valid at a nonblocking input boundary" );
        }
        return input_event{};
    }
    if( command.action.empty() && command.key == "TIMEOUT" && command.text.empty() &&
        command.modifiers.empty() ) {
        auto result = input_event{};
        result.type = input_event_t::timeout;
        return result;
    }
    if( !command.action.empty() ) {
        const auto actions = available_input_actions();
        const auto action = ranges::find( actions, command.action, &input_action::id );
        if( action == actions.end() ) {
            return std::unexpected( "Action is not registered in the active context" );
        }
        const auto binding = ranges::find_if( action->bindings, []( const auto & event ) {
            return event.type == input_event_t::keyboard && !event.sequence.empty();
        } );
        if( binding == action->bindings.end() ) {
            return std::unexpected( "Action has no keyboard binding in the active context" );
        }
        return *binding;
    }
    if( command.key.empty() ) {
        const auto codepoint = UTF8_getch( command.text );
        if( codepoint == 0 ) { return std::unexpected( "Text must not start with NUL" ); }
        auto result = input_event( static_cast<int>( codepoint ), input_event_t::keyboard );
        result.text = command.text;
        return result;
    }
    auto code = inp_mngr.get_keycode( key_name( command ) );
    if( code == 0 && command.modifiers.empty() && command.key.size() == 1 ) {
        code = static_cast<unsigned char>( command.key.front() );
    }
    if( code == 0 ) { return std::unexpected( "Unknown key" ); }
    auto result = input_event( code, input_event_t::keyboard );
    // A modified key (Ctrl+X) is a command, not typed text.
    result.text = command.text.empty() && command.modifiers.empty() && command.key.size() == 1 ?
                  command.key : command.text;
    return result;
}

} // namespace game_client
