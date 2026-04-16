#pragma once

#include <string>
#include <vector>

#include "translations.h"

class JsonObject;

namespace map_feature_descriptions
{

struct map_feature_description {
    enum class test_type {
        bashable,
        diggable,
        flag,
    };

    translation text;
    test_type test = test_type::bashable;
    std::string flag;
};

void load_map_feature_descriptions( const JsonObject &jo );
void reset_map_feature_descriptions();
const std::vector<map_feature_description> &get_map_feature_descriptions();

} // namespace map_feature_descriptions
