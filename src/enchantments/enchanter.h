#include "item.h"
#include "map/mapdata.h"
#include "requirements.h"

#include <vector>

namespace enchanter {

auto total_requirements(const enchant_info& info) -> requirement_data;

auto enchantment_info(const enchant_info& info, Character& crafter, int fold_width, item& itm)
    -> std::vector<std::string>;

} // namespace enchanter
