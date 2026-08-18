#include "enchanter.h"

#include "activity_actor_definitions.h"
#include "catalua_hooks.h"
#include "catalua_impl.h"
#include "catalua_sol.h"
#include "character.h"
#include "cursesdef.h"
#include "flag.h"
#include "game.h"
#include "game_inventory.h"
#include "generic_factory.h"
#include "generic_readers.h"
#include "iexamine.h"
#include "input.h"
#include "inventory.h"
#include "item.h"
#include "itype.h"
#include "json.h"
#include "mapdata.h"
#include "messages.h"
#include "output.h"
#include "player.h"
#include "player_activity.h"
#include "point.h"
#include "relic.h"
#include "requirements.h"
#include "skill.h"
#include "string_formatter.h"
#include "string_utils.h"
#include "translations.h"
#include "type_id.h"
#include "ui.h"
#include "ui_manager.h"
#include "uistate.h"

#include <algorithm>
#include <numeric>
#include <ranges>
#include <string>
#include <type_traits>
#include <vector>

namespace {

bool run_can_make_callback(const std::string id, const std::string ench_id) {
    auto& state = *DynamicDataLoader::get_instance().lua.get();
    auto func = cata::get_lua_callback(state, "enchanter_can_make", id);
    if (!func) {
        debugmsg("Lua callback %s for `enchanter_can_make` does not exist. Defaulting to true", id);
        return true;
    }
    auto params = state.lua.create_table();
    params["enchant_info_id"] = ench_id;
    sol::protected_function_result res = func(params);

    check_func_result(res);
    if (res.get_type() != sol::type::boolean) {
        debugmsg(
            "Lua callback %s for `enchanter_can_make` expected boolean result. Defaulting to true",
            id);
        return true;
    }
    return res.get<bool>();
}

bool run_can_use_on_callback(const std::string id, const std::string ench_id, const item& itm) {
    auto& state = *DynamicDataLoader::get_instance().lua.get();
    auto func = cata::get_lua_callback(state, "enchanter_can_use_on", id);
    if (!func) {
        debugmsg("Lua callback %s for `enchanter_can_use_on` does not exist. Defaulting to true",
                 id);
        return true;
    }
    auto params = state.lua.create_table();
    params["enchant_info_id"] = ench_id;
    params["item"] = &itm;
    sol::protected_function_result res = func(params);

    check_func_result(res);
    if (res.get_type() != sol::type::boolean) {
        debugmsg(
            "Lua callback %s for `enchanter_can_use_on` expected boolean result. Defaulting to true",
            id);
        return true;
    }
    return res.get<bool>();
}

} // namespace

