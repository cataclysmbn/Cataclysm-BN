#pragma once

#include <string>
#include <vector>

#include "translations.h"
#include "type_id.h"

class JsonObject;

struct dream {
    std::vector<translation> messages; // The messages that the dream will give
    mutation_category_id category; // The category that will trigger the dream
    int strength = 0; // The category strength required for the dream
};

namespace dreams
{
void load( const JsonObject &jo );
void clear();

/** Returns a random dream description that matches given category and strength. */
std::string get_random_for_category( const mutation_category_id &cat, int strength );

} // namespace dreams


