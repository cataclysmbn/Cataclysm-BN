#include "engine_client_component.h"
#include "engine_client_contract.h"
#include "engine_client_event.h"
#include "engine_client_panels.h"
#include "string_formatter.h"
#include "translations.h"

#include <algorithm>
#include <cmath>
#include <tuple>
#include <unordered_map>
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
auto to_json( const mode_value &value ) -> json;
auto to_json( const sidebar_text &value ) -> json;
auto to_json( const sidebar_limb &value ) -> json;
auto to_json( const sidebar_layout &value ) -> json;
auto to_json( const heading &value ) -> json;
auto to_json( const compass_direction &value ) -> json;
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
    auto result = json{{"kind", value.kind}, {"id", value.id ? json( *value.id ) : json( nullptr )},
        {"glyph", value.glyph}, {"color", value.color}};
    if( value.name ) { result["name"] = *value.name; }
    if( value.tile ) { result["tile"] = *value.tile; }
    if( !value.looks_like.empty() ) { result["looks_like"] = value.looks_like; }
    // No turn is the default, left out.
    if( value.subtile ) { result["subtile"] = *value.subtile; }
    if( value.rotation && *value.rotation != 0 ) { result["rotation"] = *value.rotation; }
    if( value.facing ) { result["facing"] = *value.facing; }
    if( value.stack ) { result["stack"] = *value.stack; }
    if( value.wall ) { result["wall"] = true; }
    if( value.window ) { result["window"] = true; }
    // No coverage is the default, left out.
    if( value.coverage != 0 ) { result["coverage"] = value.coverage; }
    if( !value.form.empty() ) { result["form"] = value.form; }
    if( value.mutation ) {
        result["mutation"] = {{"id", value.mutation->id}, {"types", value.mutation->types},
            {"flags", value.mutation->flags}
        };
    }
    if( !value.tint_keys.empty() ) { result["tint_keys"] = value.tint_keys; }
    if( value.tint_vars ) {
        result["tint_vars"] = json::object();
        for( const auto &[name, layer] : {std::pair{"bg", &value.tint_vars->bg}, std::pair{"fg", &value.tint_vars->fg}} ) {
            if( layer->empty() ) { continue; }
            auto &out = result["tint_vars"][name] = json::object();
            if( !layer->color.empty() ) { out["color"] = layer->color; }
            if( !layer->blend_mode.empty() ) { out["blend_mode"] = layer->blend_mode; }
            if( layer->contrast ) { out["contrast"] = *layer->contrast; }
            if( layer->saturation ) { out["saturation"] = *layer->saturation; }
            if( layer->brightness ) { out["brightness"] = *layer->brightness; }
        }
    }
    if( value.tint ) {
        result["tint"] = json::object();
        if( !value.tint->bg.empty() ) { result["tint"]["bg"] = value.tint->bg; }
        if( !value.tint->fg.empty() ) { result["tint"]["fg"] = value.tint->fg; }
    }
    return result;
}
/// A look in a cell layer, whose `kind` the layer already says; a cell layer is always shaped, so
/// the `center` subtile is the default there.
auto layer_json( const look &value, const std::string_view layer_kind ) -> json
{
    auto result = to_json( value );
    if( value.kind == layer_kind ) { result.erase( "kind" ); }
    if( value.subtile == "center" ) { result.erase( "subtile" ); }
    return result;
}
template<typename Values>
auto to_array( const Values &values ) -> json
{
    auto result = json::array();
    for( const auto &value : values ) { result.push_back( to_json( value ) ); }
    return result;
}
/// The localized line for a planned map route, which the engine walks when its end is selected again.
auto route_text( const std::vector<position> &route ) -> std::string
{
    const auto steps = static_cast<int>( route.size() );
    //~ The planned map travel route; %d is its length in squares.
    return string_format( vgettext( "Route of %d square; select its end again to go.",
                                    "Route of %d squares; select its end again to go.", steps ), steps );
}
auto layer_array( const std::vector<look> &values, const std::string_view layer_kind ) -> json
{
    auto result = json::array();
    for( const auto &value : values ) { result.push_back( layer_json( value, layer_kind ) ); }
    return result;
}
auto to_json( const field_entry &value ) -> json
{
    auto result = json{{"look", layer_json( value.appearance, "field" )}, {"intensity", value.intensity}};
    if( value.hides_items ) { result["hides_items"] = true; }
    return result;
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
    if( value.terrain ) { result["terrain"] = layer_json( *value.terrain, "terrain" ); }
    if( value.furniture ) { result["furniture"] = layer_json( *value.furniture, "furniture" ); }
    if( !value.fields.empty() ) { result["fields"] = to_array( value.fields ); }
    if( value.reviving ) { result["reviving"] = true; }
    if( value.cargo ) { result["cargo"] = true; }
    if( value.roofed ) { result["roofed"] = true; }
    if( !value.traps.empty() ) { result["traps"] = layer_array( value.traps, "trap" ); }
    if( !value.items.empty() ) { result["items"] = layer_array( value.items, "item" ); }
    if( value.vehicle ) { result["vehicle"] = layer_json( *value.vehicle, "vehicle_part" ); }
    if( value.light ) { result["light"] = *value.light; }
    if( value.glow ) { result["glow"] = {{"color", value.glow->color}, {"strength", value.glow->strength}}; }
    if( value.below ) { result["below"] = *value.below; }
    if( value.memory ) {
        result["memory"] = {{"terrain", layer_json( value.memory->terrain, "terrain" )}};
        if( value.memory->overlay ) { result["memory"]["overlay"] = layer_json( *value.memory->overlay, "" ); }
    }
    return result;
}
auto to_json( const entity &value ) -> json
{
    auto result = json{{"id", value.id}, {"at", to_json( value.at )}, {"known", known_name( value.known )}};
    if( value.appearance ) { result["look"] = to_json( *value.appearance ); }
    if( value.name ) { result["name"] = *value.name; }
    if( !value.overlays.empty() ) { result["overlays"] = to_array( value.overlays ); }
    if( !value.traits.empty() ) { result["traits"] = value.traits; }
    if( value.attitude ) { result["attitude"] = *value.attitude; }
    if( value.aware ) { result["aware"] = true; }
    if( value.health ) { result["health"] = json{{"fifths", value.health->fifths}, {"color", value.health->color}}; }
    if( !value.statuses.empty() ) { result["statuses"] = value.statuses; }
    if( value.sense ) { result["sense"] = *value.sense; }
    return result;
}
auto to_json( const avatar_stat &value ) -> json
{
    auto result = json{{"id", value.id}, {"label", value.label}, {"value", value.value}};
    if( !value.color.empty() ) { result["color"] = value.color; }
    if( !value.short_label.empty() ) { result["short_label"] = value.short_label; }
    return result;
}
auto to_json( const inventory_entry &value ) -> json
{
    auto result = json{{"look", to_json( value.appearance )}, {"name", value.name}, {"label", value.label}};
    if( !value.id.empty() ) { result["id"] = value.id; }
    if( value.count ) { result["count"] = *value.count; }
    if( value.slot ) { result["slot"] = *value.slot; }
    if( value.durability ) {
        result["durability"] = json{{"symbol", value.durability->symbol}, {"color", value.durability->color}};
    }
    return result;
}
auto to_json( const mode_value &value ) -> json
{
    auto choices = json::array();
    for( const auto &choice : value.values ) { choices.push_back( {{"id", choice.id}, {"label", choice.label}} ); }
    return {{"id", value.id}, {"label", value.label}, {"value", value.value}, {"values", std::move( choices )}};
}
auto to_json( const sidebar_text &value ) -> json
{
    auto result = json{};
    if( !value.label.empty() ) { result["label"] = value.label; }
    result["text"] = value.text;
    if( !value.color.empty() ) { result["color"] = value.color; }
    return result;
}
auto to_json( const sidebar_limb &value ) -> json
{
    return {{"id", value.id}, {"label", to_json( value.label )}, {"hp", value.hp}, {"hp_max", value.hp_max},
        {"color", value.color}, {"broken", value.broken}};
}
auto to_json( const heading &value ) -> json
{
    return {{"id", value.id}, {"name", value.name}};
}
auto to_json( const sidebar_layout &value ) -> json
{
    auto panels = json::array();
    for( const auto &panel : value.panels ) {
        auto item = json{{"id", panel.id}, {"label", panel.label}, {"enabled", panel.enabled}};
        if( panel.height ) { item["height"] = *panel.height; }
        panels.push_back( std::move( item ) );
    }
    return {{"id", value.id}, {"side", value.side}, {"width", value.width}, {"panels", std::move( panels )}};
}
auto to_json( const compass_direction &value ) -> json
{
    auto creatures = json::array();
    for( const auto &creature : value.creatures ) {
        creatures.push_back( {{"id", creature.id}, {"look", to_json( creature.appearance )}, {"name", creature.name},
            {"attitude", creature.attitude}, {"distance", creature.distance}} );
    }
    auto result = json{{"toward", to_json( value.toward )}, {"creatures", std::move( creatures )}};
    if( !value.label.empty() ) { result["label"] = value.label; }
    return result;
}
auto to_json( const sidebar_value &value ) -> json
{
    auto result = json{{"layout", to_json( value.layout )}, {"limbs", to_array( value.limbs )}, {"pain", to_json( value.pain )},
        {"hunger", to_json( value.hunger )}, {"thirst", to_json( value.thirst )},
        {"fatigue", to_json( value.fatigue )}, {"focus", {{"label", value.focus_label}, {"value", value.focus}}},
        {"morale", {{"level", value.morale_level}, {"face", value.morale.text}, {"color", value.morale.color}}},
        {
            "stamina", {{"label", value.stamina_label}, {"current", value.stamina}, {"max", value.stamina_max},
                {"color", value.stamina_color}
            }
        },
        {"speed", {{"label", value.speed_label}, {"value", value.speed}, {"color", value.speed_color}}},
        {
            "movement", {{"counter", value.move_counter}, {"mode", value.move_mode}, {"mode_text", value.move_text},
                {"color", value.move_color}
            }
        },
        {"temperature", to_json( value.temperature )}, {"power", to_json( value.power )},
        {"safe_mode", {{"label", value.safe_label}, {"enabled", value.safe_mode}, {"color", value.safe_color}}},
        {"location", to_json( value.location )}, {"weather", to_json( value.weather )},
        {
            "time", {{"season", value.season}, {"day_label", value.day_label}, {"day", value.day},
                {"clock", value.clock}, {"has_watch", value.has_watch}
            }
        },
        {"weapon", to_json( value.weapon )}};
    if( value.ambient ) { result["ambient_temperature"] = to_json( *value.ambient ); }
    if( value.style ) { result["style"] = to_json( *value.style ); }
    return result;
}
auto to_json( const avatar_value &value ) -> json
{
    auto result = json{{"id", value.id}, {"at", to_json( value.at )}, {"name", value.name},
        {"stats", to_array( value.stats )}, {"sidebar", to_json( value.sidebar )}};
    if( value.appearance ) { result["look"] = to_json( *value.appearance ); }
    if( !value.overlays.empty() ) { result["overlays"] = to_array( value.overlays ); }
    if( !value.traits.empty() ) { result["traits"] = value.traits; }
    if( !value.inventory.empty() ) { result["inventory"] = to_array( value.inventory ); }
    if( !value.compass.empty() ) { result["compass"] = to_array( value.compass ); }
    if( !value.modes.empty() ) { result["modes"] = to_array( value.modes ); }
    if( !value.ground.empty() ) { result["ground"] = to_array( value.ground ); }
    if( value.activity ) {
        result["activity"] = {{"id", value.activity->id}, {"name", value.activity->name}};
        if( value.activity->done ) { result["activity"]["done"] = *value.activity->done; }
    }
    return result;
}
auto to_json( const environment_value &value ) -> json
{
    auto result = json{{"turn", value.turn}, {"time", value.time}, {"weather", value.weather}};
    if( !value.season.empty() ) { result["season"] = value.season; }
    result["sky"] = {{"daylight", value.sky.daylight}, {"precipitation", value.sky.precipitation},
        {"rain", value.sky.rain}, {"dimming", value.sky.dimming}, {"moon_phase", value.sky.moon_phase}
    };
    if( value.sky.sun ) { result["sky"]["sun"] = *value.sky.sun; }
    if( value.sky.moon ) { result["sky"]["moon"] = *value.sky.moon; }
    if( !value.mod_tilesets.empty() ) {
        result["mod_tilesets"] = json::array();
        for( const auto &file : value.mod_tilesets ) {
            result["mod_tilesets"].push_back( {{"path", file.path}, {"base", file.base}, {"index", file.index},
                {"compatibility", file.compatibility}} );
        }
    }
    return result;
}
auto to_json( const bounds &value ) -> json
{
    return {{"min", to_json( value.min )}, {"max", to_json( value.max )}};
}
auto to_json( const game_client::interaction_choice &choice ) -> json
{
    auto result = json{{"id", choice.id}, {"label", choice.label}, {"description", choice.description},
        {"denial", choice.denial}};
    if( !choice.hotkey.empty() ) { result["hotkey"] = choice.hotkey; }
    if( choice.pane_id ) { result["pane_id"] = *choice.pane_id; }
    if( choice.area_id ) { result["area_id"] = *choice.area_id; }
    if( !choice.storage_kind.empty() ) { result["storage_kind"] = choice.storage_kind; }
    result["enabled"] = choice.enabled;
    result["selectable"] = choice.selectable;
    result["selected"] = choice.selected;
    result["columns"] = json::array();
    for( const auto &column : choice.columns ) {
        auto entry = json{{"key", column.key}, {"label", column.label}, {"value", column.value}};
        if( !column.color.empty() ) { entry["color"] = column.color; }
        result["columns"].push_back( std::move( entry ) );
    }
    if( choice.favorite ) { result["favorite"] = true; }
    if( !choice.color.empty() ) { result["color"] = choice.color; }
    if( !choice.key_color.empty() ) { result["key_color"] = choice.key_color; }
    if( !choice.markup.empty() ) { result["markup"] = choice.markup; }
    if( !choice.action_id.empty() ) { result["action_id"] = choice.action_id; }
    if( choice.parent_id ) { result["parent_id"] = *choice.parent_id; }
    if( !choice.nest_color.empty() ) { result["nest_color"] = choice.nest_color; }
    if( choice.mark ) {
        result["mark"] = {{"glyph", choice.mark->glyph}};
        if( !choice.mark->color.empty() ) { result["mark"]["color"] = choice.mark->color; }
    }
    if( choice.editor ) {
        const auto &editor = *choice.editor;
        auto value = json{{"type", editor.type}, {"value", editor.value}, {"values", json::array()},
            {"max_length", editor.max_length}};
        for( const auto &entry : editor.values ) { value["values"].push_back( {{"id", entry.id}, {"label", entry.label}} ); }
        if( editor.minimum ) { value["minimum"] = *editor.minimum; }
        if( editor.maximum ) { value["maximum"] = *editor.maximum; }
        result["editor"] = std::move( value );
    }
    if( choice.selected_count ) { result["selected_count"] = *choice.selected_count; }
    if( choice.minimum_count ) { result["minimum_count"] = *choice.minimum_count; }
    if( choice.available_count ) { result["available_count"] = *choice.available_count; }
    return result;
}
auto target_json( const game_client::interaction_target &target, const bubble_frame &frame ) -> json
{
    auto value = json{{"source", to_json( target.source, frame )}, {"current", to_json( target.cursor, frame )}};
    if( target.minimum_position ) { value["minimum"] = to_json( *target.minimum_position, frame ); }
    if( target.maximum_position ) { value["maximum"] = to_json( *target.maximum_position, frame ); }
    value["range"] = target.range;
    value["distance_metric"] = target.distance_metric;
    value["distance"] = target.distance.value_or( game_client::target_distance( target,
                        target.cursor ) );
    value["status"] = target.status;
    if( !target.status_text.empty() ) { value["status_text"] = target.status_text; }
    value["candidates"] = json::array();
    for( const auto &candidate : target.candidates ) {
        value["candidates"].push_back( {{"id", candidate.id}, {"label", candidate.label},
            {"description", candidate.description}, {"creature", candidate.creature},
            {"position", to_json( candidate.position, frame )}} );
    }
    if( !target.trajectory.empty() ) {
        value["trajectory"] = json::array();
        for( const auto &position : target.trajectory ) { value["trajectory"].push_back( to_json( position, frame ) ); }
    }
    if( !target.parts.empty() ) {
        value["parts"] = json::array();
        for( const auto &part : target.parts ) { value["parts"].push_back( to_json( part ) ); }
    }
    if( !target.marks.empty() ) {
        value["marks"] = json::array();
        for( const auto &mark : target.marks ) {
            auto entry = json{{"role", mark.role}};
            if( mark.rect ) {
                entry["rect"] = {{"min", to_json( mark.rect->min, frame )}, {"max", to_json( mark.rect->max, frame )}};
            } else {
                entry["squares"] = json::array();
                for( const auto &square : mark.squares ) { entry["squares"].push_back( to_json( square, frame ) ); }
            }
            value["marks"].push_back( std::move( entry ) );
        }
    }
    if( frame.omt ) { value["unit"] = "omt"; }
    return value;
}
auto overmap_json( const game_client::interaction_overmap &om ) -> json
{
    // Overmap squares are absolute in every frame, even under a popup that has the bubble's.
    const auto frame = bubble_frame{ .omt = true };
    auto cells = json::array();
    for( const auto y : std::views::iota( 0, om.rows ) ) {
        auto row = json::array();
        for( const auto x : std::views::iota( 0, om.cols ) ) {
            const auto at = static_cast<std::size_t>( y * om.cols + x );
            row.push_back( {om.glyphs[at], om.foreground[at], om.background[at]} );
        }
        cells.push_back( std::move( row ) );
    }
    auto notes = json::array();
    for( const auto &note : om.notes ) {
        notes.push_back( {{"pos", to_json( note.position, frame )}, {"text", note.text}} );
    }
    auto result = json{{"origin", to_json( om.origin, frame )}, {"player", to_json( om.player, frame )},
        {"cols", om.cols}, {"rows", om.rows}, {"cells", std::move( cells )},
        {"legend", om.legend}, {"cursor_description", om.cursor_description}, {"notes", std::move( notes )}
    };
    auto path = json::array();
    for( const auto &step : om.path ) { path.push_back( to_json( step, frame ) ); }
    result["path"] = std::move( path );
    if( !om.cursor_here.empty() ) { result["cursor_here"] = om.cursor_here; }
    if( !om.cursor_note.empty() ) { result["cursor_note"] = om.cursor_note; }
    if( !om.route_text.empty() ) { result["route_text"] = om.route_text; }
    if( !om.notes_title.empty() ) { result["notes_title"] = om.notes_title; }
    if( !om.labels.empty() ) {
        auto labels = json::array();
        for( const auto &label : om.labels ) {
            labels.push_back( {{"pos", to_json( label.position, frame )}, {"text", label.text},
                {"foreground", label.foreground}, {"background", label.background}} );
        }
        result["labels"] = std::move( labels );
    }
    if( om.mission_target ) { result["mission_target"] = to_json( *om.mission_target, frame ); }
    if( !om.tiles.empty() ) {
        auto rows = json::array();
        for( const auto y : std::views::iota( 0, om.rows ) ) {
            auto row = json::array();
            for( const auto x : std::views::iota( 0, om.cols ) ) {
                const auto &tile = om.tiles[static_cast<std::size_t>( y * om.cols + x )];
                row.push_back( {tile.id, tile.rotation, tile.subtile} );
            }
            rows.push_back( std::move( row ) );
        }
        result["tiles"] = {{"cells", std::move( rows )}, {"looks_like", om.looks_like}};
    }
    return result;
}
auto canvas_json( const game_client::interaction_canvas &canvas ) -> json
{
    auto rows = json::array();
    for( const auto &row : canvas.rows ) {
        auto runs = json::array();
        for( const auto &run : row ) { runs.push_back( {run.text, run.foreground, run.background} ); }
        rows.push_back( std::move( runs ) );
    }
    return {{"cols", canvas.cols}, {"rows", std::move( rows )}};
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
    if( !native.message_tone.empty() ) { result["message_tone"] = native.message_tone; }
    if( native.category_mode ) { result["category_mode"] = *native.category_mode; }
    if( !native.any_key_choice_id.empty() ) { result["any_key_choice_id"] = native.any_key_choice_id; }
    if( native.field ) {
        const auto &field = *native.field;
        result["field"] = {{"id", field.id}, {"label", field.label}, {"description", field.description},
            {"value", field.value}, {"type", field.type}, {"max_length", field.max_length},
            {"printable", field.printable}
        };
    }
    if( native.target ) { result["target"] = target_json( *native.target, frame ); }
    if( native.refresh_ms > 0 ) { result["refresh_ms"] = native.refresh_ms; }
    if( !native.areas.empty() ) {
        result["areas"] = json::array();
        for( const auto &area : native.areas ) {
            result["areas"].push_back( {{"action_id", area.action_id}, {"label", area.label}, {"key", area.key},
                {"x", area.x}, {"y", area.y}, {"available", area.available}} );
        }
    }
    auto compat = json::object();
    if( !native.focus_choice_id.empty() ) {
        compat["focus"] = json::object();
        if( native.focus_pane_id ) { compat["focus"]["pane_id"] = *native.focus_pane_id; }
        compat["focus"]["choice_id"] = native.focus_choice_id;
    }
    compat["panes"] = json::array();
    for( const auto &pane : native.panes ) {
        auto value = json{{"id", pane.id}, {"label", pane.label}, {"role", pane.role},
            {"area_id", pane.area_id}, {"area_label", pane.area_label},
            {"area_description", pane.area_description}, {"filter", pane.filter},
            {"storage_kind", pane.storage_kind}};
        if( !pane.sort.empty() ) { value["sort"] = pane.sort; }
        if( pane.page ) { value["page"] = {{"number", pane.page->number}, {"count", pane.page->count}}; }
        if( pane.totals ) {
            const auto &t = *pane.totals;
            value["totals"] = {{"weight", t.weight}, {"weight_max", t.weight_max}, {"weight_unit", t.weight_unit},
                {"volume", t.volume}, {"volume_max", t.volume_max}, {"volume_unit", t.volume_unit},
                {"over_weight", t.over_weight}, {"over_volume", t.over_volume}
            };
        }
        if( !pane.blocked_areas.empty() ) { value["blocked_areas"] = pane.blocked_areas; }
        compat["panes"].push_back( std::move( value ) );
    }
    result["compat"] = std::move( compat );
    return result;
}
auto to_json( const readiness &value ) -> json
{
    return {{"phase", value.phase}, {"game_ready", value.game_ready},
        {"accepts_interaction_commands", value.accepts_interaction_commands},
        {"accepts_registered_actions", value.accepts_registered_actions}};
}
auto to_json( const boundary_state &state ) -> json
{
    auto result = json{{"boundary_id", state.id},
        {"readiness", to_json( state.ready )}};
    result["actions"] = json::array();
    for( const auto &entry : state.actions ) { result["actions"].push_back( {{"id", entry.id}, {"name", entry.name}, {"keys", entry.keys}} ); }
    result["interaction"] = interaction_json( state );
    return result;
}
auto to_json( const arg_shape &shape ) -> json
{
    auto result = json::object();
    if( shape.row ) { result["row"] = {{"component_id", shape.row->component_id}}; }
    if( shape.rows ) {
        result["rows"] = {{"component_id", shape.rows->component_id}};
        if( shape.rows->counts ) { result["rows"]["counts"] = true; }
    }
    if( shape.count ) {
        auto &count = result["count"] = json::object();
        if( shape.count->min ) { count["min"] = *shape.count->min; }
        if( shape.count->max ) { count["max"] = *shape.count->max; }
        if( shape.count->initial ) { count["default"] = *shape.count->initial; }
    }
    if( shape.text ) {
        result["text"] = json::object();
        if( !shape.text->field_id.empty() ) { result["text"]["field_id"] = shape.text->field_id; }
        if( !shape.text->initial.empty() ) { result["text"]["default"] = shape.text->initial; }
    }
    if( shape.pos ) {
        result["pos"] = {{"unit", shape.pos->unit}};
        if( !shape.pos->component_id.empty() ) { result["pos"]["component_id"] = shape.pos->component_id; }
    }
    if( shape.pane ) { result["pane"] = {{"component_id", shape.pane->component_id}}; }
    return result;
}
auto to_json( const affordance &value ) -> json
{
    auto result = json{{"id", value.id}, {"label", value.label}, {"keys", value.keys}, {"enabled", value.enabled}};
    if( !value.description.empty() ) { result["description"] = value.description; }
    if( !value.denial.empty() ) { result["denial"] = value.denial; }
    if( !value.role.empty() ) { result["role"] = value.role; }
    if( auto args = to_json( value.args ); !args.empty() ) { result["args"] = std::move( args ); }
    result["expect"] = scope_name( scope_of( value ) );
    if( value.holdable ) { result["holdable"] = true; }
    if( !value.group.empty() ) { result["group"] = value.group; }
    return result;
}
/// A 0.1 choice without the members components replace.
auto to_json( const row &value ) -> json
{
    auto result = json{{"id", value.id}, {"label", value.label}, {"description", value.description},
        {"denial", value.denial}};
    if( !value.hotkey.empty() ) { result["hotkey"] = value.hotkey; }
    result["enabled"] = value.enabled;
    result["selectable"] = value.selectable;
    result["selected"] = value.selected;
    result["columns"] = json::array();
    for( const auto &column : value.columns ) {
        auto entry = json{{"key", column.key}, {"label", column.label}, {"value", column.value}};
        if( !column.color.empty() ) { entry["color"] = column.color; }
        result["columns"].push_back( std::move( entry ) );
    }
    if( value.favorite ) { result["favorite"] = true; }
    if( !value.color.empty() ) { result["color"] = value.color; }
    if( !value.key_color.empty() ) { result["key_color"] = value.key_color; }
    if( !value.markup.empty() ) { result["markup"] = value.markup; }
    if( value.parent_id ) { result["parent_id"] = *value.parent_id; }
    if( !value.nest_color.empty() ) { result["nest_color"] = value.nest_color; }
    if( value.mark ) {
        result["mark"] = {{"glyph", value.mark->glyph}};
        if( !value.mark->color.empty() ) { result["mark"]["color"] = value.mark->color; }
    }
    if( value.selected_count ) { result["selected_count"] = *value.selected_count; }
    if( value.minimum_count ) { result["minimum_count"] = *value.minimum_count; }
    // Counts are in the row's native units, up to what it holds.
    if( value.max ) { result["max"] = *value.max; }
    if( value.header ) { result["header"] = true; }
    if( value.toggled ) { result["toggled"] = *value.toggled; }
    if( value.look ) { result["look"] = to_json( *value.look ); }
    if( !value.affordance.empty() ) { result["affordance"] = value.affordance; }
    return result;
}
/// The rows that go on the wire: a prefix of `rows` of at most `maximum_rows` and `maximum_list_bytes`, whose first row always goes.
auto rows_json( const std::vector<row> &rows ) -> json
{
    auto result = json::array();
    auto bytes = std::size_t{ 0 };
    for( const auto &entry : rows | std::views::take( maximum_rows ) ) {
        auto value = to_json( entry );
        bytes += value.dump().size() + 1;
        if( !result.empty() && bytes > maximum_list_bytes ) { break; }
        result.push_back( std::move( value ) );
    }
    return result;
}
auto to_json( const component &value, const bubble_frame &frame ) -> json;
auto body_json( const list_body &body, const bubble_frame & /*frame*/ ) -> json
{
    auto columns = json::array();
    for( const auto &column : body.columns ) { columns.push_back( {{"key", column.key}, {"label", column.label}} ); }
    auto result = json{{"columns", std::move( columns )}, {"rows", rows_json( body.rows )}, {"total", body.total}};
    if( body.offset != 0 ) { result["offset"] = body.offset; }
    result["selection"] = body.selection;
    if( !body.activate.empty() ) { result["activate"] = body.activate; }
    if( body.filterable ) { result["filterable"] = true; }
    if( !body.filter.empty() ) { result["filter"] = body.filter; }
    if( !body.empty.empty() ) { result["empty"] = body.empty; }
    if( body.rows_shown != 0 ) { result["rows_shown"] = body.rows_shown; }
    return result;
}
auto body_json( const tabs_body &body, const bubble_frame & /*frame*/ ) -> json
{
    auto tabs = json::array();
    for( const auto &tab : body.tabs ) {
        auto entry = json{{"id", tab.id}, {"label", tab.label}, {"selected", tab.selected}, {"enabled", tab.enabled}};
        if( !tab.denial.empty() ) { entry["denial"] = tab.denial; }
        if( tab.unread ) { entry["unread"] = true; }
        if( !tab.hotkey.empty() ) { entry["hotkey"] = tab.hotkey; }
        tabs.push_back( std::move( entry ) );
    }
    auto result = json{{"tabs", std::move( tabs )}};
    if( !body.select.empty() ) { result["select"] = body.select; }
    return result;
}
auto body_json( const split_body &body, const bubble_frame & /*frame*/ ) -> json
{
    auto result = json{{"relation", body.relation}};
    if( !body.active.empty() ) { result["active"] = body.active; }
    return result;
}
auto body_json( const panel_body &body, const bubble_frame & /*frame*/ ) -> json
{
    auto result = json{{"title", body.title}};
    if( !body.subtitle.empty() ) { result["subtitle"] = body.subtitle; }
    if( !body.stats.empty() ) {
        result["stats"] = json::array();
        for( const auto &stat : body.stats ) {
            auto entry = json{{"key", stat.key}, {"label", stat.label}, {"value", stat.value}};
            if( !stat.max.empty() ) { entry["max"] = stat.max; }
            if( !stat.unit.empty() ) { entry["unit"] = stat.unit; }
            if( !stat.tone.empty() ) { entry["tone"] = stat.tone; }
            result["stats"].push_back( std::move( entry ) );
        }
    }
    if( !body.filter.empty() ) { result["filter"] = body.filter; }
    return result;
}
auto body_json( const form_body &body, const bubble_frame & /*frame*/ ) -> json
{
    auto fields = json::array();
    for( const auto &field : body.fields ) {
        auto entry = json{{"id", field.id}, {"label", field.label}};
        if( !field.description.empty() ) { entry["description"] = field.description; }
        entry["value"] = field.value;
        entry["type"] = field.type;
        if( field.max_length != -1 ) { entry["max_length"] = field.max_length; }
        if( field.printable ) { entry["printable"] = true; }
        if( field.multiline ) { entry["multiline"] = true; }
        if( field.required ) { entry["required"] = true; }
        if( field.cursor ) { entry["cursor"] = *field.cursor; }
        if( !field.options.empty() ) {
            entry["options"] = json::array();
            for( const auto &option : field.options ) { entry["options"].push_back( {{"id", option.id}, {"label", option.label}} ); }
        }
        // A whole-number field's bounds are whole numbers.
        const auto bound = [&]( const double value ) {
            return field.type == "integer" ? json( std::llround( value ) ) : json( value );
        };
        if( field.min ) { entry["min"] = bound( *field.min ); }
        if( field.max ) { entry["max"] = bound( *field.max ); }
        fields.push_back( std::move( entry ) );
    }
    auto result = json{{"fields", std::move( fields )}};
    if( !body.submit.empty() ) { result["submit"] = body.submit; }
    return result;
}
auto body_json( const text_body &body, const bubble_frame & /*frame*/ ) -> json
{
    auto result = json{{"markup", body.markup}};
    if( !body.tone.empty() ) { result["tone"] = body.tone; }
    if( !body.role.empty() ) { result["role"] = body.role; }
    return result;
}
auto body_json( const grid_body &body, const bubble_frame & /*frame*/ ) -> json
{
    auto result = json{{"cols", body.cols}, {"rows", body.rows}};
    if( body.cursor ) { result["cursor"] = {{"x", body.cursor->x}, {"y", body.cursor->y}}; }
    // Each reason once, as the cells name it by its index: a canvas full of empty squares
    // refuses most of them for the same reason.
    auto refusals = std::vector<std::string> {};
    const auto look_of = [&]( const grid_cell & cell ) {
        auto entry = json::object();
        for( const auto &[name, text] : {
                 std::pair{"glyph", &cell.glyph}, std::pair{"label", &cell.label},
                 std::pair{"color", &cell.color}, std::pair{"affordance", &cell.affordance}
             } ) {
            if( !text->empty() ) { entry[name] = *text; }
        }
        entry["enabled"] = cell.enabled;
        if( !cell.denial.empty() ) { entry["denial"] = cell.denial; }
        if( cell.selected ) { entry["selected"] = true; }
        if( !cell.looks.empty() ) { entry["looks"] = cell.looks; }
        if( cell.faded ) { entry["faded"] = true; }
        if( !cell.refused.empty() ) {
            const auto found = std::ranges::find( refusals, cell.refused );
            entry["refused"] = std::distance( refusals.begin(), found );
            if( found == refusals.end() ) { refusals.push_back( cell.refused ); }
        }
        return entry;
    };
    if( !body.blank ) {
        result["cells"] = json::array();
        for( const auto &cell : body.cells ) {
            auto entry = json{{"x", cell.at.x}, {"y", cell.at.y}};
            if( !cell.id.empty() ) { entry["id"] = cell.id; }
            entry.update( look_of( cell ) );
            result["cells"].push_back( std::move( entry ) );
        }
    } else {
        // A grid of squares named by place sends each look once, as a style, the blank one
        // first and the others as they first show from the north-west, and each row's squares
        // of one style as a run east; the squares that show the blank cell are left to it.
        auto styles = json::array();
        // Each style's index by its text, so a canvas of many looks stays quick.
        auto indexes = std::unordered_map<std::string, std::size_t> {};
        const auto style_of = [&]( const grid_cell & cell ) {
            auto look = look_of( cell );
            const auto [found, added] = indexes.try_emplace( look.dump(), styles.size() );
            if( added ) { styles.push_back( std::move( look ) ); }
            return found->second;
        };
        result["blank"] = style_of( *body.blank );
        auto shown = body.cells | std::views::filter( [&]( const grid_cell & entry ) { return !blank_square( body, entry ); } )
        | std::views::transform( []( const grid_cell & entry ) { return &entry; } )
        | std::ranges::to<std::vector>();
        std::ranges::sort( shown, []( const grid_cell * a, const grid_cell * b ) {
            return std::tie( a->at.y, a->at.x ) < std::tie( b->at.y, b->at.x );
        } );
        auto cells = json::array();
        for( const auto *const cell : shown ) {
            const auto style = style_of( *cell );
            if( !cells.empty() ) {
                auto &last = cells.back();
                const auto run = last.value( "n", 1 );
                if( last["y"] == cell->at.y && last["style"] == style &&
                    last["x"].get<int>() + run == cell->at.x ) {
                    last["n"] = run + 1;
                    continue;
                }
            }
            cells.push_back( {{"x", cell->at.x}, {"y", cell->at.y}, {"style", style}} );
        }
        result["styles"] = std::move( styles );
        result["cells"] = std::move( cells );
    }
    if( !refusals.empty() ) { result["refusals"] = refusals; }
    if( !body.looks.empty() ) { result["looks"] = to_array( body.looks ); }
    if( !body.role.empty() ) { result["role"] = body.role; }
    if( !body.drag.empty() ) { result["drag"] = body.drag; }
    if( !body.drag_begin.empty() ) { result["drag_begin"] = body.drag_begin; }
    if( !body.drag_end.empty() ) { result["drag_end"] = body.drag_end; }
    if( body.block.x > 0 && body.block.y > 0 ) { result["block"] = {{"x", body.block.x}, {"y", body.block.y}}; }
    if( !body.regions.empty() ) {
        result["regions"] = json::array();
        for( const auto &region : body.regions ) {
            auto entry = json{{"id", region.id}, {"from", {{"x", region.from.x}, {"y", region.from.y}}},
                {"to", {{"x", region.to.x}, {"y", region.to.y}}}};
            if( !region.label.empty() ) { entry["label"] = region.label; }
            if( region.selected ) { entry["selected"] = true; }
            result["regions"].push_back( std::move( entry ) );
        }
    }
    if( !body.region_drag.empty() ) { result["region_drag"] = body.region_drag; }
    if( !body.secondary.empty() ) { result["secondary"] = body.secondary; }
    if( !body.secondary_drag.empty() ) { result["secondary_drag"] = body.secondary_drag; }
    if( !body.stamp.empty() ) {
        result["stamp"] = json::array();
        for( const auto &offset : body.stamp ) {
            result["stamp"].push_back( {{"x", offset.x}, {"y", offset.y}} );
        }
    }
    if( body.span ) {
        const auto &span = *body.span;
        auto entry = json{{"shape", span.shape}, {"size", span.size},
            {"from", {{"x", span.from.x}, {"y", span.from.y}}}, {"to", {{"x", span.to.x}, {"y", span.to.y}}},
            {"looks", span.looks}};
        if( span.mirror_x ) { entry["mirror_x"] = true; }
        if( span.mirror_y ) { entry["mirror_y"] = true; }
        if( !span.fill.empty() ) { entry["fill"] = span.fill; }
        result["span"] = std::move( entry );
    }
    return result;
}
auto body_json( const map_body &body, const bubble_frame &frame ) -> json
{
    auto result = json{{"mode", body.mode}};
    if( body.target ) {
        result["target"] = target_json( *body.target, body.target->coordinate_space == "omt" ?
                                        bubble_frame{ .omt = true } : frame );
    }
    if( body.overmap ) { result["overmap"] = overmap_json( *body.overmap ); }
    for( const auto &list : body.item_lists ) {
        result["item_lists"].push_back( {{"id", list.id}, {"label", list.label}, {"verb", list.verb}} );
    }
    return result;
}
auto body_json( const game_client::interaction_canvas &body,
                const bubble_frame & /*frame*/ ) -> json
{
    return canvas_json( body );
}
/// Kind-specific members come after the common ones, the children last.
auto to_json( const component &value, const bubble_frame &frame ) -> json
{
    auto result = json{{"id", value.id}, {"kind", component_kind( value )}};
    if( !value.label.empty() ) { result["label"] = value.label; }
    if( !value.affordances.empty() ) { result["affordances"] = value.affordances; }
    if( !value.slot.empty() ) { result["slot"] = value.slot; }
    if( value.collapsed ) { result["collapsed"] = true; }
    result.update( std::visit( [&]( const auto & body ) { return body_json( body, frame ); },
    value.body ) );
    const auto kind = component_kind( value );
    if( kind == "split" ) {
        result["children"] = json::array();
        for( const auto &child : value.children ) { result["children"].push_back( to_json( child, frame ) ); }
    } else if( !value.children.empty() ) {
        result["child"] = to_json( value.children.front(), frame );
    }
    return result;
}
auto to_json( const screen_view &view, const bubble_frame &frame ) -> json
{
    auto result = json{{"screen_id", view.screen_id}, {"role", view.role}};
    if( !view.kind.empty() ) { result["kind"] = view.kind; }
    if( !view.title.empty() ) { result["title"] = view.title; }
    result["root"] = view.root ? to_json( *view.root, frame ) : json( nullptr );
    result["affordances"] = json::array();
    for( const auto &entry : view.affordances ) { result["affordances"].push_back( to_json( entry ) ); }
    if( !view.focus_component.empty() || !view.focus_row.empty() ) {
        auto &focus = result["focus"] = json::object();
        if( !view.focus_component.empty() ) { focus["component_id"] = view.focus_component; }
        // A grid that names its squares by place names the focused cell so too.
        const auto *const part = view.root && !view.focus_component.empty() ?
                                 find_component( *view.root, view.focus_component ) : nullptr;
        const auto *const grid = part ? std::get_if<grid_body>( &part->body ) : nullptr;
        const auto *const cell = grid && grid->blank ? cell_of( *grid, view.focus_row ) : nullptr;
        if( !view.focus_row.empty() ) { focus["row_id"] = cell ? square_id( cell->at ) : view.focus_row; }
    }
    if( view.refresh_ms > 0 ) { result["refresh_ms"] = view.refresh_ms; }
    if( !view.dock.empty() ) { result["dock"] = view.dock; }
    if( !view.shortcuts.empty() ) {
        result["shortcuts"] = json::array();
        for( const auto &entry : view.shortcuts ) {
            auto item = json{{"affordance_id", entry.affordance_id}, {"label", entry.label}, {"keys", entry.keys},
                {"icon", entry.icon}, {"enabled", entry.enabled}};
            if( !entry.glyph.empty() ) { item["glyph"] = entry.glyph; }
            result["shortcuts"].push_back( std::move( item ) );
        }
    }
    if( !view.context.empty() ) { result["context"] = view.context; }
    return result;
}
/// The boundary with the `stack` of its screens.
auto to_stacked_json( const boundary_state &state ) -> json
{
    auto result = to_json( state );
    // What was captured with the boundary; a state that was not captured reads the live screens.
    const auto live = state.stack ? std::vector<screen_view> {} :
                      interaction_stack( state );
    result["stack"] = json::array();
    for( const auto &screen : state.stack ? *state.stack : live ) {
        result["stack"].push_back( to_json( screen, state.frame ) );
    }
    return result;
}
auto to_json( const changes &delta ) -> json
{
    auto result = json::object();
    if( delta.coverage ) { result["coverage"] = to_json( *delta.coverage ); }
    if( delta.view ) { result["view"] = to_json( *delta.view ); }
    if( !delta.cells.empty() ) { result["cells"] = to_array( delta.cells ); }
    if( !delta.forgotten.empty() ) { result["forgotten"] = to_array( delta.forgotten ); }
    if( !delta.entities.empty() ) { result["entities"] = to_array( delta.entities ); }
    if( !delta.gone.empty() ) {
        result["gone"] = json::array();
        for( const auto &value : delta.gone ) { result["gone"].push_back( {{"id", value.id}, {"reason", value.reason}} ); }
    }
    if( delta.avatar ) {
        result["avatar"] = to_json( *delta.avatar );
        const auto &kept = delta.avatar_kept;
        if( !kept.empty() ) {
            std::ranges::for_each( kept, [&]( const auto & member ) {
                result["avatar"].erase( member );
            } );
            result["avatar"]["keep"] = kept;
        }
    }
    if( delta.environment ) { result["environment"] = to_json( *delta.environment ); }
    if( delta.route ) {
        result["route"] = to_array( *delta.route );
        if( !delta.route->empty() ) { result["route_text"] = route_text( *delta.route ); }
    }
    if( delta.interaction ) {
        result["interaction"] = to_stacked_json( *delta.interaction );
        if( delta.actions_unchanged ) { result["interaction"].erase( "actions" ); }
    }
    return result;
}
auto to_json( const localised_text &value ) -> json
{
    auto result = json{{"key", value.key}};
    if( value.ctx ) { result["ctx"] = *value.ctx; }
    if( value.plural ) { result["plural"] = *value.plural; }
    if( value.plural_key ) { result["plural_key"] = *value.plural_key; }
    const auto arg_json = []( const localised_text::arg & arg ) {
        return std::visit( []( const auto & held ) {
            if constexpr( std::is_same_v<std::remove_cvref_t<decltype( held )>, std::vector<localised_text>> ) {
                return to_json( held.front() );
            } else {
                return json( held );
            }
        }, arg );
    };
    if( !value.args.empty() ) {
        result["args"] = value.args | std::views::transform( arg_json ) | std::ranges::to<std::vector>();
    }
    if( value.npcname ) { result["npcname"] = arg_json( *value.npcname ); }
    return result;
}
/// A converted line is a LocalisedString; its repeat suffix is the native `%s x %d` message around it.
/// `lang` only for a line that is not in the locale of the connection reading it.
auto to_json( const message_value &line ) -> json
{
    auto result = json{{"id", std::to_string( line.id )}, {"text", line.text}, {"kind", line.kind},
        {"color", line.color}, {"count", line.count}, {"turn", std::to_string( line.turn )},
        {"label", line.label}};
    if( line.localised ) {
        const auto text = to_json( *line.localised );
        result["text"] = text;
result["label"] = line.count > 1 ? json{{"key", "%s x %d"}, {"args", {text, line.count}}} :
        text;
    } else if( !line.lang.empty() ) {
        result["lang"] = line.lang;
    }
    return result;
}
auto to_json( const event_value &value ) -> json
{
    auto result = json{{"sequence", std::to_string( value.sequence )}, {"revision", std::to_string( value.revision )},
        {"type", value.type}};
    if( value.cause ) { result["cause"] = std::to_string( *value.cause ); }
    if( value.command ) { result["command"] = *value.command; }
    result["changes"] = to_json( value.delta );
    if( value.message ) {
        result["data"] = to_json( *value.message );
    }
    if( value.presentation ) {
        const auto &fact = *value.presentation;
        auto data = json::object();
        if( value.type == "projectile.moved" ) {
            data = {{"projectile", fact.id}, {"path", to_array( fact.cells )}};
            if( fact.appearance ) { data["look"] = to_json( *fact.appearance ); }
        } else if( value.type == "explosion.started" ) {
            data = {{"explosion", fact.id}};
            if( fact.at ) { data["at"] = to_json( *fact.at ); }
            if( fact.radius ) { data["radius"] = *fact.radius; }
            if( !fact.color.empty() ) { data["color"] = fact.color; }
            if( !fact.tile.empty() ) { data["tile"] = fact.tile; }
        } else if( value.type == "explosion.blast" || value.type == "explosion.shrapnel" ) {
            data = {{"explosion", fact.id}, {"cells", to_array( fact.cells )}};
            if( !fact.colors.empty() ) { data["colors"] = fact.colors; }
        } else if( value.type == "explosion.ended" ) {
            data = {{"explosion", fact.id}};
        } else if( value.type == "item.activated" ) {
            data = {{"at", to_json( *fact.at )}, {"look", to_json( *fact.appearance )}};
            if( fact.turns ) { data["turns"] = *fact.turns; }
        } else if( value.type == "item.thrown" ) {
            data = {{"projectile", fact.id}, {"look", to_json( *fact.appearance )}, {"by", fact.entity},
                {"from", to_json( *fact.origin )}, {"to", to_json( *fact.target )}
            };
        } else if( value.type == "item.landed" ) {
            data = {{"at", to_json( *fact.at )}, {"look", to_json( *fact.appearance )}};
            if( !fact.id.empty() ) { data["projectile"] = fact.id; }
        } else if( value.type == "entity.moved" ) {
            data = {{"id", fact.entity}, {"to", to_json( *fact.at )}, {"mode", fact.kind}};
            if( fact.origin ) { data["from"] = to_json( *fact.origin ); }
        } else if( value.type == "entity.gone" ) {
            data = {{"id", fact.entity}, {"reason", fact.kind}};
        } else if( value.type == "sound.heard" ) {
            data = {{"text", fact.text}, {"volume", *fact.amount}, {"logged", fact.logged}};
            if( fact.distance ) { data["distance"] = *fact.distance; }
            if( fact.from ) { data["direction"] = to_json( *fact.from ); }
            if( fact.at ) { data["marker"] = to_json( *fact.at ); }
        } else if( value.type == "projectile.hit" ) {
            data = {{"projectile", fact.id}, {"at", to_json( *fact.at )}};
            if( !fact.entity.empty() ) { data["entity"] = fact.entity; }
        } else if( value.type == "entity.damaged" ) {
            data = {{"id", fact.entity}, {"amount", *fact.amount}};
            if( !fact.part.empty() ) { data["part"] = fact.part; }
        } else if( value.type == "terrain.changed" || value.type == "furniture.changed" ) {
            data = {{"at", to_json( *fact.at )}, {"from", to_json( *fact.previous )}, {"to", to_json( *fact.appearance )}};
        } else if( value.type == "field.changed" ) {
            data = {{"at", to_json( *fact.at )}, {"field", fact.field}, {"intensity", *fact.amount}};
        } else if( value.type == "combat_text.shown" ) {
            data = {{"at", to_json( *fact.at )}, {"segments", json::array()}};
            for( const auto &segment : fact.segments ) {
                data["segments"].push_back( {{"text", segment.text}, {"color", segment.color}} );
            }
            if( fact.scroll ) { data["scroll"] = {{"x", fact.scroll->x}, {"y", fact.scroll->y}}; }
            if( !fact.kind.empty() ) { data["kind"] = fact.kind; }
        } else if( value.type == "sound.played" ) {
            data = {{"id", fact.sound->id}, {"variant", fact.sound->variant}, {"volume", fact.sound->volume},
                {"stacks", fact.sound->stacks}
            };
            if( fact.sound->angle ) { data["angle"] = *fact.sound->angle; }
            if( fact.sound->wait_ms ) { data["wait_ms"] = *fact.sound->wait_ms; }
        } else if( value.type == "sound.channel" ) {
            data = {{"channel", fact.sound->channel}, {"fade_ms", fact.sound->fade_ms}};
            if( !fact.sound->id.empty() ) {
                data.update( {{"id", fact.sound->id}, {"variant", fact.sound->variant}, {"volume", fact.sound->volume},
                    {"loops", fact.sound->loops}} );
                if( fact.sound->angle ) { data["angle"] = *fact.sound->angle; }
                if( fact.sound->gain ) { data["gain"] = *fact.sound->gain; }
            }
        } else if( value.type == "music.changed" ) {
            if( !fact.sound->playlist.empty() ) {
                data = {{"playlist", fact.sound->playlist}, {"volume", fact.sound->volume}};
            }
        }
        if( !fact.group.empty() ) { data["group"] = fact.group; }
        result["data"] = std::move( data );
        if( fact.duration_ms != 0 ) { result["display"] = {{"duration_ms", fact.duration_ms}}; }
    }
    return result;
}
} // namespace

