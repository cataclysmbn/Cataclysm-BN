#include "panels_utility.h"

#include "catacharset.h"
#include "color.h"

#include <unordered_map>

std::string trunc_ellipse(const std::string& input, unsigned int trunc) {
    if (utf8_width(input) > static_cast<int>(trunc)) {
        return utf8_truncate(input, trunc - 1) + "…";
    }
    return input;
}

nc_color color_compare_base(int base, int value) {
    if (base < value) {
        return c_green;
    } else if (value < base) {
        return c_red;
    } else {
        return c_white;
    }
}

std::string value_trimmed(int value, int maximum) {
    if (value > maximum) {
        return "++";
    } else {
        return std::to_string(value);
    }
}

nc_color focus_color(int focus) {
    if (focus < 25) {
        return c_red;
    } else if (focus < 50) {
        return c_light_red;
    } else if (focus < 75) {
        return c_yellow;
    } else if (focus < 100) {
        return c_light_gray;
    } else if (focus < 125) {
        return c_white;
    } else {
        return c_green;
    }
}
