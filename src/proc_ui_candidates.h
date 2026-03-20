#pragma once

#include <string>
#include <vector>

#include "proc_builder.h"

namespace proc
{

struct candidate_label_entry {
    std::string key;
    std::string label;
    part_ix ix = invalid_part_ix;
    int count = 1;
};

struct grouped_candidate_entry {
    std::string key;
    std::string label;
    std::vector<part_ix> ixs;
    int total_count = 0;
};

auto group_candidate_entries( const std::vector<candidate_label_entry> &entries ) ->
std::vector<grouped_candidate_entry>;
auto grouped_candidate_label( const grouped_candidate_entry &entry ) -> std::string;
auto first_grouped_candidate_ix( const grouped_candidate_entry &entry ) -> part_ix;

} // namespace proc
