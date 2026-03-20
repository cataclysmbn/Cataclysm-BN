#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "proc_schema.h"

class item;
class recipe;

namespace proc
{

struct part_search_options {
    std::string name;
    std::string where;
};

struct builder_state {
    schema_id id = schema_id::NULL_ID();
    schema sch;
    std::vector<part_fact> facts;
    std::map<slot_id, std::vector<part_ix>> cand;
    std::map<slot_id, std::vector<part_ix>> chosen;
    fast_blob fast;

    auto picks_for( const slot_id &slot ) const -> const std::vector<part_ix>&; // *NOPAD*
};

auto matches_atom( const part_fact &fact, const std::string &atom ) -> bool;
auto matches_slot( const slot_data &slot, const part_fact &fact ) -> bool;
auto build_candidates( const schema &sch,
                       const std::vector<part_fact> &facts ) -> std::map<slot_id, std::vector<part_ix>>;
auto build_state( const schema &sch, const std::vector<part_fact> &facts ) -> builder_state;
auto pick_count( const builder_state &state, part_ix ix ) -> int;
auto remaining_uses( const builder_state &state, part_ix ix ) -> int;
auto filter_available_candidates( const builder_state &state,
                                  const std::vector<part_ix> &candidates ) -> std::vector<part_ix>;
auto slot_can_meet_minimum( const builder_state &state, const schema &sch,
                            const slot_id &slot ) -> bool;
auto slot_complete( const builder_state &state, const schema &sch, const slot_id &slot ) -> bool;
auto complete( const builder_state &state, const schema &sch ) -> bool;
auto add_pick( builder_state &state, const schema &sch, const slot_id &slot, part_ix ix ) -> bool;
auto remove_pick( builder_state &state, const slot_id &slot, part_ix ix ) -> bool;
auto remove_last_pick( builder_state &state, const slot_id &slot ) -> bool;
auto selected_picks( const builder_state &state, const schema &sch ) -> std::vector<craft_pick>;
auto selected_facts( const builder_state &state ) -> std::vector<part_fact>;
auto rebuild_fast( const builder_state &state ) -> fast_blob;
auto debug_part_fact( const schema &sch, const item &it,
                      part_ix ix ) -> std::optional<part_fact>;
auto fast_fp( const schema &sch, const fast_blob &blob,
              const std::vector<part_fact> &facts ) -> std::string;
auto part_search_texts( const part_fact &fact,
                        const part_search_options &opts ) -> std::vector<std::string>;
auto part_matches_search( const part_fact &fact, const part_search_options &opts,
                          const std::string &txt ) -> bool;
auto recipe_search_texts( const recipe &rec ) -> std::vector<std::string>;
auto recipe_matches_search( const recipe &rec, const std::string &txt ) -> bool;

} // namespace proc
