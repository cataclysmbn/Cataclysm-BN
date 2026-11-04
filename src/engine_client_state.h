#pragma once

#include "engine_client_mode.h"

#include <compare>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace engine_client
{

/// Absolute map square. `dim` is the game dimension_id; "" is the primary dimension.
struct position {
    std::string dim = {};
    int x = 0;
    int y = 0;
    int z = 0;
    auto operator<=>( const position & ) const = default; // *NOPAD*
};
struct bounds {
    position min;
    position max;
    auto operator<=>( const bounds & ) const = default; // *NOPAD*
};
/// The paint of a vehicle part as `#rrggbb`; an empty member is unpainted.
struct look_tint {
    std::string bg = {};
    std::string fg = {};
    auto operator<=>( const look_tint & ) const = default; // *NOPAD*
};
/// What a tileset needs to tint the sprite of a mutation: its id, and its types and flags, which
/// its `tint_pairs` are keyed by.
struct look_mutation {
    std::string id = {};
    std::vector<std::string> types = {};
    std::vector<std::string> flags = {};
    auto operator<=>( const look_mutation & ) const = default; // *NOPAD*
};
/// What an item's own variables set of the tint of one layer of its sprite, over the tileset's.
struct look_tint_layer {
    /// `#rrggbb`.
    std::string color = {};
    std::string blend_mode = {};
    std::optional<float> contrast = std::nullopt;
    std::optional<float> saturation = std::nullopt;
    std::optional<float> brightness = std::nullopt;
    auto empty() const -> bool {
        return color.empty() && blend_mode.empty() && !contrast && !saturation && !brightness;
    }
    auto operator<=>( const look_tint_layer & ) const = default; // *NOPAD*
};
struct look_tint_vars {
    look_tint_layer bg;
    look_tint_layer fg;
    auto operator<=>( const look_tint_vars & ) const = default; // *NOPAD*
};
/// Appearance from game data, so text clients need no tileset. The optional members give a tiled
/// client what only the engine can know: the sprite id when it is not `id`, the data's fallback
/// ids, and the shape the native tile selection computed from the neighbours.
struct look {
    std::string kind = {};
    std::optional<std::string> id = std::nullopt;
    /// Player-facing name as the native look shows it ("floor" for t_floor); absent when the data has none.
    std::optional<std::string> name = std::nullopt;
    std::string glyph = {};
    std::string color = {};
    /// Tile id native tilesets look up when it differs from `id` (corpses, characters).
    std::optional<std::string> tile = std::nullopt;
    /// The `looks_like` chain of the game data, nearest first.
    std::vector<std::string> looks_like = {};
    /// Multitile key of the native selection: center, corner, edge, t_connection, end_piece, unconnected, open, broken.
    std::optional<std::string> subtile = std::nullopt;
    /// Quarter turns of the native selection, 0 north, 1 west, 2 south, 3 east.
    std::optional<int> rotation = std::nullopt;
    /// Side a creature faces: left or right.
    std::optional<std::string> facing = std::nullopt;
    /// Items on the square when there are several; set on the displayed (last) item, which the native
    /// view then highlights.
    std::optional<int> stack = std::nullopt;
    /// Paint a tileset multiplies into the part's sprites: the vehicle part's own colors.
    std::optional<look_tint> tint = std::nullopt;
    /// Set on the overlay of a mutation.
    std::optional<look_mutation> mutation = std::nullopt;
    /// The ids a tileset's tints are keyed by for this thing, in the order the native view tries them (the
    /// flags, then the id), kept to the tints the client said its tileset has.
    std::vector<std::string> tint_keys = {};
    /// The tint an item's own variables ask for, over what the tileset's tints give.
    std::optional<look_tint_vars> tint_vars = std::nullopt;
    /// Terrain that stands as a wall, which a view drawn from above can raise: `WALL`, or joined to walls
    /// (`CONNECT_TO_WALL`) and not `FLAT`, as a closed door or a window.
    bool wall = false;
    /// Terrain the data flags `WINDOW`.
    bool window = false;
    /// Of terrain and furniture: the data's `coverage`, the percent of a person behind it that it covers.
    int coverage = 0;
    /// Of terrain, furniture and fields: what the thing stands as, `door`, `fence`, `plant`, `seat`, `table`, `bed`,
    /// `fire` or `gas`; empty for anything else.
    std::string form = {};
    auto operator<=>( const look & ) const = default; // *NOPAD*
};
struct field_entry {
    look appearance;
    int intensity = 0;
    /// The field's type hides the items under it from the native view.
    bool hides_items = false;
    auto operator<=>( const field_entry & ) const = default; // *NOPAD*
};
enum class knowledge { remembered, visible, sensed };
/// The colored light reaching a square, as the native tiles view tints it: the light's game-data color as
/// `#rrggbb` and its strength, twice its light level, from 1 to 255.
struct light_glow {
    std::string color = {};
    int strength = 0;
    auto operator<=>( const light_glow & ) const = default; // *NOPAD*
};
/// The two layers the avatar memorized for a cell.
struct memory_layers {
    look terrain;
    std::optional<look> overlay = std::nullopt;
    auto operator<=>( const memory_layers & ) const = default; // *NOPAD*
};
struct cell {
    position at;
    knowledge known = knowledge::visible;
    std::optional<look> terrain = std::nullopt;
    std::optional<look> furniture = std::nullopt;
    std::vector<field_entry> fields = {};
    /// A corpse on the square can rise; the native view marks it.
    bool reviving = false;
    /// A cargo part of a vehicle on the square holds items; the native view highlights the part.
    bool cargo = false;
    /// The square is not outside: a roof or a floor above covers it, so weather does not fall there.
    bool roofed = false;
    std::vector<look> traps = {};
    std::vector<look> items = {};
    std::optional<look> vehicle = std::nullopt;
    std::optional<int> light = std::nullopt;
    std::optional<light_glow> glow = std::nullopt;
    /// A square in sight on the avatar's level that the native view looks through: how many levels down
    /// the square is that it draws here. Bare open air has nothing else.
    std::optional<int> below = std::nullopt;
    std::optional<memory_layers> memory = std::nullopt;
    auto operator<=>( const cell & ) const = default; // *NOPAD*
};
/// A direction the native game names: a stable id (`north`, `southwest_above`, `center`, `above`) and its
/// name in the connection's language.
struct heading {
    std::string id = {};
    std::string name = {};
    auto operator<=>( const heading & ) const = default; // *NOPAD*
};
/// A hurt creature's health in the fifths and color of the native look-around bar.
struct entity_health {
    int fifths = 0;
    std::string color = {};
    auto operator<=>( const entity_health & ) const = default; // *NOPAD*
};
/// `sensed` entities carry only id, position and sense.
struct entity {
    std::string id = {};
    position at;
    knowledge known = knowledge::visible;
    std::optional<look> appearance = std::nullopt;
    std::optional<std::string> name = std::nullopt;
    /// Sprites drawn over a character, bottom first: worn and wielded items, mutations, bionics.
    std::vector<look> overlays = {};
    /// The ids of the mutations of a character by mutation type, which tint the mutation overlays.
    std::map<std::string, std::vector<std::string>> traits = {};
    /// How the creature regards the avatar (hostile, friendly, neutral, any) and whether it is aware of
    /// it: the native view marks monsters and NPCs with it.
    std::optional<std::string> attitude = std::nullopt;
    bool aware = false;
    /// None at full health.
    std::optional<entity_health> health = std::nullopt;
    std::vector<std::string> statuses = {};
    std::optional<std::string> sense = std::nullopt;
    auto operator<=>( const entity & ) const = default; // *NOPAD*
};
struct avatar_stat {
    std::string id = {};
    std::string label = {};
    std::string value = {};
    /// Native color name of the stat, empty when the sidebar does not color it.
    std::string color = {};
    /// The abbreviation the native sidebar prints before the value, translated; empty where it prints none.
    std::string short_label = {};
    auto operator<=>( const avatar_stat & ) const = default; // *NOPAD*
};
/// The native condition marker at the start of an item name (`||`, `|\`, ...) and its color name.
struct item_durability {
    std::string symbol = {};
    std::string color = {};
    auto operator<=>( const item_durability & ) const = default; // *NOPAD*
};
struct inventory_entry {
    /// Names the row for the item operations; stable while the stack stays what it is.
    std::string id = {};
    look appearance;
    /// The display name the native UI shows, without color markup.
    std::string name = {};
    /// How much the row holds in native units, charges of a charges item, else items: the most a pickup or
    /// drop of the row takes.
    std::optional<std::uint64_t> count = std::nullopt;
    /// "wielded", "worn" or "carried"; absent for items on the ground.
    std::optional<std::string> slot = std::nullopt;
    /// Present when `name` starts with the native condition marker: the marker and the color it is drawn in.
    std::optional<item_durability> durability = std::nullopt;
    /// The row the way the native lists word it: the stack's display name with its count and condition
    /// marker as color markup, then the translated slot for a wielded or worn item.
    std::string label = {};
    auto operator<=>( const inventory_entry & ) const = default; // *NOPAD*
};
/// A value the native sidebar prints, after the label its panel prints before it.
struct sidebar_text {
    std::string text = {};
    /// Native color name of the value where the color carries meaning; empty otherwise.
    std::string color = {};
    /// The native panel label, translated; empty where the panel prints none.
    std::string label = {};
    auto operator<=>( const sidebar_text & ) const = default; // *NOPAD*
};
struct sidebar_limb {
    std::string id = {};
    /// The native short label (`HEAD`, `TORSO`, ...) in the limb's condition color.
    sidebar_text label;
    int hp = 0;
    int hp_max = 0;
    /// Color of the native health bar, which also marks a broken limb.
    std::string color = {};
    bool broken = false;
    auto operator<=>( const sidebar_limb & ) const = default; // *NOPAD*
};
/// A panel of the native sidebar layout the panel manager lists. Panels it hides now (the vehicle panel
/// outside a vehicle) are left out.
struct sidebar_panel {
    /// The panel manager's own name for it (`Health`, `Compass`, `Messages`), untranslated.
    std::string id = {};
    /// The name the native panel options print, translated.
    std::string label = {};
    /// The player has not switched it off.
    bool enabled = true;
    /// Rows; absent for the panel that takes the rest of the column (`Messages`).
    std::optional<int> height = std::nullopt;
    auto operator<=>( const sidebar_panel & ) const = default; // *NOPAD*
};
/// The native sidebar options: the layout in use, the side it sits on and its width in columns.
struct sidebar_layout {
    std::string id = {};
    /// `left` or `right`.
    std::string side = {};
    int width = 0;
    /// In the panel manager's order, top to bottom.
    std::vector<sidebar_panel> panels = {};
    auto operator<=>( const sidebar_layout & ) const = default; // *NOPAD*
};
/// What the native sidebar (classic layout) shows, taken from the same getters, with the labels its panels
/// print. A client lays these out itself.
struct sidebar_value {
    sidebar_layout layout;
    std::vector<sidebar_limb> limbs = {};
    sidebar_text pain;
    sidebar_text hunger;
    sidebar_text thirst;
    sidebar_text fatigue;
    int focus = 0;
    std::string focus_label = {};
    /// `text` is the native face for the morale level.
    sidebar_text morale;
    int morale_level = 0;
    int stamina = 0;
    int stamina_max = 0;
    std::string stamina_color = {};
    std::string stamina_label = {};
    int speed = 0;
    std::string speed_color = {};
    std::string speed_label = {};
    int move_counter = 0;
    /// `walk`, `run`, `crouch` or `prone`.
    std::string move_mode = {};
    /// The native one-letter name of the mode, translated.
    std::string move_text = {};
    std::string move_color = {};
    /// Body temperature state with the native trend arrows.
    sidebar_text temperature;
    sidebar_text power;
    bool safe_mode = false;
    std::string safe_color = {};
    std::string safe_label = {};
    sidebar_text location;
    /// Unlabelled underground, where the native panel prints only "Underground".
    sidebar_text weather;
    std::string season = {};
    std::string day_label = {};
    int day = 0;
    /// The watch time, the approximate time of day without a watch, or the native unknown-time text underground.
    std::string clock = {};
    bool has_watch = false;
    /// Ambient temperature, only with a thermometer.
    std::optional<sidebar_text> ambient = std::nullopt;
    sidebar_text weapon;
    std::optional<sidebar_text> style = std::nullopt;
    auto operator<=>( const sidebar_value & ) const = default; // *NOPAD*
};
/// A creature the native compass panel lists.
struct compass_creature {
    /// The creature's entity ID.
    std::string id = {};
    look appearance;
    std::string name = {};
    /// How the creature regards the avatar, as an entity's `attitude`.
    std::string attitude = {};
    /// Squares from the avatar, as the native sound and safe mode code count them.
    int distance = 0;
    auto operator<=>( const compass_creature & ) const = default; // *NOPAD*
};
/// The creatures the avatar sees toward one direction, nearest first.
struct compass_direction {
    heading toward;
    /// The heading the native compass panel prints for it ("North:", "NE:"), translated; empty for `center`, which it does not print.
    std::string label = {};
    std::vector<compass_creature> creatures = {};
    auto operator<=>( const compass_direction & ) const = default; // *NOPAD*
};
/// What the avatar is busy with over turns, as the native wait popup names it: the activity's id and verb and,
/// when it is of a set length, the thousandths of it done.
struct activity_value {
    std::string id = {};
    std::string name = {};
    std::optional<int> done = std::nullopt;
    auto operator<=>( const activity_value & ) const = default; // *NOPAD*
};
struct avatar_value {
    std::string id = {};
    position at;
    std::string name = {};
    std::optional<look> appearance = std::nullopt;
    std::vector<look> overlays = {};
    std::map<std::string, std::vector<std::string>> traits = {};
    std::vector<avatar_stat> stats = {};
    /// The wielded item, the worn items, then the carried stacks. Items have no instance IDs.
    std::vector<inventory_entry> inventory = {};
    sidebar_value sidebar;
    /// The creatures the native compass panel lists, by the direction of their horizontal offset from the avatar
    /// (`center` for the avatar's own column), clockwise from north and `center` last; directions with none are absent.
    std::vector<compass_direction> compass = {};
    /// The modes `set_mode` sets, with their values now.
    std::vector<mode_value> modes = {};
    /// The stacks on the avatar's own square, which a pickup chooses from.
    std::vector<inventory_entry> ground = {};
    std::optional<activity_value> activity = std::nullopt;
    auto operator<=>( const avatar_value & ) const = default; // *NOPAD*
};
/// A file of the game data that adds sprites to the tilesets its `compatibility` names.
struct mod_tileset_file {
    /// The JSON file, and the folder its sheet files are named from.
    std::string path = {};
    std::string base = {};
    /// Which `mod_tileset` of the file this is, counting from 1.
    int index = 1;
    std::vector<std::string> compatibility = {};
    auto operator<=>( const mod_tileset_file & ) const = default; // *NOPAD*
};
/// The sky over the avatar: how much daylight there is, where the sun or the moon is and what the weather lets fall.
struct sky_value {
    /// The daylight now, in percent of full day.
    int daylight = 0;
    /// The weather's precipitation: `none`, `very_light`, `light`, `medium` or `heavy`.
    std::string precipitation = "none";
    /// What falls is rain, not snow.
    bool rain = false;
    /// How much ambient light the weather takes away, in the game's light points.
    int dimming = 0;
    /// How far the sun has crossed the sky from sunrise to sunset, in percent; empty at night.
    std::optional<int> sun;
    /// How far the moon has crossed the night sky from sunset to sunrise, in percent; empty by day.
    std::optional<int> moon;
    /// The moon's phase: `new`, `waxing_crescent`, `half_moon_waxing`, `waxing_gibbous`, `full`, `waning_gibbous`,
    /// `half_moon_waning` or `waning_crescent`.
    std::string moon_phase = "new";
    auto operator<=>( const sky_value & ) const = default; // *NOPAD*
};
struct environment_value {
    std::string turn = {};
    std::string time = {};
    std::string weather = {};
    std::string season = {};
    sky_value sky = {};
    /// The tilesets of the loaded game data, in the order the native view applies them.
    std::vector<mod_tileset_file> mod_tilesets = {};
    auto operator<=>( const environment_value & ) const = default; // *NOPAD*
};

/// What the avatar knows. Cells are sparse: absent inside the coverage means unknown.
struct world_state {
    std::optional<bounds> coverage = std::nullopt;
    /// The squares of the avatar's level a map click resolves to: the native terrain window.
    std::optional<bounds> view = std::nullopt;
    std::map<position, cell> cells = {};
    std::map<std::string, entity> entities = {};
    std::optional<avatar_value> avatar = std::nullopt;
    std::optional<environment_value> environment = std::nullopt;
    /// The native auto-move route a map click planned and not yet confirmed, nearest square first.
    std::vector<position> route = {};
    auto operator==( const world_state & ) const -> bool = default;
};

/// A capture that names the cells that may differ from the previous capture instead of listing every
/// cell, so that publishing it costs what changed. A cell the patch does not name equals the previous one.
struct world_patch {
    /// Every section but the cells, which stay empty.
    world_state world;
    /// The named cells the avatar knows now.
    std::map<position, cell> cells = {};
    /// The named cells the avatar knows nothing about now; it leaves out those the previous state did not hold.
    std::set<position> unknown = {};
    /// `cells` is every cell the avatar knows: a previous cell it does not hold is forgotten.
    bool exhaustive = false;
};

} // namespace engine_client