auto serialize_clock( const clock_point &at ) -> std::string { return to_json( at ).dump(); }
auto serialize_boundary( const boundary_state &state ) -> std::string { return to_json( state ).dump(); }
auto serialize_interaction_boundary( const boundary_state &state ) -> std::string
{
    return to_stacked_json( state ).dump();
}
auto serialize_choices( const choices_page &page ) -> std::string
{
    return json{{"boundary_id", page.boundary_id}, {"total", page.total}, {"choices", to_array( page.choices )}}.dump();
}
auto serialize_overmap( const game_client::interaction_overmap &squares ) -> std::string
{
    return overmap_json( squares ).dump();
}
namespace
{
/// Every row of each list under `value` into `result`, its JSON: a panel has no pages to fetch the rest from.
auto all_rows( const component &value, json &result ) -> void
{
    if( const auto *const list = std::get_if<list_body>( &value.body ) ) {
        result["rows"] = json::array();
        for( const auto &entry : list->rows ) { result["rows"].push_back( to_json( entry ) ); }
    }
    if( result.contains( "children" ) ) {
        for( const auto &[child, wire] : std::views::zip( value.children, result["children"] ) ) { all_rows( child, wire ); }
    } else if( result.contains( "child" ) && !value.children.empty() ) {
        all_rows( value.children.front(), result["child"] );
    }
}
} // namespace

