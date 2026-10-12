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

auto part_list_label_of(const vehicle& veh, int part, bool detail) -> part_list_label;
