#pragma once

#include "engine_client_component.h"
#include "type_id.h"

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

class Character;
class inventory;
class item;

/// Detail pieces shared by the crafting and construction screens, composed from model state.
namespace engine_client
{

/// A native caption as a label: without its trailing colon and spaces.
auto caption( std::string native ) -> std::string;

/// One factor of a crafting or construction speed, as a multiplier of the base time.
struct speed_modifier {
    /// Stable and untranslated.
    std::string key = {};
    std::string label = {};
    float multiplier = 1.0f;
};
/// The stats a detail panel shows for `modifiers`, the total first: the total and each factor that is not
/// 100%, or every factor when `verbose`, and none when all are 100%.
auto speed_stats( const std::vector<speed_modifier> &modifiers,
                  bool verbose ) -> std::vector<panel_stat>;

struct skill_view_options {
    /// The id of the list.
    std::string id = {};
    std::string label = {};
    /// The skills and the level each needs.
    std::vector<std::pair<skill_id, int>> skills = {};
    const Character &crafter;
};
/// A list with a row for each skill: whether the crafter has the level in `enabled`, and the level it needs
/// and the level it has in its columns, the level colored red when short of the need, light blue under 125% of
/// it and green otherwise. None when `skills` is empty.
auto skill_requirements( const skill_view_options &options ) -> std::optional<component>;

struct requirement_view_options {
    const requirement_data &requirements;
    /// What the crafter can draw on.
    const inventory &available;
    /// Which items count as components, as `requirement_data::can_make_with_inventory` takes it; empty: all.
    std::function < auto( const item & ) -> bool > filter = {};
    int batch = 1;
    /// Starts the ids of the lists, which a screen showing several requirements keeps apart.
    std::string prefix = "requirements:";
};
/// The qualities, then the tools, that the requirements need. A row says whether the crafter has it in
/// `enabled` and how many it needs and has in its columns; a group of alternatives is a parent row over
/// its alternatives.
auto tool_requirements( const requirement_view_options &options ) -> component;
/// The components that the requirements need, in the same shape.
auto component_requirements( const requirement_view_options &options ) -> component;

} // namespace engine_client
