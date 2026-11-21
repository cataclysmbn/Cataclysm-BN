#pragma once

#include <optional>
#include <string>
#include <vector>
#include <variant>
#include "color.h"
#include "type_id.h"
#include "translations.h"

class JsonObject;
struct uilist_entry;

struct loot_zone_group {
    string_id<loot_zone_group> id;
    bool was_loaded = false;
    translation title;
    std::vector<zone_type_id> categories;
    std::string kind = "storage";
    int sort_order = 0;

    auto load( const JsonObject &jo, const std::string &src ) -> void;
};

/// Rows shared by the checklist renderer and its interaction tests.
struct zone_checklist_row {
    std::string label;
    std::string description;
    std::vector<zone_type_id> members;
    bool rule = false;
    nc_color color = c_light_gray;
    bool single_type = false;
};

auto make_zone_checklist_rows( bool categories_only ) -> std::vector<zone_checklist_row>;

/// Ordered presentation groups for the zone creation checklist.
auto loot_zone_groups() -> const std::vector<loot_zone_group> &; // *NOPAD*

/// A storage selection is one category zone; each rule remains a native functional zone.
struct zone_category_selection {
    std::vector<zone_type_id> categories;
    std::vector<zone_type_id> rules;
};

/// Build a checklist entry; text searches include individual types, not group toggles.
auto make_zone_checklist_entry( const zone_checklist_row &row,
                                const zone_category_selection &selection, int retval ) -> uilist_entry;

/// Preserve native types used by butchery, plank cutting and seed selection.
auto zone_storage_type( const std::vector<zone_type_id> &selected ) -> zone_type_id;

/// Confirming the existing single category retains its native sorting semantics.
auto zone_storage_type_after_edit( const zone_type_id &current,
                                   const std::vector<zone_type_id> &selected ) -> zone_type_id;

auto zone_storage_categories() -> std::vector<zone_type_id>;
auto zone_creation_rules() -> std::vector<zone_type_id>;
auto toggle_zone_categories( std::vector<zone_type_id> &selected,
                             const std::vector<zone_type_id> &members ) -> void;
using zone_creation_choice = std::variant<zone_category_selection, zone_type_id>;

/// Keep checkbox state when a separate setup dialog is canceled.
auto query_zone_creation( zone_category_selection &selection,
                          bool categories_only = false ) -> std::optional<zone_creation_choice>;

auto load_loot_zone_group( const JsonObject &jo, const std::string &src ) -> void;
auto finalize_loot_zone_groups() -> void;
/// Clear loaded groups, or restore a complete snapshot without losing translation metadata.
auto reset_loot_zone_groups( std::vector<loot_zone_group> groups = {} ) -> void;
auto zone_selection_name( const std::vector<zone_type_id> &categories ) -> std::string;

/// Whether a native functional zone can be added without a separate options dialog.
auto zone_can_share_area( const zone_type_id &type ) -> bool;
