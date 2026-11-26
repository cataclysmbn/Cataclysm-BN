#pragma once

#include "type_id.h"

#include <string>
#include <vector>

class JsonObject;

struct overmap_location {
public:
    void load(const JsonObject& jo, const std::string& src);
    void check() const;
    void finalize();

    // Test if oter meets the terrain restrictions.
    auto test(const oter_id& oter) const -> bool;
    auto get_all_terrains() const -> std::vector<oter_type_id>;
    auto get_random_terrain() const -> oter_type_id;

public:
    // Used by generic_factory
    overmap_location_id id;
    bool was_loaded = false;

private:
    std::vector<oter_type_str_id> terrains;
    std::vector<std::string> flags;
};

namespace overmap_locations {

void load(const JsonObject& jo, const std::string& src);
void check_consistency();
void reset();
void finalize();

} // namespace overmap_locations
