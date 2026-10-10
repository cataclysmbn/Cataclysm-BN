#pragma once

#include <compare>
#include <cstdint>
#include <map>
#include <optional>
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
    /// Set on the overlay of a worn or wielded item, a bionic or an effect: the ids a tileset's tints are
    /// keyed by, in the order the native view tries them (the flags, then the id).
    std::vector<std::string> tint_keys = {};
    auto operator<=>( const look & ) const = default; // *NOPAD*
};
struct field_entry {
    look appearance;
    int intensity = 0;
    auto operator<=>( const field_entry & ) const = default; // *NOPAD*
};
enum class knowledge { remembered, visible, sensed };
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
    std::vector<look> traps = {};
    std::vector<look> items = {};
    std::optional<look> vehicle = std::nullopt;
    std::optional<int> light = std::nullopt;
    std::optional<memory_layers> memory = std::nullopt;
    auto operator<=>( const cell & ) const = default; // *NOPAD*
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
    auto operator<=>( const avatar_stat & ) const = default; // *NOPAD*
};
struct inventory_entry {
    look appearance;
    /// The display name the native UI shows, without color markup.
    std::string name = {};
    std::optional<std::uint64_t> count = std::nullopt;
    /// "wielded", "worn" or "carried"; absent for items on the ground.
    std::optional<std::string> slot = std::nullopt;
    auto operator<=>( const inventory_entry & ) const = default; // *NOPAD*
};
/// Text the native sidebar prints in a color; `color` is a native color name.
struct sidebar_text {
    std::string text = {};
    std::string color = {};
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
/// What the native sidebar (classic layout) shows, taken from the same getters.
struct sidebar_value {
    std::vector<sidebar_limb> limbs = {};
    sidebar_text pain;
    sidebar_text hunger;
    sidebar_text thirst;
    sidebar_text fatigue;
    int focus = 0;
    /// `text` is the native face for the morale level.
    sidebar_text morale;
    int morale_level = 0;
    int stamina = 0;
    int stamina_max = 0;
    std::string stamina_color = {};
    int speed = 0;
    std::string speed_color = {};
    int move_counter = 0;
    /// `walk`, `run`, `crouch` or `prone`.
    std::string move_mode = {};
    std::string move_color = {};
    /// Body temperature state with the native trend arrows.
    sidebar_text temperature;
    sidebar_text power;
    bool safe_mode = false;
    std::string safe_color = {};
    sidebar_text location;
    sidebar_text weather;
    std::string season = {};
    int day = 0;
    /// The watch time, or the approximate time of day without a watch; empty underground.
    std::string clock = {};
    bool has_watch = false;
    /// Ambient temperature, only with a thermometer.
    std::optional<std::string> ambient = std::nullopt;
    std::string weapon = {};
    std::optional<sidebar_text> style = std::nullopt;
    auto operator<=>( const sidebar_value & ) const = default; // *NOPAD*
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
    /// The stacks on the avatar's own square, which a pickup chooses from.
    std::vector<inventory_entry> ground = {};
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
struct environment_value {
    std::string turn = {};
    std::string time = {};
    std::string weather = {};
    std::string season = {};
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

} // namespace engine_client