// A panel shows no map, so it needs no frame of one. Its lists go whole while the frame allows, else a page of each.
auto serialize_screen_view( const screen_view &view ) -> std::string
{
    auto result = to_json( view, bubble_frame{} );
    const auto paged = result.dump();
    if( view.root ) { all_rows( *view.root, result["root"] ); }
    auto whole = result.dump();
    return whole.size() <= maximum_inline_bytes ? whole : paged;
}
auto serialize_rows( const rows_page &page ) -> std::string
{
    return json{{"boundary_id", page.boundary_id}, {"component_id", page.component_id},
        {"total", page.total}, {"rows", rows_json( page.rows )}}.dump();
}
auto serialize_command_result( const command_result &value ) -> std::string
{
    auto result = json{{"epoch", value.epoch}, {"command_id", value.command_id}, {"stage", stage_name( value.stage )},
        {"stage_text", stage_text( value.stage )}};
    if( value.failure ) {
        result["error"] = error_name( *value.failure );
        result["error_text"] = value.message.empty() ? error_text( *value.failure ) : value.message;
    }
    if( value.at ) { result["at"] = to_json( *value.at ); }
    return result.dump();
}
auto serialize_cells( const cells_page &page ) -> std::string
{
    return json{{"at", to_json( page.at )}, {"cells", to_array( page.cells )},
        {"forgotten", to_array( page.forgotten )}}.dump();
}
auto serialize_events( const event_batch &batch ) -> std::string
{
    // A sealed event goes in as the stream serialized it; one that no stream sealed, or whose boundary has no
    // captured stack and so shows the screens open now, is serialized now.
    auto result = R"({"epoch":)" + json( batch.epoch ).dump() + R"(,"events":[)";
    for( const auto &event : batch.events ) {
        if( &event != &batch.events.front() ) { result += ','; }
        const auto &interaction = event.value().delta.interaction;
        const auto reads_live = interaction && !interaction->stack;
        if( const auto sealed = event.element(); !sealed.empty() && !reads_live ) { result += sealed; }
        else { result += to_json( event.value() ).dump(); }
    }
    return result + "]}";
}
auto serialize_snapshot( const snapshot &value ) -> std::expected<snapshot_wire, error>
{
    const auto &world = value.value.world;
    // Room for the part envelope: epoch, clock, index and flags.
    const auto budget = maximum_inline_bytes - 1024 - 2 * value.at.epoch.size();
    auto groups = std::vector<json> {};
    auto bytes = std::size_t{0};
    for( const auto &entry : world.cells ) {
        auto cell = to_json( entry.second );
        const auto size = cell.dump().size() + 1;
        if( size > budget ) { return std::unexpected( error::resource_limit ); }
        if( groups.empty() || bytes + size > budget || groups.back().size() == cells_per_part ) {
            groups.push_back( json::array() );
            bytes = 0;
        }
        groups.back().push_back( std::move( cell ) );
        bytes += size;
    }
    auto result = snapshot_wire{};
    for( const auto index : std::views::iota( std::size_t{0}, groups.size() ) ) {
        result.parts.push_back( json{{"epoch", value.at.epoch}, {"at", to_json( value.at )}, {"index", index},
            {"last", index + 1 == groups.size()}, {"cells", std::move( groups[index] )}}.dump() );
    }
    auto header = json{{"at", to_json( value.at )}};
    if( world.coverage ) { header["coverage"] = to_json( *world.coverage ); }
    if( world.view ) { header["view"] = to_json( *world.view ); }
    header["interaction"] = to_stacked_json( value.value.interaction );
    if( world.avatar ) { header["avatar"] = to_json( *world.avatar ); }
    if( world.environment ) { header["environment"] = to_json( *world.environment ); }
    if( !world.route.empty() ) {
        header["route"] = to_array( world.route );
        header["route_text"] = route_text( world.route );
    }
    header["entities"] = json::array();
    for( const auto &entry : world.entities ) { header["entities"].push_back( to_json( entry.second ) ); }
    if( !value.log.empty() ) {
        header["messages"] = json::array();
        for( const auto &line : value.log ) { header["messages"].push_back( to_json( line ) ); }
    }
    if( !value.catalogues.empty() ) { header["catalogues"] = value.catalogues; }
    header["parts"] = result.parts.size();
    result.header = header.dump();
    if( result.header.size() > maximum_inline_bytes ) { return std::unexpected( error::resource_limit ); }
    return result;
}
auto serialize_loading( const std::string &epoch, const game_client::loading_progress &progress )
-> std::string
{
    if( progress.done ) { return json{{"epoch", epoch}, {"done", true}}.dump(); }
    auto value = json{{"epoch", epoch}, {"title", progress.title}, {"entries", progress.entries},
        {"index", progress.index}};
    if( progress.image ) {
        value["image"] = json{{"path", progress.image->path}};
        if( progress.image->author ) { value["image"]["author"] = *progress.image->author; }
    }
    return value.dump();
}
auto serialize_resync( const std::string &epoch, const std::string_view reason,
                       const counter lost_after )
-> std::string
{
    return json{{"epoch", epoch}, {"reason", std::string{reason}}, {"lost_after", std::to_string( lost_after )}}.dump();
}
} // namespace engine_client
