#pragma once

#include <string>

class vehicle;

/// A part as the native part list prints it: the name with its contents, between the marks of its
/// layer.
struct part_list_label {
    std::string name;
    std::string open;
    std::string close;
};

struct part_list_label_options {
    /// Fuel stores name their contents' amount.
    bool detail = false;
    /// The name starts with the durability symbol.
    bool health_bar = true;
};

auto part_list_label_of(const vehicle& veh, int part, const part_list_label_options& opts)
    -> part_list_label;
