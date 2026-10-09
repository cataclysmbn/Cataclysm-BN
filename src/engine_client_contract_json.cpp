#include "engine_client_contract.h"
#include "engine_client_event.h"
#include "json.h"

#include <sstream>

namespace engine_client
{
namespace
{
auto write_position( JsonOut &out, const game_client::interaction_position &position,
                     const std::string &frame ) -> void
{
    out.start_object();
    out.member( "space", "reality_bubble_map_square" );
    out.member( "frame_id", frame );
    out.member( "x", position.x );
    out.member( "y", position.y );
    out.member( "z", position.z );
    out.end_object();
}
auto write_interaction( JsonOut &out, const state_value &state ) -> void
{
    if( !state.interaction ) { out.write_null(); return; }
    const auto &native = *state.interaction;
    // Explicit 1.0 mapping of the native owned model, not a second interaction model.
    // Future native fields must not implicitly change this version's closed wire schema.
    out.start_object();
    out.member( "schema_id", native.schema_id );
    out.member( "context", native.context );
    out.member( "kind", game_client::interaction_kind_name( native.kind ) );
    out.member( "structured", native.structured );
    out.member( "actions_only", native.actions_only );
    out.member( "allow_cancel", native.allow_cancel );
    out.member( "allow_set_count", native.allow_set_count );
    out.member( "title", native.title );
    out.member( "message", native.message );
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
    out.member( "choices" );
    out.start_array();
    for( const auto &choice : native.choices ) {
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
        out.member( "highlighted", choice.highlighted );
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
    out.end_array();
    out.member( "choice_page" );
    out.start_object();
    out.member( "offset", native.choice_offset );
    out.member( "count", native.choices.size() );
    out.member( "total", native.choice_total );
    out.end_object();
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
        write_position( out, target.source, state.input_boundary_id );
        out.member( "cursor" );
        write_position( out, target.cursor, state.input_boundary_id );
        if( target.minimum_position ) {
            out.member( "minimum_position" );
            write_position( out, *target.minimum_position, state.input_boundary_id );
        }
        if( target.maximum_position ) {
            out.member( "maximum_position" );
            write_position( out, *target.maximum_position, state.input_boundary_id );
        }
        out.member( "range", target.range );
        out.member( "distance_metric", target.distance_metric );
        out.member( "status", target.status );
        out.member( "limit_to_reality_bubble", target.limit_to_reality_bubble );
        out.member( "candidates" );
        out.start_array();
        for( const auto &candidate : target.candidates ) {
            out.start_object();
            out.member( "id", candidate.id );
            out.member( "label", candidate.label );
            out.member( "description", candidate.description );
            out.member( "creature", candidate.creature );
            out.member( "position" );
            write_position( out, candidate.position, state.input_boundary_id );
            out.end_object();
        }
        out.end_array();
        out.end_object();
    }
    out.end_object();
}
auto write_state( JsonOut &out, const state_value &state ) -> void
{
    out.start_object();
    out.member( "projection" );
    out.start_object();
    out.member( "kind", "interaction_choices" );
    out.member( "offset", state.view.offset );
    out.member( "limit", state.view.limit );
    out.end_object();
    out.member( "readiness" );
    out.start_object();
    out.member( "phase", state.ready.phase );
    out.member( "game_ready", state.ready.game_ready );
    out.member( "accepts_interaction_commands", state.ready.accepts_interaction_commands );
    out.member( "accepts_registered_actions", state.ready.accepts_registered_actions );
    out.end_object();
    out.member( "input_boundary_id", state.input_boundary_id );
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
auto write_event( JsonOut &out, const public_event &event ) -> void
{
    const auto &value = event.value();
    out.start_object();
    out.member( "contract_version", contract_version );
    out.member( "session_epoch", value.session_epoch );
    out.member( "event_id", value.event_id );
    out.member( "public_sequence", std::to_string( value.public_sequence ) );
    out.member( "type", "interaction.replaced" );
    out.member( "state_revision", std::to_string( value.state_revision ) );
    out.member( "command_id", value.payload.command_id );
    out.member( "cause_sequence" );
    if( value.payload.cause_sequence ) { out.write( std::to_string( *value.payload.cause_sequence ) ); }
    else { out.write_null(); }
    out.member( "display" );
    if( value.payload.display ) {
        const auto &display = *value.payload.display;
        out.start_object();
        out.member( "group_id", display.group_id );
        out.member( "ordinal", display.ordinal );
        out.member( "count", display.count );
        out.member( "duration_ms", display.duration_ms );
        out.end_object();
    } else { out.write_null(); }
    out.member( "payload" );
    out.start_object();
    out.member( "base_state_revision", std::to_string( value.base_state_revision ) );
    out.member( "state" );
    write_state( out, value.payload.state );
    out.end_object();
    out.end_object();
}
auto stage_name( const command_stage stage ) -> std::string
{
    switch( stage ) {
        case command_stage::received:
            return "received";
        case command_stage::validated:
            return "validated";
        case command_stage::executing:
            return "executing";
        case command_stage::rejected:
            return "rejected";
        case command_stage::completed:
            return "completed";
        case command_stage::interrupted:
            return "interrupted";
    }
    return "interrupted";
}
} // namespace

auto serialize_state( const state_value &state ) -> std::string
{
    auto output = std::ostringstream{};
    auto out = JsonOut{output};
    write_state( out, state );
    return output.str();
}
auto serialize_snapshot( const snapshot &value ) -> std::string
{
    auto output = std::ostringstream{};
    auto out = JsonOut{output};
    out.start_object();
    out.member( "contract_version", contract_version );
    out.member( "session_epoch", value.session_epoch );
    out.member( "state_revision", std::to_string( value.state_revision ) );
    out.member( "through_public_sequence", std::to_string( value.through_public_sequence ) );
    out.member( "state" );
    write_state( out, value.state );
    out.end_object();
    return output.str();
}
auto serialize_event( const public_event &event ) -> std::string
{
    auto output = std::ostringstream{};
    auto out = JsonOut{output};
    write_event( out, event );
    return output.str();
}
namespace
{
auto write_batch( JsonOut &out, const event_batch &batch ) -> void
{
    out.start_object();
    out.member( "complete", true );
    out.member( "first_sequence" );
    if( batch.first_sequence ) { out.write( std::to_string( *batch.first_sequence ) ); }
    else { out.write_null(); }
    out.member( "last_sequence" );
    if( batch.last_sequence ) { out.write( std::to_string( *batch.last_sequence ) ); }
    else { out.write_null(); }
    out.member( "events" );
    out.start_array();
    for( const auto &event : batch.events ) { write_event( out, event ); }
    out.end_array();
    out.end_object();
}
} // namespace
auto serialize_batch( const event_batch &batch ) -> std::string
{
    auto output = std::ostringstream{};
    auto out = JsonOut{output};
    write_batch( out, batch );
    return output.str();
}
auto serialize_negotiation( const negotiated_contract &value,
                            const std::string &epoch ) -> std::string
{
    auto output = std::ostringstream{};
    auto out = JsonOut{output};
    out.start_object();
    out.member( "contract_version", contract_version );
    out.member( "schema_id", "bn-engine-client-1.0" );
    out.member( "session_epoch", epoch );
    out.member( "capabilities", value.capabilities );
    out.member( "limits" );
    out.start_object();
    out.member( "maximum_outstanding_commands", 1 );
    out.member( "maximum_interaction_page_size", maximum_rows );
    out.member( "maximum_inline_events", maximum_inline_events );
    out.member( "maximum_inline_bytes", maximum_inline_bytes );
    out.end_object();
    out.member( "delivery" );
    out.start_object();
    out.member( "selected", "inline_completion" );
    out.member( "push_supported", false );
    out.member( "credits_supported", false );
    out.member( "history_supported", false );
    out.member( "reconnect_supported", false );
    out.member( "resync_snapshot_supported", true );
    out.end_object();
    out.end_object();
    return output.str();
}
namespace
{
auto write_result( JsonOut &out, const command_result &value ) -> void
{
    out.start_object();
    out.member( "session_epoch", value.session_epoch );
    out.member( "command_id", value.command_id );
    out.member( "stage", stage_name( value.stage ) );
    out.member( "receipt" );
    out.start_object();
    out.member( "status", "received" );
    out.end_object();
    out.member( "validation" );
    out.start_object();
    out.member( "status", value.validation_succeeded ? "validated" :
                value.stage == command_stage::rejected ? "rejected" : "pending" );
    if( value.failure ) { out.member( "error", error_name( *value.failure ) ); }
    out.end_object();
    out.member( "execution" );
    out.start_object();
    out.member( "status", value.execution_started ? "native_input_delivered" : "pending" );
    out.end_object();
    out.member( "completion" );
    if( value.completed ) {
        out.start_object();
        out.member( "status", "next_interaction_boundary" );
        out.member( "state_revision", std::to_string( value.completed->state_revision ) );
        out.member( "through_public_sequence", std::to_string( value.completed->through_public_sequence ) );
        out.member( "resync_required", value.completed->resync_required );
        out.end_object();
    } else { out.write_null(); }
    out.end_object();
}
} // namespace
auto serialize_result( const command_result &value ) -> std::string
{
    auto output = std::ostringstream{};
    auto out = JsonOut{output};
    write_result( out, value );
    return output.str();
}
auto serialize_command_response( const command_result &command, const event_batch &batch )
-> std::expected<std::string, error>
{
    if( command.session_epoch.empty() || command.session_epoch.size() > maximum_id_bytes ||
        command.command_id.empty() || command.command_id.size() > maximum_id_bytes ) {
        return std::unexpected( error::validation_failed );
    }
    const auto valid_lifecycle = [&]() {
        switch( command.stage ) {
            case command_stage::received:
                return !command.validation_succeeded && !command.execution_started && !command.failure &&
                       !command.completed;
            case command_stage::validated:
                return command.validation_succeeded && !command.execution_started && !command.failure &&
                       !command.completed;
            case command_stage::executing:
                return command.validation_succeeded && command.execution_started && !command.failure &&
                       !command.completed;
            case command_stage::rejected:
                return !command.validation_succeeded && !command.execution_started && command.failure &&
                       !command.completed;
            case command_stage::completed:
                return command.validation_succeeded && command.execution_started && !command.failure &&
                       command.completed;
            case command_stage::interrupted:
                return !command.failure && !command.completed && ( !command.execution_started ||
                        command.validation_succeeded );
        }
        return false;
    };
    if( !valid_lifecycle() ) { return std::unexpected( error::invalid_lifecycle ); }
    for( const auto &event : batch.events ) {
        if( event.value().session_epoch != command.session_epoch ) { return std::unexpected( error::stale_epoch ); }
        if( event.value().payload.command_id && *event.value().payload.command_id != command.command_id ) {
            return std::unexpected( error::validation_failed );
        }
    }
    if( command.stage == command_stage::completed ) {
        if( command.completed->resync_required && !batch.events.empty() ) {
            return std::unexpected( error::invalid_lifecycle );
        }
        if( !batch.events.empty() ) {
            if( batch.last_sequence != command.completed->through_public_sequence ||
                batch.events.back().value().state_revision != command.completed->state_revision ) {
                return std::unexpected( error::resync_required );
            }
        } else if( !command.completed->resync_required ) {
            return std::unexpected( error::resync_required );
        }
    } else if( !batch.events.empty() || command.completed ) {
        return std::unexpected( error::invalid_lifecycle );
    }
    auto receiver = snapshot{};
    receiver.session_epoch = command.session_epoch;
    if( !batch.events.empty() ) {
        const auto &first = batch.events.front().value();
        receiver.state.view = first.payload.state.view;
        receiver.state_revision = first.base_state_revision;
        receiver.through_public_sequence = first.public_sequence - 1;
    }
    if( const auto valid = apply_batch( receiver, batch ); !valid ) { return std::unexpected( valid.error() ); }
    auto output = std::ostringstream{};
    auto out = JsonOut{output};
    out.start_object();
    out.member( "command" );
    write_result( out, command );
    out.member( "event_batch" );
    write_batch( out, batch );
    out.end_object();
    auto result = output.str();
    if( result.size() > maximum_inline_bytes ) { return std::unexpected( error::resource_limit ); }
    return result;
}
} // namespace engine_client
