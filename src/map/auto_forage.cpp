#include "map/auto_forage.h"

#include "map/mapdata.h"

namespace {

auto has_forage_class(forage_class present, forage_class wanted) -> bool {
    return (static_cast<std::uint8_t>(present) & static_cast<std::uint8_t>(wanted)) != 0;
}

} // namespace

auto pick_auto_forage_target(forage_class terrain, forage_class furniture, std::string_view option)
    -> auto_forage_target {
    const auto everything = option == "both";
    const auto bushes = everything || option == "bushes";
    const auto trees = everything || option == "trees";
    const auto flowers = everything || option == "flowers";
    if ((bushes && has_forage_class(terrain, forage_class::bushes))
        || (trees && has_forage_class(terrain, forage_class::trees))) {
        return auto_forage_target::terrain;
    }
    if (flowers && has_forage_class(furniture, forage_class::flowers)) {
        return auto_forage_target::furniture;
    }
    return auto_forage_target::none;
}
