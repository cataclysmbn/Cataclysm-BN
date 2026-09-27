#pragma once

#include "omdata.h"
#include "type_id.h"

#include <list>
#include <mutex>
#include <set>
#include <string>
#include <vector>

class JsonObject;
class JsonIn;
struct overmap_location;

enum class overmap_connection_layout { city, p2p, last };

template <> struct enum_traits<overmap_connection_layout> {
    static constexpr overmap_connection_layout last = overmap_connection_layout::last;
};

class overmap_connection {
public:
    class subtype {
        friend overmap_connection;

    public:
        enum class flag { orthogonal };

    public:
        oter_type_str_id terrain;

        int basic_cost = 0;

        int weight = 1;

        auto allows_terrain(const oter_id& oter) const -> bool;
        auto allows_turns() const -> bool { return terrain->is_linear(); }

        auto is_orthogonal() const -> bool { return flags.contains(flag::orthogonal); }

        void load(const JsonObject& jo);
        void deserialize(JsonIn& jsin);

    private:
        std::set<overmap_location_id> locations;
        std::set<flag> flags;
    };

public:
    overmap_connection() = default;
    overmap_connection(const overmap_connection& other);
    auto operator=(const overmap_connection& other) -> overmap_connection&;

    overmap_connection(overmap_connection&& other) noexcept;
    auto operator=(overmap_connection&& other) noexcept -> overmap_connection&;

    auto pick_subtype_for(const oter_id& ground) const -> const subtype*;
    void clear_subtype_cache() const;
    auto can_start_at(const oter_id& ground) const -> bool;
    auto has(const oter_id& oter) const -> bool;

    auto get_layout() const -> const overmap_connection_layout& { return layout; }

    void load(const JsonObject& jo, const std::string& src);
    void check() const;
    void finalize();

public:
    overmap_connection_id id;
    bool was_loaded = false;

    oter_type_str_id default_terrain;

private:
    struct cache {
        const subtype* value = nullptr;
        bool assigned = false;
        explicit operator bool() const { return assigned; }
    };

    overmap_connection_layout layout;
    std::vector<subtype> subtypes;
    mutable std::unordered_map<oter_id, cache> cached_subtypes;
    mutable std::mutex mutex;
};

namespace overmap_connections {

void load(const JsonObject& jo, const std::string& src);
void finalize();
void check_consistency();
void reset();

auto guess_for(const oter_type_id& oter) -> overmap_connection_id;
auto guess_for(const oter_id& oter) -> overmap_connection_id;

} // namespace overmap_connections