namespace enchanter {
requirement_data total_requirements(const enchant_info& info) {
    return std::accumulate(
        info.requirements.begin(), info.requirements.end(), requirement_data(),
        [](const requirement_data& lhs, const std::pair<requirement_id, int>& rhs) {
            return lhs + (*rhs.first * rhs.second);
        });
}

std::vector<std::string> enchantment_info(
    const enchant_info& info, Character& crafter, int fold_width, item& itm) {
    units::volume vol = itm.base_volume();
    std::ostringstream oss = std::ostringstream();

    oss << string_format(_("Result Info:\n "));

    for (const std::string& str : info.to_enchant_with->get_effect_string(true)) {
        oss << "  " << str << "\n";
    }

    if (info.max_count > 0) {
        int count = itm.get_var<int>(info.count_var, 0);
        oss << string_format(_("Applications Left: %d"), info.max_count - count);
    }

    if (info.required_skills.size() > 0) {
        oss << "\n";
        oss << string_format(_("Required Skills:\n"));
        for (const auto& [skill_id, level] : info.required_skills) {
            oss << string_format("  <color_cyan>%s</color>: %d\n", skill_id->name(), level);
        }
    }

    oss << "\n";

    oss << string_format(
        _("Time to complete: <color_cyan>%s</color>\n"),
        to_string(info.time_to_enchant
                  * std::max(1, (info.volume_time_effect ? int(vol / info.volume_per_time) : 1))));


    if (info.requirements.size() > 0) {
        auto crafting_inv = crafter.crafting_inventory(true);
        auto real_req =
            total_requirements(info)
            * std::max(1, (info.volume_batch_effect ? int(vol / info.volume_per_batch) : 1));
        std::vector<std::string> tools =
            real_req.get_folded_tools_list(fold_width, c_white, crafting_inv, 1);
        std::vector<std::string> comps = real_req.get_folded_components_list(
            fold_width, c_white, crafting_inv, return_true<item>, 1, "");

        oss << "\n";
        for (const std::string& str : tools) { oss << str << "\n"; }
        for (const std::string& str : comps) { oss << str << "\n"; }
    }

    std::vector<std::string> result = foldstring(oss.str(), fold_width);
    return result;
}

int enchantment_selector_menu(std::vector<enchant_info> options, Character& user, item& itm) {
    units::volume vol = itm.base_volume();
    auto crafting_inv = user.crafting_inventory(true);
    int width = 0;
    int height = 0;
    int item_info_width = 0;
    catacurses::window w_ench;
    catacurses::window w_info;
    ui_adaptor ui;

    input_context ctxt("CRAFTING");
    ctxt.register_action("QUIT");
    ctxt.register_action("CONFIRM");
    ctxt.register_action("UP");
    ctxt.register_action("DOWN");
    ctxt.register_action("PAGE_UP", to_translation("Fast scroll up"));
    ctxt.register_action("PAGE_DOWN", to_translation("Fast scroll down"));
    ctxt.register_action("HELP_KEYBINDINGS");

    ui.on_screen_resize([&](ui_adaptor& ui) {
        width = TERMX / 2 - 2;
        height = TERMY - 2;

        w_ench = catacurses::newwin(TERMY, TERMX / 2, point(0, 0));
        w_info = catacurses::newwin(TERMY, TERMX / 2, point(TERMX / 2, 0));

        ui.position(point(0, 0), point(TERMX, TERMY));
    });
    ui.mark_resize();

    std::vector<std::string> names;
    for (const auto& ench_info : options) { names.push_back(ench_info.name); }
    int line = 0;
    int names_scroll_min = 0;
    int names_scroll_max = 0;
    int info_lines = 0;
    int info_scroll = 0;
    int info_scroll_min = 0;
    int info_scroll_max = 0;
    int num_options = names.size();
    ui.on_redraw([&](ui_adaptor& ui) {
        werase(w_ench);
        calcStartPos(names_scroll_min, line, height, num_options);
        names_scroll_max = std::min(num_options, names_scroll_min + height);
        for (int i = names_scroll_min; i < names_scroll_max; ++i) {
            const bool highlight = i == line;
            const point print_from(2, i - names_scroll_min + 1);
            nc_color col = highlight ? c_white : c_dark_gray;
            if (highlight) { ui.set_cursor(w_ench, print_from); }
            trim_and_print(w_ench, print_from, width, col, names[i]);
        }
        draw_scrollbar(w_ench, line, TERMY, num_options, point(width, 0));
        draw_border(w_ench, c_white, "Enchantment Options", c_white);
        wnoutrefresh(w_ench);
        werase(w_info);
        draw_scrollbar(w_info, line, height, num_options, point(width, 0));
        draw_border(w_info, c_white, "Enchantment Info", c_white);
        if (num_options != 0) {
            const std::vector<std::string>& info =
                enchantment_info(options[line], user, width, itm);
            const int total_lines = info.size();
            info_lines = total_lines;
            calcStartPos(info_scroll_min, info_scroll, height, total_lines);
            info_scroll_max = std::min(total_lines, info_scroll_min + height);
            for (int i = info_scroll_min; i < info_scroll_max; ++i) {
                auto dummy = c_white;
                trim_and_print(w_info, point(2, i - info_scroll_min + 1), width, c_white, info[i]);
            }

            if (total_lines > height) {
                scrollbar()
                    .offset_x(width)
                    .content_size(total_lines)
                    .viewport_pos(info_scroll)
                    .viewport_size(TERMY)
                    .apply(w_info);
            }
        }
        wnoutrefresh(w_info);
    });
    while (true) {
        ui_manager::redraw();
        const int scroll_info_lines = catacurses::getmaxy(w_info) - 4;
        const std::string action = ctxt.handle_input();
        if (action == "PAGE_UP") {
            info_scroll -= scroll_info_lines;
            if (info_scroll < 0) { info_scroll = info_lines; }
        } else if (action == "PAGE_DOWN") {
            info_scroll += scroll_info_lines;
            if (info_scroll > info_lines) { info_scroll = 0; }
        } else if (action == "DOWN") {
            line++;
            info_scroll = 0;
        } else if (action == "UP") {
            line--;
            info_scroll = 0;
        } else if (action == "CONFIRM") {
            auto info = options[line];
            auto total_reqs =
                total_requirements(info)
                * std::max(1, (info.volume_batch_effect ? int(vol / info.volume_per_batch) : 1));
            if (!total_reqs.can_make_with_inventory(crafting_inv, return_true<item>)) {
                popup("You have insufficient items to make this enchantment.");
            } else if (itm.get_var<int>(info.count_var, 0) >= info.max_count) {
                popup("You have already applied this enchantment too many times.");
            } else {
                return line;
            }
        } else if (action == "QUIT") {
            return -1;
        }
        if (line < 0) {
            line = num_options - 1;
        } else if (line >= static_cast<int>(num_options)) {
            line = 0;
        }
    }
    // This should never be reached really
    return line;
}

} // namespace enchanter

