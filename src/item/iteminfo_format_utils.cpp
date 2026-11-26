#include "iteminfo_format_utils.h"

void insert_separation_line(std::vector<iteminfo>& info) {
    if (info.empty() || info.back().sName != "--") {
        info.push_back(iteminfo("DESCRIPTION", "--"));
    }
}
