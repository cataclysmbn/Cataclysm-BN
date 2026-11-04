#include "engine_client_mode.h"

#include "avatar.h"
#include "cached_options.h"
#include "client_interaction_data.h"
#include "effect.h"
#include "game.h"
#include "input.h"
#include "mapsharing.h"
#include "messages.h"
#include "options.h"
#include "translations.h"
#include "type_id.h"

#include <algorithm>
#include <functional>
#include <ranges>
#include <string_view>

namespace engine_client
{
namespace
{
constexpr auto mode_schema = std::string_view { "mode_operation" };
const auto effect_laserlocked = efftype_id( "laserlocked" );

/// A mode as published, and what sets it to a value it does not hold now.
struct mode_def {
    mode_value shown = {};
    std::function < auto( const std::string & ) -> void > apply = {};
};

auto on_off() -> std::vector<mode_choice> { return { { "on", _( "On" ) }, { "off", _( "Off" ) } }; }
auto on_off_id( const bool on ) -> std::string { return on ? "on" : "off"; }

/// An on/off mode: its id and label, whether it is on now and what sets it on or off.
struct flag_options {
    std::string id;
    std::string label;
    bool now = false;
    std::function < auto( bool ) -> void > apply;
};
auto flag( flag_options opts ) -> mode_def
{
    return { .shown = { .id = std::move( opts.id ), .label = std::move( opts.label ), .value = on_off_id( opts.now ), .values = on_off() },
    .apply = [apply = std::move( opts.apply )]( const std::string & value ) { apply( value == "on" ); } };
}

/// An option as a mode: `on`/`off` for a bool option, the option's own values for a selection. `announce` is what
/// the native key prints once the option holds the value.
struct option_mode_options {
    std::string id;
    std::string name;
    std::function < auto( const options_manager::cOpt & ) -> void > announce;
    bool save = false;
};
auto option_mode( option_mode_options opts ) -> mode_def
{
    auto &option = get_options().get_option( opts.name );
    const auto is_bool = option.getType() == "bool";
    auto values = is_bool ? on_off() : option.getItems() | std::views::transform( [](
    const auto & item ) {
        return mode_choice{ .id = item.first, .label = item.second.translated() };
    } ) | std::ranges::to<std::vector>();
    return { .shown = { .id = std::move( opts.id ), .label = option.getMenuText(),
                        .value = is_bool ? on_off_id( option.value_as<bool>() ) : option.getValue(), .values = std::move( values )
                      },
             .apply = [name = opts.name, is_bool, save = opts.save, announce = opts.announce]( const std::string & value )
    {
        auto &chosen = get_options().get_option( name );
        chosen.setValue( is_bool ? ( value == "on" ? "true" : "false" ) : value );
        if( save ) { get_options().save(); }
        announce( chosen );
    } };
}

auto thief_value() -> std::string
{
    const auto now = get_avatar().get_value( "THIEF_MODE" );
    return now == "THIEF_HONEST" ? "honest" : now == "THIEF_STEAL" ? "steal" : "ask";
}

auto move_choices() -> std::vector<mode_choice>
{
    return { { "walk", _( "Walk" ) }, { "run", _( "Run" ) }, { "crouch", _( "Crouch" ) }, { "prone", _( "Prone" ) } };
}
auto move_value( const character_movemode mode ) -> std::string
{
    return mode == CMM_RUN ? "run" : mode == CMM_CROUCH ? "crouch" : mode == CMM_PRONE ? "prone" :
           "walk";
}

auto mode_defs() -> std::vector<mode_def>
{
    auto &you = get_avatar();
    auto result = std::vector<mode_def> {};
    result.push_back( flag( { .id = "safe_mode", .label = _( "Safe mode" ), .now = g->safe_mode != SAFE_MODE_OFF, .apply = []( const bool on )
    {
        if( on ) {
            g->set_safe_mode( SAFE_MODE_ON );
            g->mostseen = 0;
            add_msg( m_info, to_translation( "Safe mode ON!" ) );
        } else {
            g->turnssincelastmon = 0;
            g->set_safe_mode( SAFE_MODE_OFF );
            add_msg( m_info, get_option<bool>( "AUTOSAFEMODE" )
                     ? to_translation( "Safe mode OFF!  (Auto safe mode still enabled!)" ) :
                     to_translation( "Safe mode OFF!" ) );
        }
        if( get_avatar().has_effect( effect_laserlocked ) ) {
            get_avatar().remove_effect( effect_laserlocked );
            g->reset_safe_mode_warning();
        }
    } } ) );
    result.push_back( option_mode( { .id = "auto_safe_mode", .name = "AUTOSAFEMODE", .announce = []( const options_manager::cOpt & option )
    {
        add_msg( m_info, option.value_as<bool>() ? to_translation( "Auto safe mode ON!" ) :
                 to_translation( "Auto safe mode OFF!" ) );
    }, .save = false } ) );
    result.push_back( flag( { .id = "auto_travel_mode", .label = _( "Auto travel mode" ), .now = g->auto_travel_mode, .apply = []( const bool on )
    {
        g->auto_travel_mode = on;
        add_msg( m_info, on ? to_translation( "Auto travel mode ON!" ) :
                 to_translation( "Auto travel mode OFF!" ) );
    } } ) );
    result.push_back( flag( { .id = "manual_combat_mode", .label = _( "Manual combat mode" ), .now = g->manual_combat_mode, .apply = []( const bool on )
    {
        g->manual_combat_mode = on;
        add_msg( m_info, on ? to_translation( "Manual combat mode ON!" ) :
                 to_translation( "Manual combat mode OFF!" ) );
    } } ) );
    result.push_back( { .shown = { .id = "thief_mode", .label = _( "Thief mode" ), .value = thief_value(),
    .values = { { "ask", _( "Ask" ) }, { "honest", _( "Honest" ) }, { "steal", _( "Steal" ) } }
                                 },
    .apply = []( const std::string & value )
    {
        const auto honest = value == "honest";
        const auto steal = value == "steal";
        get_avatar().set_value( "THIEF_MODE",
                                honest ? "THIEF_HONEST" : steal ? "THIEF_STEAL" : "THIEF_ASK" );
        get_avatar().set_value( "THIEF_MODE_KEEP", honest || steal ? "YES" : "NO" );
        //~ Thief mode cycled between THIEF_ASK/THIEF_HONEST/THIEF_STEAL
        add_msg( honest ? to_translation( "You will not pick up other peoples belongings." ) : steal ?
                 to_translation( "You will pick up also those things that belong to others!" ) :
                 to_translation( "You will be reminded not to steal." ) );
    } } );
    result.push_back( option_mode( { .id = "auto_features", .name = "AUTO_FEATURES", .announce = []( const options_manager::cOpt & option )
    {
        //~ Auto Features are now ON/OFF
        add_msg( to_translation( "%s are now %s." ), option.getMenuText(),
                 option.value_as<bool>() ? _( "ON" ) : _( "OFF" ) );
    }, .save = true } ) );
    result.push_back( option_mode( { .id = "auto_pulp_butcher", .name = "AUTO_PULP_BUTCHER", .announce = []( const options_manager::cOpt & option )
    {
        //~ Auto Pulp/Pulp Adjacent/Butcher is now set to x
        add_msg( to_translation( "%s is now set to %s." ), option.getMenuText(), option.getValueName() );
    }, .save = true } ) );
    result.push_back( option_mode( { .id = "auto_mining", .name = "AUTO_MINING", .announce = []( const options_manager::cOpt & option )
    {
        //~ Auto Mining is now ON/OFF
        add_msg( to_translation( "%s is now %s." ), option.getMenuText(),
                 option.value_as<bool>() ? _( "ON" ) : _( "OFF" ) );
    }, .save = true } ) );
    result.push_back( option_mode( { .id = "auto_foraging", .name = "AUTO_FORAGING", .announce = []( const options_manager::cOpt & option )
    {
        //~ Auto Foraging is now set to x
        add_msg( to_translation( "%s is now set to %s." ), option.getMenuText(), option.getValueName() );
    }, .save = true } ) );
    result.push_back( option_mode( { .id = "auto_pickup", .name = "AUTO_PICKUP", .announce = []( const options_manager::cOpt & option )
    {
        //~ Auto pickup is now set to x
        add_msg( to_translation( "%s is now set to %s." ), option.getMenuText(), option.getValueName() );
    }, .save = true } ) );
    result.push_back( flag( { .id = "zone_overlay", .label = _( "Zone overlay" ), .now = g->show_zone_overlay, .apply = []( const bool on ) { g->show_zone_overlay = on; } } ) );
    result.push_back( flag( { .id = "map_memory", .label = _( "Map memory" ), .now = you.should_show_map_memory(), .apply = []( const bool /*on*/ ) { get_avatar().toggle_map_memory(); } } ) );
    result.push_back( flag( { .id = "hour_timer", .label = _( "Hour timer" ), .now = g->debug_hour_timer_enabled(), .apply = []( const bool /*on*/ ) { g->toggle_debug_hour_timer(); } } ) );
    // The native key does nothing in a competitive shared map without the debugger.
    if( !MAP_SHARING::isCompetitive() || MAP_SHARING::isDebugger() ) {
        result.push_back( flag( { .id = "debug_mode", .label = _( "Debug mode" ), .now = debug_mode, .apply = []( const bool on )
        {
            debug_mode = on;
            add_msg( m_info, on ? to_translation( "Debug mode ON!" ) : to_translation( "Debug mode OFF!" ) );
        } } ) );
    }
    result.push_back( { .shown = { .id = "move_mode", .label = _( "Movement mode" ), .value = move_value( you.get_movement_mode() ),
                                   .values = move_choices()
                                 },
                        .apply = []( const std::string & value )
    {
        get_avatar().set_movement_mode( value == "run" ? CMM_RUN : value == "crouch" ? CMM_CROUCH :
                                        value == "prone" ? CMM_PRONE : CMM_WALK );
    } } );
    return result;
}

auto find_mode( const std::vector<mode_def> &defs, const std::string &id ) -> const mode_def *
{
    const auto found = std::ranges::find( defs, id, []( const auto & def ) { return def.shown.id; } );
    return found == defs.end() ? nullptr : &*found;
}
} // namespace

auto engine_modes() -> std::vector<mode_value>
{
    return mode_defs() | std::views::transform( &mode_def::shown ) | std::ranges::to<std::vector>();
}

auto mode_operation_check( const mode_operation &operation ) -> std::expected<void, std::string>
{
    const auto defs = mode_defs();
    const auto *const found = find_mode( defs, operation.mode );
    if( found == nullptr ) { return std::unexpected( _( "There is no such mode." ) ); }
    if( !std::ranges::contains( found->shown.values, operation.value, &mode_choice::id ) ) {
        return std::unexpected( _( "That mode has no such value." ) );
    }
    return {};
}

auto mode_operation_event( const mode_operation &operation ) -> input_event
{
    auto result = input_event{};
    result.type = input_event_t::interaction;
    result.interaction = game_client::interaction_event{
        // A field filled with the value: the shape replay reads back with its value.
        .operation = game_client::interaction_operation::fill,
        .schema_id = std::string( mode_schema ),
        .target_id = operation.mode,
        .value = operation.value,
        .submit = true,
    };
    return result;
}

auto mode_operation_of( const input_event &event ) -> std::optional<mode_operation>
{
    const auto &semantic = event.interaction;
    if( !semantic || semantic->schema_id != mode_schema ) { return std::nullopt; }
    return mode_operation{ .mode = semantic->target_id, .value = semantic->value };
}

auto run_mode_operation( const mode_operation &operation ) -> bool
{
    if( !mode_operation_check( operation ) ) { return false; }
    const auto defs = mode_defs();
    const auto &found = *find_mode( defs, operation.mode );
    if( found.shown.value != operation.value ) { found.apply( operation.value ); }
    return true;
}

} // namespace engine_client