void enchant_info::deserialize(JsonIn& jin) {
    JsonObject obj = jin.get_object();
    mandatory(obj, false, "id", id);
    mandatory(obj, false, "name", name);
    mandatory(obj, false, "enchant", to_enchant_with);
    mandatory(obj, false, "time_to_enchant", time_to_enchant, time_reader());
    optional(obj, false, "volume_per_time", volume_per_time, volume_reader());
    optional(obj, false, "volume_time_effect", volume_time_effect, false);
    optional(obj, false, "volume_per_batch", volume_per_batch, volume_reader());
    optional(obj, false, "volume_batch_effect", volume_batch_effect, false);
    optional(obj, false, "applied_flag", applied_flag_id, flag_NULL);
    optional(obj, false, "count_var", count_var, "ENCH_COUNT");
    optional(obj, false, "max_count", max_count, -1);
    optional(obj, false, "can_make", can_make, "");
    optional(obj, false, "can_use_on", can_use_on, "");
    // Requirements
    if (obj.has_string("using")) {
        requirements = {{requirement_id(obj.get_string("using")), 1}};
    } else if (obj.has_array("using")) {
        requirements.clear();
        for (JsonArray cur : obj.get_array("using")) {
            requirements.emplace_back(requirement_id(cur.get_string(0)), cur.get_int(1));
        }
    } else {
        requirements.clear();
        // Construct a requirement to capture "components", "qualities", and
        // "tools" that might be listed.
        requirement_id req_id;
        int i = 0;
        do {
            req_id = requirement_id(string_format("inline_enchanter_requirements_%s_%d", name, i));
        } while (req_id.is_valid());
        requirement_data::load_requirement(obj, req_id);
        requirements.emplace_back(req_id, 1);
    }
    // Skills
    if (obj.has_array("skill_levels")) {
        for (JsonValue val : obj.get_array("skill_levels")) {
            JsonObject iobj = val.get_object();
            required_skills[skill_id(iobj.get_string("skill"))] = iobj.get_int("level");
        }
    }
}

void iexamine::enchanter(player& p, const tripoint_bub_ms& pos) {
    map& here = get_map();
    const furn_id& furn_id = here.furn(pos);
    if (furn_id->enchanter.size() == 0) { debugmsg("Enchanter iuse has no enchanter info"); }
    std::vector<enchant_info> valid_infos;
    for (enchant_info info : furn_id->enchanter) {
        bool quit = false;
        for (const auto& [skill_id, level] : info.required_skills) {
            if (p.get_skill_level(skill_id, false) < level) {
                quit = true;
                break;
            }
        }
        if (quit) { continue; }

        if (info.can_make != "" && !run_can_make_callback(info.can_make, info.id)) { continue; }

        valid_infos.push_back(info);
    }
    if (valid_infos.size() == 0) {
        popup("You are unable to make anything here yet.");
        return;
    }

    item* to_ench = g->inv_map_splice(
        [&](const item& e) {
            for (enchant_info info : valid_infos) {
                if (e.get_var<int>(info.count_var, 0) >= info.max_count) { continue; }
                if (info.can_use_on != ""
                    && !run_can_use_on_callback(info.can_use_on, info.id, e)) {
                    continue;
                }
                return true;
            }
            return false;
        },
        _("Enchant What?"), 1, _("You dont have a suitable item to enchant here"));

    if (!to_ench) { return; }

    std::vector<enchant_info> infos;
    for (enchant_info info : valid_infos) {
        if (info.can_use_on != "" && !run_can_use_on_callback(info.can_use_on, info.id, *to_ench)) {
            continue;
        }
        infos.push_back(info);
    }

    int index = enchanter::enchantment_selector_menu(infos, p, *to_ench);
    if (index != -1) {
        auto info = infos[index];
        int moves =
            to_moves<int>(info.time_to_enchant)
            * std::max(
                1,
                (info.volume_time_effect ? int(to_ench->base_volume() / info.volume_per_time) : 1));
        p.assign_activity(
            std::make_unique<player_activity>(std::make_unique<enchant_activity_actor>(
                *to_ench, furn_id.id(), info.id, to_moves<int>(info.time_to_enchant))));
    }
}
