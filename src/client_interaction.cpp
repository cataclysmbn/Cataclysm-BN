#include "client_interaction.h"
#include "client_interaction_validation.h"

#include "catacharset.h"
#include "char_validity_check.h"
#include "client_command.h"
#include "client_input.h"
#include "client_memory.h"
#include "input.h"
#include "ui_manager.h"
#include "wcwidth.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <format>
#include <limits>
#include <ranges>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace game_client
{
namespace
{

struct provider_entry {
    const input_context *context;
    std::shared_ptr<interaction_provider> provider;
    std::size_t token;
};

thread_local auto providers = std::vector<provider_entry> {};
thread_local auto next_provider_token = std::size_t {0};

constexpr auto maximum_page_size = std::size_t {200};
auto wire_overmap_tiles = false;

/// Length-framed, deterministic content identity of an interaction.
class content_hash
{
    public:
        auto add( const std::string_view value ) -> void {
            // Length framing includes embedded NULs and prevents separator collisions.
            text_ += std::to_string( value.size() ) + ':';
            text_ += value;
        }
        /// 64-bit FNV-1a: replays compare it across builds, and std::hash differs between standard
        /// libraries. The repository has no other string hash with a fixed definition.
        auto value() const -> std::uint64_t {
            return std::ranges::fold_left( text_, std::uint64_t{ 0xcbf29ce484222325 }, []( const auto hash,
            const char c ) { return ( hash ^ static_cast<unsigned char>( c ) ) * std::uint64_t{ 0x100000001b3 }; } );
        }
    private:
        std::string text_;
};

auto hex_digest( const content_hash &hash ) -> std::string
{
    return std::format( "{:016x}", hash.value() );
}

auto provider_for( const input_context &context ) -> const provider_entry *
{
    const auto provider =
    std::ranges::find_if( providers | std::views::reverse, [&context]( const auto & entry ) {
        return entry.context == &context;
    } );
    return provider == providers.rend() ? nullptr : &*provider;
}

auto add_position( content_hash &hash, const interaction_position &position ) -> void
{
    hash.add( std::to_string( position.x ) );
    hash.add( std::to_string( position.y ) );
    hash.add( std::to_string( position.z ) );
}

/// The identity of what the native screen offers: its kind, the ids of its choices and targets, their
/// counts and states, and positions. No text goes in, so the same screen has the same schema in every
/// language and a recorded input stays valid when it is played in another.
auto schema_for( const interaction_snapshot &snapshot ) -> std::string
{
    auto hash = content_hash{};
    hash.add( interaction_kind_name( snapshot.kind ) );
    hash.add( snapshot.context );
    hash.add( snapshot.message_tone );
    hash.add( std::to_string( snapshot.refresh_ms ) );
    hash.add( snapshot.allow_cancel ? "cancel" : "no-cancel" );
    hash.add( snapshot.allow_set_count ? "set-count" : "no-set-count" );
    for( const auto &choice : snapshot.choices ) {
        hash.add( choice.id );
        hash.add( choice.enabled ? "enabled" : "disabled" );
        hash.add( choice.selectable ? "selectable" : "blocked" );
        std::ranges::for_each( choice.columns, [&hash]( const auto & column ) { hash.add( column.key ); } );
        if( choice.editor ) {
            hash.add( choice.editor->type );
            hash.add( std::to_string( choice.editor->max_length ) );
            hash.add( choice.editor->minimum ? std::to_string( *choice.editor->minimum ) : "no-minimum" );
            hash.add( choice.editor->maximum ? std::to_string( *choice.editor->maximum ) : "no-maximum" );
            std::ranges::for_each( choice.editor->values, [&hash]( const auto & value ) { hash.add( value.id ); } );
        }
        hash.add( choice.minimum_count ? std::to_string( *choice.minimum_count ) : "default-minimum" );
        hash.add( choice.available_count ? std::to_string( *choice.available_count ) : "no-limit" );
    }
    if( snapshot.field ) {
        hash.add( snapshot.field->id );
        hash.add( snapshot.field->type );
        hash.add( std::to_string( snapshot.field->max_length ) );
        hash.add( snapshot.field->printable ? "printable" : "controls-allowed" );
    }
    if( snapshot.target ) {
        hash.add( snapshot.target->coordinate_space );
        add_position( hash, snapshot.target->source );
        add_position( hash, snapshot.target->cursor );
        if( snapshot.target->minimum_position ) {
            add_position( hash, *snapshot.target->minimum_position );
        }
        if( snapshot.target->maximum_position ) {
            add_position( hash, *snapshot.target->maximum_position );
        }
        hash.add( std::to_string( snapshot.target->range ) );
        hash.add( snapshot.target->distance_metric );
        hash.add( snapshot.target->limit_to_reality_bubble ? "bubble-limited" : "map-limited" );
        for( const auto &candidate : snapshot.target->candidates ) {
            hash.add( candidate.id );
            add_position( hash, candidate.position );
            hash.add( candidate.creature ? "creature" : "tile" );
        }
    }
    return hex_digest( hash );
}

auto normalize( const input_context &context,
                interaction_snapshot snapshot ) -> interaction_snapshot
{
    snapshot.context = context.category_name();
    snapshot.structured = true;
    snapshot.actions_only = false;
    snapshot.choice_offset = 0;
    snapshot.choice_total = snapshot.choices.size();
    snapshot.schema_id = schema_for( snapshot );
    return snapshot;
}

/// The value of `values` whose id is `id`; the first of several.
template<typename Values>
auto find_id( const Values &values,
              const std::string &id ) -> const typename Values::value_type * // *NOPAD*
{
    const auto found = std::ranges::find_if( values, [&id]( const auto & value ) { return value.id == id; } );
    return found == values.end() ? nullptr : &*found;
}

auto acquire( const input_context &context ) -> interaction_snapshot
{
    const auto entry = provider_for( context );
    if( entry == nullptr ) {
        return {
            .schema_id = {},
            .context = context.category_name(),
            .kind = interaction_kind::custom,
            .structured = false,
            .actions_only = true,
        };
    }
    // Pin the original callable: nested registration can grow the registry, and mutable
    // callbacks must retain their own state rather than run on a copied callable.
    const auto provider = entry->provider;
    return normalize( context, ( *provider )() );
}

auto acquire_active() -> interaction_snapshot
{
    const auto active = active_input_context();
    if( active.context != nullptr ) { return acquire( *active.context ); }
    return { .context = std::string( active.category ) };
}

auto materialize( interaction_snapshot full, const interaction_page page ) -> interaction_snapshot
{
    const auto total = full.choices.size();
    const auto offset = std::min( page.offset, total );
    const auto count = std::min( total - offset, std::min( page.limit, maximum_page_size ) );
    const auto focus = std::ranges::find( full.choices, true, &interaction_choice::highlighted );
    full.focus_choice_id = focus == full.choices.end() ? std::string{} :
                           focus->id;
    full.choices.erase( full.choices.begin() + offset + count, full.choices.end() );
    full.choices.erase( full.choices.begin(), full.choices.begin() + offset );
    full.input_id = current_input_id();
    full.choice_offset = offset;
    full.choice_total = total;
    return full;
}

auto validate_field_value( const interaction_field &field, const std::string &value )
-> std::expected<void, std::string>
{
    if( field.type == "key" ) {
        return inp_mngr.get_keycode( value ) != 0 || utf8_wrapper( value ).length() == 1 ?
               std::expected<void, std::string> {} :
               std::unexpected( "invalid: field value is not a key name" );
    }
    const auto text = utf8_wrapper( value );
    if( field.max_length > 0 && text.display_width() > static_cast<std::size_t>( field.max_length ) ) {
        return std::unexpected( "invalid: field value exceeds max_length" );
    }
    auto remaining = static_cast<int>( value.size() );
    const auto *cursor = value.c_str();
    auto position = std::size_t{0};
    while( remaining > 0 ) {
        const auto codepoint = UTF8_getch( &cursor, &remaining );
        if( mk_wcwidth( codepoint ) < 0 || ( field.printable && !is_char_allowed( codepoint ) ) ) {
            return std::unexpected( "invalid: field value contains a disallowed character" );
        }
        if( field.type == "integer" ) {
            const auto is_sign = codepoint == '-' && position == 0;
            if( !is_sign && ( codepoint < '0' || codepoint > '9' ) ) {
                return std::unexpected(
                           "invalid: integer field accepts an optional leading minus and digits" );
            }
        }
        ++position;
    }
    return {};
}

/// The value must be one the editor offers, so a client cannot set what the native menu would refuse.
auto validate_editor_value( const interaction_editor &editor, const std::string &value )
-> std::expected<void, std::string>
{
    if( editor.type == "bool" ) {
        if( value != "true" && value != "false" ) {
            return std::unexpected( "invalid: bool editor accepts true or false" );
        }
    } else if( editor.type == "select" ) {
        if( std::ranges::none_of( editor.values, [&value]( const auto & entry ) { return entry.id == value; } ) ) {
            return std::unexpected( "invalid: value is not one of the editor values" );
        }
    } else if( editor.type == "integer" || editor.type == "float" ) {
        auto parsed = 0.0;
        const auto *const end = value.data() + value.size();
        const auto result = std::from_chars( value.data(), end, parsed );
        if( value.empty() || result.ec != std::errc{} || result.ptr != end || !std::isfinite( parsed ) ||
            ( editor.type == "integer" && parsed != std::trunc( parsed ) ) ) {
            return std::unexpected( "invalid: editor value is not a number of its type" );
        }
        if( ( editor.minimum && parsed < *editor.minimum ) || ( editor.maximum &&
                parsed > *editor.maximum ) ) {
            return std::unexpected( "invalid: editor value is outside its range" );
        }
    } else {
        return validate_field_value( { .max_length = editor.max_length }, value );
    }
    return {};
}

auto validate( const interaction_snapshot &snapshot, const interaction_event &event )
-> std::expected<void, std::string>
{
    if( event.operation == interaction_operation::invoke ) {
        // The screen that offered the affordance runs it from its own loop; the lifecycle has matched it
        // to the affordance's shape, so no native widget has to describe it.
        if( event.target_id.empty() || !event.value.empty() || event.submit || event.count ||
            event.position || !event.arguments ) {
            return std::unexpected( "invalid: invoke requires only an affordance_id and its arguments" );
        }
        return {};
    }
    if( event.arguments ) { return std::unexpected( "invalid: only invoke carries arguments" ); }
    if( !snapshot.structured ) {
        return std::unexpected( "unsupported: active view only exposes generic actions" );
    }
    if( event.schema_id != snapshot.schema_id ) {
        return std::unexpected(
                   "invalid: interaction schema changed (recorded=" + event.schema_id
                   + ", active=" + snapshot.schema_id + ", context=" + snapshot.context + ")" );
    }
    if( event.operation == interaction_operation::cancel ) {
        if( !event.target_id.empty() || !event.value.empty() || event.submit || event.count
            || event.position ) {
            return std::unexpected(
                       "invalid: cancel does not accept a target, value, submit, count, or position" );
        }
        if( !snapshot.allow_cancel ) {
            return std::unexpected( "unsupported: active interaction cannot be canceled" );
        }
        return {};
    }
    if( event.operation == interaction_operation::choose ) {
        if( event.target_id.empty() || !event.value.empty() || event.submit || event.count
            || event.position ) {
            return std::unexpected( "invalid: choose requires only a choice_id" );
        }
        const auto choice = find_id( snapshot.choices, event.target_id );
        if( choice == nullptr ) {
            return std::unexpected( "invalid: choice_id is not in the active interaction" );
        }
        if( !choice->selectable ) { return std::unexpected( "disabled: choice is not selectable" ); }
        return {};
    }
    if( event.operation == interaction_operation::set_count ) {
        if( event.target_id.empty() || !event.value.empty() || event.submit || !event.count
            || event.position ) {
            return std::unexpected( "invalid: set_count requires only a choice_id and count" );
        }
        if( !snapshot.allow_set_count ) {
            return std::unexpected(
                       "unsupported: active interaction does not accept item quantities" );
        }
        const auto choice = find_id( snapshot.choices, event.target_id );
        if( choice == nullptr ) {
            return std::unexpected( "invalid: choice_id is not in the active interaction" );
        }
        if( !choice->selectable ) { return std::unexpected( "disabled: choice is not selectable" ); }
        const auto minimum = choice->minimum_count.value_or( 0 );
        if( *event.count < minimum ) {
            return std::unexpected( "invalid: count is below the minimum quantity" );
        }
        if( !choice->available_count || *event.count > *choice->available_count ) {
            return std::unexpected( "invalid: count exceeds the available item quantity" );
        }
        return {};
    }
    if( event.operation == interaction_operation::set_target ) {
        if( !event.value.empty() || event.submit || event.count || !event.position ) {
            return std::unexpected(
                       "invalid: set_target requires a position and optional candidate_id" );
        }
        if( !snapshot.target ) {
            return std::unexpected( "unsupported: active interaction does not accept a target" );
        }
        if( !event.target_id.empty() ) {
            const auto candidate = find_id( snapshot.target->candidates, event.target_id );
            if( candidate == nullptr
                || candidate->position != *event.position ) {
                return std::unexpected(
                           "invalid: candidate_id does not identify the requested target" );
            }
        }
        if( snapshot.target->minimum_position
            && ( event.position->x < snapshot.target->minimum_position->x
                 || event.position->y < snapshot.target->minimum_position->y
                 || event.position->z < snapshot.target->minimum_position->z ) ) {
            return std::unexpected(
                       "invalid: target position is below the active coordinate bounds" );
        }
        if( snapshot.target->maximum_position
            && ( event.position->x > snapshot.target->maximum_position->x
                 || event.position->y > snapshot.target->maximum_position->y
                 || event.position->z > snapshot.target->maximum_position->z ) ) {
            return std::unexpected(
                       "invalid: target position is above the active coordinate bounds" );
        }
        if( target_distance( *snapshot.target, *event.position ) > snapshot.target->range ) {
            return std::unexpected( "invalid: target position is outside the active range" );
        }
        return {};
    }
    if( event.target_id.empty() || !event.submit || event.count || event.position ) {
        return std::unexpected( "invalid: fill requires field_id, value, and explicit submit" );
    }
    if( const auto choice = find_id( snapshot.choices, event.target_id ); choice != nullptr &&
        choice->editor ) {
        if( !choice->selectable ) { return std::unexpected( "disabled: choice is not selectable" ); }
        return validate_editor_value( *choice->editor, event.value );
    }
    if( !snapshot.field || snapshot.field->id != event.target_id ) {
        return std::unexpected( "invalid: field_id is not in the active interaction" );
    }
    return validate_field_value( *snapshot.field, event.value );
}

} // namespace

interaction_scope::interaction_scope( const input_context &context, interaction_provider provider )
    : token_( ++next_provider_token )
{
    providers.push_back( {.context = &context,
                          .provider = std::make_shared<interaction_provider>( std::move( provider ) ),
                          .token = token_} );
}

interaction_scope::~interaction_scope()
{
    const auto entry = std::ranges::find( providers, token_, &provider_entry::token );
    if( entry != providers.end() ) { providers.erase( entry ); }
}

namespace
{
/// Row-major `cells` as color runs, without the blank cells that end a row.
auto canvas_of( const std::vector<screen_cell> &cells, const int width ) -> interaction_canvas
{
    auto result = interaction_canvas{ .cols = width };
    if( width <= 0 ) { return result; }
    for( const auto y : std::views::iota( 0, static_cast<int>( cells.size() ) / width ) ) {
        auto row = std::vector<interaction_canvas_run> {};
        for( const auto &cell : cells | std::views::drop( y * width ) | std::views::take( width ) ) {
            if( row.empty() || row.back().foreground != cell.foreground ||
                row.back().background != cell.background ) {
                row.push_back( { .foreground = cell.foreground, .background = cell.background } );
            }
            row.back().text += cell.text;
        }
        // Blank cells over the default background carry nothing a client draws.
        while( !row.empty() && row.back().background == 0 &&
               row.back().text.find_first_not_of( ' ' ) == std::string::npos ) {
            row.pop_back();
        }
        if( !row.empty() && row.back().background == 0 ) {
            row.back().text.erase( row.back().text.find_last_not_of( ' ' ) + 1 );
        }
        result.rows.push_back( std::move( row ) );
    }
    return result;
}

/// Whether one of the windows `areas` covers the terminal cell `at`.
auto covered_by( const std::vector<rectangle<point>> &areas, const point &at ) -> bool
{
    return std::ranges::any_of( areas, [&]( const auto & area ) {
        return at.x >= area.p_min.x && at.y >= area.p_min.y && at.x < area.p_max.x && at.y < area.p_max.y;
    } );
}
} // namespace

auto composed_canvas() -> interaction_canvas
{
    // The screen is what the windows above the game's map and sidebar cover: the bounding box of those windows, with
    // what the map and the sidebar drew in the gaps between them left blank.
    const auto screen = memory::snapshot();
    const auto height = screen.width > 0 ? static_cast<int>( screen.cells.size() ) / screen.width : 0;
    auto areas = std::vector<rectangle<point>> {};
    for( const auto &area : ui_adaptor::layer_areas() ) {
        areas.push_back( { point( std::max( area.p_min.x, 0 ), std::max( area.p_min.y, 0 ) ),
                           point( std::min( area.p_max.x, screen.width ), std::min( area.p_max.y, height ) ) } );
    }
    if( areas.empty() ) {
        return canvas_of( screen.cells, screen.width );
    }
    const auto box = std::ranges::fold_left( areas, areas.front(), []( const auto & sum,
    const auto & area ) {
        return rectangle<point> { point( std::min( sum.p_min.x, area.p_min.x ), std::min( sum.p_min.y, area.p_min.y ) ),
                                  point( std::max( sum.p_max.x, area.p_max.x ), std::max( sum.p_max.y, area.p_max.y ) )
                                };
    } );
    const auto size = box.p_max - box.p_min;
    auto cells = std::vector<screen_cell> {};
    for( const auto y : std::views::iota( box.p_min.y, box.p_max.y ) ) {
        for( const auto x : std::views::iota( box.p_min.x, box.p_max.x ) ) {
            cells.push_back( covered_by( areas, point( x,
                                         y ) ) ? screen.cells[y * screen.width + x] : screen_cell{} );
        }
    }
    return canvas_of( cells, size.x );
}

auto terminal_canvas( const rectangle<point> &area ) -> interaction_canvas
{
    // What a window above the game drew in the area is its own, so the area shows what the game drew.
    const auto screen = memory::snapshot_below();
    const auto height = screen.width > 0 ? static_cast<int>( screen.cells.size() ) / screen.width : 0;
    const auto from = point( std::max( area.p_min.x, 0 ), std::max( area.p_min.y, 0 ) );
    const auto to = point( std::min( area.p_max.x, screen.width ), std::min( area.p_max.y, height ) );
    auto cells = std::vector<screen_cell> {};
    for( const auto y : std::views::iota( from.y, to.y ) ) {
        for( const auto x : std::views::iota( from.x, to.x ) ) { cells.push_back( screen.cells[y * screen.width + x] ); }
    }
    return canvas_of( cells, std::max( to.x - from.x, 0 ) );
}

auto overmap_tiles() -> bool { return wire_overmap_tiles; }
auto set_overmap_tiles( const bool tiles ) -> void { wire_overmap_tiles = tiles; }

auto current_interaction( interaction_page page ) -> interaction_snapshot
{
    return materialize( acquire_active(), page );
}

auto resolve_interaction_command( const interaction_command &command )
-> std::expected<interaction_event, std::string>
{
    auto result = resolve_checked_interaction( { .command = command } );
    if( !result ) { return std::unexpected( std::move( result.error().message ) ); }
    return std::move( *result );
}

auto resolve_checked_interaction( const interaction_validation_request &request )
-> std::expected<interaction_event, interaction_validation_error>
{
    const auto &command = request.command;
    if( command.input_id != current_input_id() ) {
        return std::unexpected( interaction_validation_error{
            .kind = interaction_rejection::stale_boundary,
            .message = "stale: input boundary changed; query bn.interaction again",
        } );
    }
    const auto active = active_input_context();
    if( active.context == nullptr ) {
        return std::unexpected( interaction_validation_error{
            .message = "unsupported: no active input interaction",
        } );
    }
    const auto snapshot = acquire( *active.context );
    if( request.target_space && ( !snapshot.target ||
                                  *request.target_space != snapshot.target->coordinate_space ) ) {
        return std::unexpected( interaction_validation_error{
            .message = "invalid: target coordinate space changed",
        } );
    }
    auto event = interaction_event{
        .operation = command.operation,
        .schema_id = snapshot.schema_id,
        .target_id = command.target_id,
        .value = command.value,
        .submit = command.submit,
        .count = command.count,
        .position = command.position,
    };
    if( const auto valid = validate( snapshot, event ); !valid ) {
        return std::unexpected( interaction_validation_error{ .message = valid.error() } );
    }
    return event;
}

auto validate_interaction_event( const input_context &context, const interaction_event &event )
-> std::expected<void, std::string>
{
    return validate( acquire( context ), event );
}

auto interaction_kind_name( const interaction_kind kind ) -> std::string
{
    switch( kind ) {
        case interaction_kind::choices:
            return "choices";
        case interaction_kind::field:
            return "field";
        case interaction_kind::inventory:
            return "inventory";
        case interaction_kind::target:
            return "target";
        case interaction_kind::custom:
            return "custom";
    }
    return "custom";
}

auto target_distance( const interaction_target &target, const interaction_position &to ) -> int
{
    const auto dx = static_cast<double>( to.x ) - target.source.x;
    const auto dy = static_cast<double>( to.y ) - target.source.y;
    const auto dz = static_cast<double>( to.z ) - target.source.z;
    return target.distance_metric == "trig" ? static_cast<int>( std::round( std::sqrt(
                dx * dx + dy * dy + dz * dz ) ) )
           : static_cast<int>( std::max( {std::abs( dx ), std::abs( dy ), std::abs( dz )} ) );
}

auto target_bounds_contain( const interaction_target &target,
                            const interaction_position &to ) -> bool
{
    const auto above = [&]( const interaction_position & low ) { return to.x < low.x || to.y < low.y || to.z < low.z; };
    const auto below = [&]( const interaction_position & high ) { return to.x > high.x || to.y > high.y || to.z > high.z; };
    return !( target.minimum_position && above( *target.minimum_position ) ) &&
           !( target.maximum_position && below( *target.maximum_position ) );
}

auto opaque_interaction_id( const std::string &prefix, const std::vector<std::string> &identity )
-> std::string
{
    auto hash = content_hash{};
    for( const auto &part : identity ) { hash.add( part ); }
    return prefix + ":" + hex_digest( hash );
}

} // namespace game_client
