#pragma once

#include <list>
#include <vector>
#include <set>
#include <string>
#include <mutex>

#include "omdata.h"
#include "type_id.h"

class JsonObject;
class JsonIn;
struct overmap_location;

enum class overmap_connection_layout {
    city,
    p2p,
    last
};

template<>
struct enum_traits<overmap_connection_layout> {
    static constexpr overmap_connection_layout last = overmap_connection_layout::last;
};

class overmap_connection
{
    public:
        class subtype
        {
                friend overmap_connection;

            public:
                enum class flag { orthogonal };

            public:
                oter_type_str_id terrain;

                int basic_cost = 0;

                int weight = 1;

                bool allows_terrain( const oter_id &oter ) const;
                bool allows_turns() const {
                    return terrain->is_linear();
                }

                bool is_orthogonal() const {
                    return flags.contains( flag::orthogonal );
                }

                void load( const JsonObject &jo );
                void deserialize( JsonIn &jsin );

            private:
                std::set<overmap_location_id> locations;
                std::set<flag> flags;
        };

    public:
        overmap_connection() = default;
        overmap_connection( const overmap_connection &other );
        overmap_connection &operator=( const overmap_connection &other );

        overmap_connection( overmap_connection &&other ) noexcept;
        overmap_connection &operator=( overmap_connection &&other ) noexcept;

        const subtype *pick_subtype_for( const oter_id &ground ) const;
        void clear_subtype_cache() const;
        bool can_start_at( const oter_id &ground ) const;
        bool has( const oter_id &oter ) const;

        const overmap_connection_layout &get_layout() const {
            return layout;
        }

        void load( const JsonObject &jo, const std::string &src );
        void check() const;
        void finalize();

    public:
        overmap_connection_id id;
        bool was_loaded = false;

        oter_type_str_id default_terrain;

    private:
        struct cache {
            const subtype *value = nullptr;
            bool assigned = false;
            explicit operator bool() const {
                return assigned;
            }
        };

        overmap_connection_layout layout;
        std::vector<subtype> subtypes;
        mutable std::unordered_map<oter_id, cache> cached_subtypes;
        mutable std::mutex mutex;
};

namespace overmap_connections
{

void load( const JsonObject &jo, const std::string &src );
void finalize();
void check_consistency();
void reset();

overmap_connection_id guess_for( const oter_type_id &oter );
overmap_connection_id guess_for( const oter_id &oter );

} // namespace overmap_connections


