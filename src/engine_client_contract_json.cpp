#include "engine_client_contract.h"
#include "engine_client_event.h"
#include "json.h"

#include <algorithm>
#include <ranges>
#include <sstream>

namespace engine_client
{
namespace
{
auto write_counter( JsonOut &out, const counter value ) -> void { out.write( std::to_string( value ) ); }
auto write_clock_value( JsonOut &out, const clock_point &at ) -> void
{
    out.start_object();
    out.member( "epoch", at.epoch );
    out.member( "sequence" );
    write_counter( out, at.sequence );
    out.member( "revision" );
    write_counter( out, at.revision );
    out.end_object();
}
auto write_pos( JsonOut &out, const position &value ) -> void
{
    out.start_object();
    out.member( "dim", value.dim );
    out.member( "x", value.x );
    out.member( "y", value.y );
    out.member( "z", value.z );
    out.end_object();
}
/// Native interaction positions are bubble-relative; the wire is absolute.
auto write_native_pos( JsonOut &out, const game_client::interaction_position &value,
                       const bubble_frame &frame ) -> void
{
    write_pos( out, {.dim = frame.dim, .x = value.x + frame.x, .y = value.y + frame.y, .z = value.z} );
}
auto write_look( JsonOut &out, const look &value ) -> void
{
    out.start_object();
    out.member( "kind", value.kind );
    out.member( "id" );
    if( value.id ) { out.write( *value.id ); }
    else { out.write_null(); }
    out.member( "glyph", value.glyph );
    out.member( "color", value.color );
    out.end_object();
}
auto write_looks( JsonOut &out, const std::vector<look> &values ) -> void
{
    out.start_array();
    for( const auto &value : values ) { write_look( out, value ); }
    out.end_array();
}
auto known_name( const knowledge value ) -> const char *
{
    switch( value ) {
        case knowledge::remembered:
            return "remembered";
        case knowledge::visible:
            return "visible";
        case knowledge::sensed:
            return "sensed";
    }
    return "sensed";
}
auto write_cell( JsonOut &out, const cell &value ) -> void
{
    out.start_object();
    out.member( "at" );
    write_pos( out, value.at );
    out.member( "known", known_name( value.known ) );
    if( value.terrain ) { out.member( "terrain" ); write_look( out, *value.terrain ); }
    if( value.furniture ) { out.member( "furniture" ); write_look( out, *value.furniture ); }
    if( !value.fields.empty() ) {
        out.member( "fields" );
        out.start_array();
        for( const auto &entry : value.fields ) {
            out.start_object();
            out.member( "look" );
            write_look( out, entry.appearance );
            out.member( "intensity", entry.intensity );
            out.end_object();
        }
        out.end_array();
    }
    if( !value.traps.empty() ) { out.member( "traps" ); write_looks( out, value.traps ); }
    if( !value.items.empty() ) { out.member( "items" ); write_looks( out, value.items ); }
    if( value.vehicle ) { out.member( "vehicle" ); write_look( out, *value.vehicle ); }
    if( value.light ) { out.member( "light", *value.light ); }
    if( value.memory ) {
        out.member( "memory" );
        out.start_object();
        out.member( "terrain" );
        write_look( out, value.memory->terrain );
        if( value.memory->overlay ) { out.member( "overlay" ); write_look( out, *value.memory->overlay ); }
        out.end_object();
    }
    out.end_object();
}
auto write_entity( JsonOut &out, const entity &value ) -> void
{
    out.start_object();
    out.member( "id", value.id );
    out.member( "at" );
    write_pos( out, value.at );
    out.member( "known", known_name( value.known ) );
    if( value.appearance ) { out.member( "look" ); write_look( out, *value.appearance ); }
    if( value.name ) { out.member( "name", *value.name ); }
    if( !value.statuses.empty() ) { out.member( "statuses", value.statuses ); }
    if( value.sense ) { out.member( "sense", *value.sense ); }
    out.end_object();
}
auto write_avatar( JsonOut &out, const avatar_value &value ) -> void
{
    out.start_object();
    out.member( "id", value.id );
    out.member( "at" );
    write_pos( out, value.at );
    out.member( "name", value.name );
    out.member( "stats" );
    out.start_array();
    for( const auto &stat : value.stats ) {
        out.start_object();
        out.member( "id", stat.id );
        out.member( "label", stat.label );
        out.member( "value", stat.value );
        out.end_object();
    }
    out.end_array();
    if( !value.inventory.empty() ) {
        out.member( "inventory" );
        out.start_array();
        for( const auto &entry : value.inventory ) {
            out.start_object();
            out.member( "look" );
            write_look( out, entry.appearance );
            out.member( "name", entry.name );
            if( entry.count ) { out.member( "count", *entry.count ); }
            out.end_object();
        }
        out.end_array();
    }
    out.end_object();
}
auto write_environment( JsonOut &out, const environment_value &value ) -> void
{
    out.start_object();
    out.member( "turn", value.turn );
    out.member( "time", value.time );
    out.member( "weather", value.weather );
    out.end_object();
}
auto write_bounds( JsonOut &out, const bounds &value ) -> void
{
    out.start_object();
    out.member( "min" );
    write_pos( out, value.min );
    out.member( "max" );
    write_pos( out, value.max );
    out.end_object();
}
auto write_choice( JsonOut &out, const game_client::interaction_choice &choice ) -> void
{
    out.start_object();
    out.member( "id", choice.id );
    out.member( "label", choice.label );
    out.member( "description", choice.description );
    out.member( "denial", choice.denial );
    if( choice.pane_id ) { out.member( "pane_id", *choice.pane_id ); }
    if( choice.area_id ) { out.member( "area_id", *choice.area_id ); }
    if( !choice.storage_kind.empty() ) { out.member( "storage_kind", choice.storage_kind ); }
    out.member( "enabled", choice.enabled );
    out.member( "selectable", choice.selectable );
    out.member( "selected", choice.selected );
    out.member( "columns" );
    out.start_array();
    for( const auto &column : choice.columns ) {
        out.start_object();
        out.member( "label", column.label );
        out.member( "value", column.value );
        out.end_object();
    }
    out.end_array();
    if( choice.selected_count ) { out.member( "selected_count", *choice.selected_count ); }
    if( choice.minimum_count ) { out.member( "minimum_count", *choice.minimum_count ); }
    if( choice.available_count ) { out.member( "available_count", *choice.available_count ); }
    out.end_object();
}
auto write_interaction( JsonOut &out, const boundary_state &state ) -> void
{
    if( !state.interaction ) { out.write_null(); return; }
    const auto &native = *state.interaction;
    const auto &frame = state.frame;
    out.start_object();
    out.member( "schema_id", native.schema_id );
    out.member( "context", native.context );
    out.member( "kind", game_client::interaction_kind_name( native.kind ) );
    out.member( "allow_cancel", native.allow_cancel );
    out.member( "allow_set_count", native.allow_set_count );
    out.member( "title", native.title );
    out.member( "message", native.message );
    out.member( "choices" );
    out.start_array();
    for( const auto &choice : native.choices ) { write_choice( out, choice ); }
    out.end_array();
    out.member( "choice_total", native.choice_total );
    if( native.field ) {
        const auto &field = *native.field;
        out.member( "field" );
        out.start_object();
        out.member( "id", field.id );
        out.member( "label", field.label );
        out.member( "description", field.description );
        out.member( "value", field.value );
        out.member( "type", field.type );
        out.member( "max_length", field.max_length );
        out.member( "printable", field.printable );
        out.end_object();
    }
    if( native.target ) {
        const auto &target = *native.target;
        out.member( "target" );
        out.start_object();
        out.member( "source" );
        write_native_pos( out, target.source, frame );
        out.member( "current" );
        write_native_pos( out, target.cursor, frame );
        if( target.minimum_position ) {
            out.member( "minimum" );
            write_native_pos( out, *target.minimum_position, frame );
        }
        if( target.maximum_position ) {
            out.member( "maximum" );
            write_native_pos( out, *target.maximum_position, frame );
        }
        out.member( "range", target.range );
        out.member( "distance_metric", target.distance_metric );
        out.member( "status", target.status );
        out.member( "candidates" );
        out.start_array();
        for( const auto &candidate : target.candidates ) {
            out.start_object();
            out.member( "id", candidate.id );
            out.member( "label", candidate.label );
            out.member( "description", candidate.description );
            out.member( "creature", candidate.creature );
            out.member( "position" );
            write_native_pos( out, candidate.position, frame );
            out.end_object();
        }
        out.end_array();
        out.end_object();
    }
    out.member( "compat" );
    out.start_object();
    if( !native.focus_choice_id.empty() ) {
        out.member( "focus" );
        out.start_object();
        if( native.focus_pane_id ) { out.member( "pane_id", *native.focus_pane_id ); }
        out.member( "choice_id", native.focus_choice_id );
        out.end_object();
    }
    out.member( "panes" );
    out.start_array();
    for( const auto &pane : native.panes ) {
        out.start_object();
        out.member( "id", pane.id );
        out.member( "label", pane.label );
        out.member( "role", pane.role );
        out.member( "area_id", pane.area_id );
        out.member( "area_label", pane.area_label );
        out.member( "area_description", pane.area_description );
        out.member( "filter", pane.filter );
        out.member( "storage_kind", pane.storage_kind );
        out.end_object();
    }
    out.end_array();
    out.end_object();
    out.end_object();
}
auto write_boundary( JsonOut &out, const boundary_state &state ) -> void
{
    out.start_object();
    out.member( "boundary_id", state.id );
    out.member( "readiness" );
    out.start_object();
    out.member( "phase", state.ready.phase );
    out.member( "game_ready", state.ready.game_ready );
    out.member( "accepts_interaction_commands", state.ready.accepts_interaction_commands );
    out.member( "accepts_registered_actions", state.ready.accepts_registered_actions );
    out.end_object();
    out.member( "actions" );
    out.start_array();
    for( const auto &entry : state.actions ) {
        out.start_object();
        out.member( "id", entry.id );
        out.member( "name", entry.name );
        out.end_object();
    }
    out.end_array();
    out.member( "interaction" );
    write_interaction( out, state );
    out.end_object();
}
auto write_changes( JsonOut &out, const changes &delta ) -> void
{
    out.start_object();
    if( delta.coverage ) { out.member( "coverage" ); write_bounds( out, *delta.coverage ); }
    if( !delta.cells.empty() ) {
        out.member( "cells" );
        out.start_array();
        for( const auto &value : delta.cells ) { write_cell( out, value ); }
        out.end_array();
    }
    if( !delta.forgotten.empty() ) {
        out.member( "forgotten" );
        out.start_array();
        for( const auto &value : delta.forgotten ) { write_pos( out, value ); }
        out.end_array();
    }
    if( !delta.entities.empty() ) {
        out.member( "entities" );
        out.start_array();
        for( const auto &value : delta.entities ) { write_entity( out, value ); }
        out.end_array();
    }
    if( !delta.gone.empty() ) {
        out.member( "gone" );
        out.start_array();
        for( const auto &value : delta.gone ) {
            out.start_object();
            out.member( "id", value.id );
            out.member( "reason", value.reason );
            out.end_object();
        }
        out.end_array();
    }
    if( delta.avatar ) { out.member( "avatar" ); write_avatar( out, *delta.avatar ); }
    if( delta.environment ) { out.member( "environment" ); write_environment( out, *delta.environment ); }
    if( delta.interaction ) { out.member( "interaction" ); write_boundary( out, *delta.interaction ); }
    out.end_object();
}
auto write_event( JsonOut &out, const event_value &value ) -> void
{
    out.start_object();
    out.member( "sequence" );
    write_counter( out, value.sequence );
    out.member( "revision" );
    write_counter( out, value.revision );
    out.member( "type", value.type );
    if( value.cause ) { out.member( "cause" ); write_counter( out, *value.cause ); }
    if( value.command ) { out.member( "command", *value.command ); }
    out.member( "changes" );
    write_changes( out, value.delta );
    out.end_object();
}
template<typename Write>
auto render( const Write &write ) -> std::string
{
    auto output = std::ostringstream{};
    auto out = JsonOut{output};
    write( out );
    return output.str();
}
} // namespace

auto serialize_clock( const clock_point &at ) -> std::string
{
    return render( [&]( JsonOut & out ) { write_clock_value( out, at ); } );
}
auto serialize_boundary( const boundary_state &state ) -> std::string
{
    return render( [&]( JsonOut & out ) { write_boundary( out, state ); } );
}
auto serialize_choices( const choices_page &page ) -> std::string
{
    return render( [&]( JsonOut & out ) {
        out.start_object();
        out.member( "boundary_id", page.boundary_id );
        out.member( "total", page.total );
        out.member( "choices" );
        out.start_array();
        for( const auto &choice : page.choices ) { write_choice( out, choice ); }
        out.end_array();
        out.end_object();
    } );
}
auto serialize_command_result( const command_result &value ) -> std::string
{
    return render( [&]( JsonOut & out ) {
        out.start_object();
        out.member( "epoch", value.epoch );
        out.member( "command_id", value.command_id );
        out.member( "stage", stage_name( value.stage ) );
        if( value.failure ) { out.member( "error", error_name( *value.failure ) ); }
        if( value.at ) { out.member( "at" ); write_clock_value( out, *value.at ); }
        out.end_object();
    } );
}
auto serialize_events( const event_batch &batch ) -> std::string
{
    return render( [&]( JsonOut & out ) {
        out.start_object();
        out.member( "epoch", batch.epoch );
        out.member( "events" );
        out.start_array();
        for( const auto &event : batch.events ) { write_event( out, event.value() ); }
        out.end_array();
        out.end_object();
    } );
}
auto serialize_snapshot_header( const snapshot &value ) -> std::string
{
    return render( [&]( JsonOut & out ) {
        const auto &world = value.value.world;
        out.start_object();
        out.member( "at" );
        write_clock_value( out, value.at );
        if( world.coverage ) { out.member( "coverage" ); write_bounds( out, *world.coverage ); }
        out.member( "interaction" );
        write_boundary( out, value.value.interaction );
        if( world.avatar ) { out.member( "avatar" ); write_avatar( out, *world.avatar ); }
        if( world.environment ) { out.member( "environment" ); write_environment( out, *world.environment ); }
        out.member( "entities" );
        out.start_array();
        for( const auto &entry : world.entities ) { write_entity( out, entry.second ); }
        out.end_array();
        out.member( "parts", snapshot_part_count( value ) );
        out.end_object();
    } );
}
auto snapshot_part_count( const snapshot &value ) -> std::size_t
{
    return ( value.value.world.cells.size() + cells_per_part - 1 ) / cells_per_part;
}
auto serialize_snapshot_part( const snapshot &value, const std::size_t index ) -> std::string
{
    return render( [&]( JsonOut & out ) {
        const auto &cells = value.value.world.cells;
        out.start_object();
        out.member( "epoch", value.at.epoch );
        out.member( "at" );
        write_clock_value( out, value.at );
        out.member( "index", index );
        out.member( "last", index + 1 >= snapshot_part_count( value ) );
        out.member( "cells" );
        out.start_array();
        for( const auto &entry : cells | std::views::drop( index * cells_per_part ) |
             std::views::take( cells_per_part ) ) {
            write_cell( out, entry.second );
        }
        out.end_array();
        out.end_object();
    } );
}
auto serialize_resync( const std::string &epoch, const std::string_view reason,
                       const counter lost_after )
-> std::string
{
    return render( [&]( JsonOut & out ) {
        out.start_object();
        out.member( "epoch", epoch );
        out.member( "reason", std::string{reason} );
        out.member( "lost_after" );
        write_counter( out, lost_after );
        out.end_object();
    } );
}
} // namespace engine_client
