#include "engine_client_contract.h"
#include "engine_client_event.h"

#include <nlohmann/json.hpp>
#include <ranges>

namespace engine_client
{
namespace
{
/// Insertion order is the documented member order of the schema.
using json = nlohmann::ordered_json;

auto to_json( const position &value ) -> json;
auto to_json( const look &value ) -> json;
auto to_json( const field_entry &value ) -> json;
auto to_json( const cell &value ) -> json;
auto to_json( const entity &value ) -> json;
auto to_json( const avatar_stat &value ) -> json;
auto to_json( const inventory_entry &value ) -> json;
auto to_json( const game_client::interaction_choice &choice ) -> json;

auto to_json( const clock_point &at ) -> json
{
    return {{"epoch", at.epoch}, {"sequence", std::to_string( at.sequence )},
        {"revision", std::to_string( at.revision )}};
}
auto to_json( const position &value ) -> json
{
    return {{"dim", value.dim}, {"x", value.x}, {"y", value.y}, {"z", value.z}};
}
/// Native interaction positions are bubble-relative; the wire is absolute.
auto to_json( const game_client::interaction_position &value, const bubble_frame &frame ) -> json
{
    return to_json( position{.dim = frame.dim, .x = value.x + frame.x, .y = value.y + frame.y, .z = value.z} );
}
auto to_json( const look &value ) -> json
{
    return {{"kind", value.kind}, {"id", value.id ? json( *value.id ) : json( nullptr )},
        {"glyph", value.glyph}, {"color", value.color}};
}
template<typename Values>
auto to_array( const Values &values ) -> json
{
    auto result = json::array();
    for( const auto &value : values ) { result.push_back( to_json( value ) ); }
    return result;
}
auto to_json( const field_entry &value ) -> json
{
    return {{"look", to_json( value.appearance )}, {"intensity", value.intensity}};
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
auto to_json( const cell &value ) -> json
{
    auto result = json{{"at", to_json( value.at )}, {"known", known_name( value.known )}};
    if( value.terrain ) { result["terrain"] = to_json( *value.terrain ); }
    if( value.furniture ) { result["furniture"] = to_json( *value.furniture ); }
    if( !value.fields.empty() ) { result["fields"] = to_array( value.fields ); }
    if( !value.traps.empty() ) { result["traps"] = to_array( value.traps ); }
    if( !value.items.empty() ) { result["items"] = to_array( value.items ); }
    if( value.vehicle ) { result["vehicle"] = to_json( *value.vehicle ); }
    if( value.light ) { result["light"] = *value.light; }
    if( value.memory ) {
        result["memory"] = {{"terrain", to_json( value.memory->terrain )}};
        if( value.memory->overlay ) { result["memory"]["overlay"] = to_json( *value.memory->overlay ); }
    }
    return result;
}
auto to_json( const entity &value ) -> json
{
    auto result = json{{"id", value.id}, {"at", to_json( value.at )}, {"known", known_name( value.known )}};
    if( value.appearance ) { result["look"] = to_json( *value.appearance ); }
    if( value.name ) { result["name"] = *value.name; }
    if( !value.statuses.empty() ) { result["statuses"] = value.statuses; }
    if( value.sense ) { result["sense"] = *value.sense; }
    return result;
}
auto to_json( const avatar_stat &value ) -> json
{
    return {{"id", value.id}, {"label", value.label}, {"value", value.value}};
}
auto to_json( const inventory_entry &value ) -> json
{
    auto result = json{{"look", to_json( value.appearance )}, {"name", value.name}};
    if( value.count ) { result["count"] = *value.count; }
    return result;
}
auto to_json( const avatar_value &value ) -> json
{
    auto result = json{{"id", value.id}, {"at", to_json( value.at )}, {"name", value.name},
        {"stats", to_array( value.stats )}};
    if( !value.inventory.empty() ) { result["inventory"] = to_array( value.inventory ); }
    return result;
}
auto to_json( const environment_value &value ) -> json
{
    return {{"turn", value.turn}, {"time", value.time}, {"weather", value.weather}};
}
auto to_json( const bounds &value ) -> json
{
    return {{"min", to_json( value.min )}, {"max", to_json( value.max )}};
}
auto to_json( const game_client::interaction_choice &choice ) -> json
{
    auto result = json{{"id", choice.id}, {"label", choice.label}, {"description", choice.description},
        {"denial", choice.denial}};
    if( choice.pane_id ) { result["pane_id"] = *choice.pane_id; }
    if( choice.area_id ) { result["area_id"] = *choice.area_id; }
    if( !choice.storage_kind.empty() ) { result["storage_kind"] = choice.storage_kind; }
    result["enabled"] = choice.enabled;
    result["selectable"] = choice.selectable;
    result["selected"] = choice.selected;
    result["columns"] = json::array();
    for( const auto &column : choice.columns ) {
        result["columns"].push_back( {{"label", column.label}, {"value", column.value}} );
    }
    if( choice.selected_count ) { result["selected_count"] = *choice.selected_count; }
    if( choice.minimum_count ) { result["minimum_count"] = *choice.minimum_count; }
    if( choice.available_count ) { result["available_count"] = *choice.available_count; }
    return result;
}
auto interaction_json( const boundary_state &state ) -> json
{
    if( !state.interaction ) { return nullptr; }
    const auto &native = *state.interaction;
    const auto &frame = state.frame;
    auto result = json{{"schema_id", native.schema_id}, {"context", native.context},
        {"kind", game_client::interaction_kind_name( native.kind )}, {"allow_cancel", native.allow_cancel},
        {"allow_set_count", native.allow_set_count}, {"title", native.title}, {"message", native.message},
        {"choices", to_array( native.choices )}, {"choice_total", native.choice_total}};
    if( native.field ) {
        const auto &field = *native.field;
        result["field"] = {{"id", field.id}, {"label", field.label}, {"description", field.description},
            {"value", field.value}, {"type", field.type}, {"max_length", field.max_length},
            {"printable", field.printable}
        };
    }
    if( native.target ) {
        const auto &target = *native.target;
        auto value = json{{"source", to_json( target.source, frame )}, {"current", to_json( target.cursor, frame )}};
        if( target.minimum_position ) { value["minimum"] = to_json( *target.minimum_position, frame ); }
        if( target.maximum_position ) { value["maximum"] = to_json( *target.maximum_position, frame ); }
        value["range"] = target.range;
        value["distance_metric"] = target.distance_metric;
        value["status"] = target.status;
        value["candidates"] = json::array();
        for( const auto &candidate : target.candidates ) {
            value["candidates"].push_back( {{"id", candidate.id}, {"label", candidate.label},
                {"description", candidate.description}, {"creature", candidate.creature},
                {"position", to_json( candidate.position, frame )}} );
        }
        result["target"] = std::move( value );
    }
    auto compat = json::object();
    if( !native.focus_choice_id.empty() ) {
        compat["focus"] = json::object();
        if( native.focus_pane_id ) { compat["focus"]["pane_id"] = *native.focus_pane_id; }
        compat["focus"]["choice_id"] = native.focus_choice_id;
    }
    compat["panes"] = json::array();
    for( const auto &pane : native.panes ) {
        compat["panes"].push_back( {{"id", pane.id}, {"label", pane.label}, {"role", pane.role},
            {"area_id", pane.area_id}, {"area_label", pane.area_label},
            {"area_description", pane.area_description}, {"filter", pane.filter},
            {"storage_kind", pane.storage_kind}} );
    }
    result["compat"] = std::move( compat );
    return result;
}
auto to_json( const boundary_state &state ) -> json
{
    auto result = json{{"boundary_id", state.id},
        {
            "readiness", {
                {"phase", state.ready.phase}, {"game_ready", state.ready.game_ready},
                {"accepts_interaction_commands", state.ready.accepts_interaction_commands},
                {"accepts_registered_actions", state.ready.accepts_registered_actions}
            }
        }};
    result["actions"] = json::array();
    for( const auto &entry : state.actions ) { result["actions"].push_back( {{"id", entry.id}, {"name", entry.name}} ); }
    result["interaction"] = interaction_json( state );
    return result;
}
auto to_json( const changes &delta ) -> json
{
    auto result = json::object();
    if( delta.coverage ) { result["coverage"] = to_json( *delta.coverage ); }
    if( !delta.cells.empty() ) { result["cells"] = to_array( delta.cells ); }
    if( !delta.forgotten.empty() ) { result["forgotten"] = to_array( delta.forgotten ); }
    if( !delta.entities.empty() ) { result["entities"] = to_array( delta.entities ); }
    if( !delta.gone.empty() ) {
        result["gone"] = json::array();
        for( const auto &value : delta.gone ) { result["gone"].push_back( {{"id", value.id}, {"reason", value.reason}} ); }
    }
    if( delta.avatar ) { result["avatar"] = to_json( *delta.avatar ); }
    if( delta.environment ) { result["environment"] = to_json( *delta.environment ); }
    if( delta.interaction ) { result["interaction"] = to_json( *delta.interaction ); }
    return result;
}
auto to_json( const event_value &value ) -> json
{
    auto result = json{{"sequence", std::to_string( value.sequence )}, {"revision", std::to_string( value.revision )},
        {"type", value.type}};
    if( value.cause ) { result["cause"] = std::to_string( *value.cause ); }
    if( value.command ) { result["command"] = *value.command; }
    result["changes"] = to_json( value.delta );
    return result;
}
} // namespace

auto serialize_clock( const clock_point &at ) -> std::string { return to_json( at ).dump(); }
auto serialize_boundary( const boundary_state &state ) -> std::string { return to_json( state ).dump(); }
auto serialize_choices( const choices_page &page ) -> std::string
{
    return json{{"boundary_id", page.boundary_id}, {"total", page.total}, {"choices", to_array( page.choices )}}.dump();
}
auto serialize_command_result( const command_result &value ) -> std::string
{
    auto result = json{{"epoch", value.epoch}, {"command_id", value.command_id}, {"stage", stage_name( value.stage )}};
    if( value.failure ) { result["error"] = error_name( *value.failure ); }
    if( value.at ) { result["at"] = to_json( *value.at ); }
    return result.dump();
}
auto serialize_events( const event_batch &batch ) -> std::string
{
    auto events = json::array();
    for( const auto &event : batch.events ) { events.push_back( to_json( event.value() ) ); }
    return json{{"epoch", batch.epoch}, {"events", std::move( events )}}.dump();
}
auto snapshot_part_count( const snapshot &value ) -> std::size_t
{
    return ( value.value.world.cells.size() + cells_per_part - 1 ) / cells_per_part;
}
auto serialize_snapshot_header( const snapshot &value ) -> std::string
{
    const auto &world = value.value.world;
    auto result = json{{"at", to_json( value.at )}};
    if( world.coverage ) { result["coverage"] = to_json( *world.coverage ); }
    result["interaction"] = to_json( value.value.interaction );
    if( world.avatar ) { result["avatar"] = to_json( *world.avatar ); }
    if( world.environment ) { result["environment"] = to_json( *world.environment ); }
    result["entities"] = json::array();
    for( const auto &entry : world.entities ) { result["entities"].push_back( to_json( entry.second ) ); }
    result["parts"] = snapshot_part_count( value );
    return result.dump();
}
auto serialize_snapshot_part( const snapshot &value, const std::size_t index ) -> std::string
{
    auto cells = json::array();
    for( const auto &entry : value.value.world.cells | std::views::drop( index * cells_per_part ) |
         std::views::take( cells_per_part ) ) {
        cells.push_back( to_json( entry.second ) );
    }
    return json{{"epoch", value.at.epoch}, {"at", to_json( value.at )}, {"index", index},
        {"last", index + 1 >= snapshot_part_count( value )}, {"cells", std::move( cells )}}.dump();
}
auto serialize_resync( const std::string &epoch, const std::string_view reason,
                       const counter lost_after )
-> std::string
{
    return json{{"epoch", epoch}, {"reason", std::string{reason}}, {"lost_after", std::to_string( lost_after )}}.dump();
}
} // namespace engine_client
