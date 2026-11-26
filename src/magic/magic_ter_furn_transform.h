#pragma once

#include "coordinates.h"
#include "type_id.h"
#include "weighted_list.h"

#include <map>
#include <optional>
#include <vector>

class Creature;
class JsonObject;
class map;
class mapgen_constructor;
struct tripoint;

// this is a small class that contains the "results" of a terrain transform.
// T can be either ter_str_id or furn_str_id
template <class T> class ter_furn_data {
private:
    weighted_int_list<T> list;
    std::string message;
    bool message_good;

public:
    ter_furn_data() = default;
    ter_furn_data(
        const weighted_int_list<T>& list, const std::string& message, const bool message_good)
        : list(list),
          message(message),
          message_good(message_good) {}

    auto has_msg() const -> bool;
    void add_msg(const Creature& critter) const;
    auto pick() const -> std::optional<T>;
    void load(const JsonObject& jo);
    auto is_empty() const -> bool;
};

class ter_furn_transform {

private:
    std::string fail_message;

    std::map<ter_str_id, ter_furn_data<ter_str_id>> ter_transform;
    std::map<std::string, ter_furn_data<ter_str_id>> ter_flag_transform;
    ter_furn_data<ter_str_id> diggable_ter_transform;

    std::map<furn_str_id, ter_furn_data<furn_str_id>> furn_transform;
    std::map<std::string, ter_furn_data<furn_str_id>> furn_flag_transform;

    auto next_ter(const ter_str_id& ter) const -> std::optional<ter_str_id>;
    auto next_ter(const std::string& flag) const -> std::optional<ter_str_id>;
    auto next_furn(const furn_str_id& furn) const -> std::optional<furn_str_id>;
    auto next_furn(const std::string& flag) const -> std::optional<furn_str_id>;

    template <class T, class K>
    auto find_transform(const std::map<K, ter_furn_data<T>>& list, const K& key) const
        -> std::optional<ter_furn_data<T>>;

    template <class T, class K>
    auto next(const std::map<K, ter_furn_data<T>>& list, const K& key) const -> std::optional<T>;

    // return value is success of message found
    template <class T, class K>
    auto add_message(
        const std::map<K, ter_furn_data<T>>& list, const K& key, const Creature& critter,
        const tripoint_bub_ms& location) const -> bool;

public:
    ter_furn_transform_id id;
    bool was_loaded = false;

    void add_all_messages(const Creature& critter, const tripoint_bub_ms& location) const;
    void add_all_messages(
        const map& m, const Creature& critter, const tripoint_bub_ms& location) const;

    void transform(const tripoint_bub_ms& location) const;
    void transform(map& m, const tripoint_bub_ms& location) const;
    auto transform(mapgen_constructor& m, const point_omt_ms& location) const -> void;

    static void load_transform(const JsonObject& jo, const std::string& src);
    void load(const JsonObject& jo, const std::string&);

    static auto get_all() -> const std::vector<ter_furn_transform>&;
    static void reset_all();
    auto is_valid() const -> bool;
};
