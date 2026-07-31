#include "item.h"
#include "mapdata.h"
#include "requirements.h"

#include <vector>

namespace enchanter {

requirement_data total_requirements(const enchant_info& info);

std::vector<std::string> enchantment_info(
    const enchant_info& info, Character& crafter, int fold_width, item& itm);

} // namespace enchanter
