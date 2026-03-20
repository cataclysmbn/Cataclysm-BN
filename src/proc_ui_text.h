#pragma once

#include <string>
#include <vector>

namespace proc
{

enum class builder_readiness {
    missing_required_slots,
    missing_recipe_requirements,
    ready_to_craft,
};

struct counted_label {
    std::string label;
    int count = 0;
};

auto group_duplicate_labels( const std::vector<std::string> &labels ) -> std::vector<counted_label>;
auto grouped_label_summary( const std::vector<std::string> &labels,
                            const std::string &empty_label ) -> std::string;
auto grouped_label_lines( const std::vector<std::string> &labels ) -> std::vector<std::string>;
auto builder_readiness_label( builder_readiness readiness ) -> std::string;
auto compact_requirement_text( const std::string &text ) -> std::string;

} // namespace proc
