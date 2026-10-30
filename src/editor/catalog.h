#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace editor {

/// What a mapgen can name.
enum class catalog_kind {
    terrain,
    furniture,
    trap,
    field,
    item,
    item_group,
    monster,
    monster_group,
    vehicle,
    npc_template,
    nested_mapgen,
    palette,
    overmap_terrain,
};

struct catalog_entry {
    std::string id;
    /// The translated name; the id for what has none.
    std::string name;
    /// The mod that last defined it; empty where the game does not record it.
    std::string mod;
    /// What tells it from the other entries the query found of the same name: the part of its id
    /// after the prefix they share; empty for a name of its own.
    std::string distinct;
};

struct catalog_query {
    catalog_kind kind = catalog_kind::terrain;
    /// Matched, ignoring case, against the id and the name; empty matches all.
    std::string filter = {};
    /// Keeps only what this mod last defined; empty keeps all.
    std::string mod = {};
    std::size_t page = 0;
    std::size_t page_size = 50;
};

struct catalog_page {
    /// With a filter, first by how well the name matches it: the name itself, a name starting
    /// with it, a word of the name starting with it, a name holding it, then an id holding it;
    /// then the game's own before a mod's, what the game's data names most, the shortest name,
    /// then by id. Without a filter, by id.
    std::vector<catalog_entry> entries;
    /// How many match, on every page.
    std::size_t total = 0;
};

auto search_catalog(const catalog_query& query) -> catalog_page;

/// The ids of what the game keeps where only its own file can list it.
auto nested_mapgen_ids() -> std::vector<std::string>;
auto palette_ids() -> std::vector<std::string>;
auto npc_template_ids() -> std::vector<std::string>;

} // namespace editor
