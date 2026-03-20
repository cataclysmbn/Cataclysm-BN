#pragma once

#include <map>
#include <string>
#include <vector>

#include "proc_schema.h"

class recipe;

namespace proc
{

struct builder_state {
    schema_id id = schema_id::NULL_ID();
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
auto add_pick( builder_state &state, const schema &sch, const slot_id &slot, part_ix ix ) -> bool;
auto remove_pick( builder_state &state, const slot_id &slot, part_ix ix ) -> bool;
auto selected_facts( const builder_state &state ) -> std::vector<part_fact>;
auto rebuild_fast( const builder_state &state ) -> fast_blob;
auto fast_fp( const schema &sch, const fast_blob &blob,
              const std::vector<part_fact> &facts ) -> std::string;
auto recipe_search_texts( const recipe &rec ) -> std::vector<std::string>;
auto recipe_matches_search( const recipe &rec, const std::string &txt ) -> bool;

} // namespace proc
