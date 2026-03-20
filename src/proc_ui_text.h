#pragma once

#include <string>
#include <vector>

namespace proc
{

struct counted_label {
    std::string label;
    int count = 0;
};

auto group_duplicate_labels( const std::vector<std::string> &labels ) -> std::vector<counted_label>;
auto grouped_label_summary( const std::vector<std::string> &labels,
                            const std::string &empty_label ) -> std::string;
auto grouped_label_lines( const std::vector<std::string> &labels ) -> std::vector<std::string>;

} // namespace proc
