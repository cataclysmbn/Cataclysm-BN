#include "reload_ui.h"

#include "ammo.h"
#include "cata_utility.h"
#include "catacharset.h"
#include "cursesdef.h"
#include "damage.h"
#include "flag.h"
#include "input.h"
#include "item.h"
#include "itype.h"
#include "messages.h"
#include "output.h"
#include "player.h"
#include "ranged.h"
#include "skill.h"
#include "string_formatter.h"
#include "string_utils.h"
#include "translations.h"
#include "ui.h"
#include "uistate.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace {
const auto itype_battery = itype_id("battery");
const auto skill_throw = skill_id("throw");

struct reload_callback_context {
    std::vector<item_reload_option>& options;
    std::function<std::string(int)> draw_row;
    int opening_key;
    int fallback_index;
    bool can_partial_reload;
};

class reload_callback: public uilist_callback {
public:
    explicit reload_callback(reload_callback_context context): context(std::move(context)) {}

    auto key(const input_context&, const input_event& event, int index, uilist* menu)
        -> bool override {
        const auto current_key = event.get_first_input();
        if (context.fallback_index != -1 && current_key == context.opening_key) {
            menu->ret = context.fallback_index;
            return true;
        }
        if (index < 0 || index >= static_cast<int>(context.options.size())) { return false; }
        auto& selected = context.options[index];
        switch (current_key) {
            case KEY_LEFT:
                if (context.can_partial_reload) {
                    selected.qty(selected.qty() - 1);
                    menu->entries[index].txt = context.draw_row(index);
                }
                return true;

            case KEY_RIGHT:
                if (context.can_partial_reload) {
                    selected.qty(selected.qty() + 1);
                    menu->entries[index].txt = context.draw_row(index);
                }
                return true;
        }
        return false;
    }

private:
    reload_callback_context context;
};

auto query_menu(const player& who, item& base, std::vector<item_reload_option> options)
    -> item_reload_option {
    auto menu = uilist{};
    menu.text = string_format(
        base.is_container() ? _("Refill %s")
        : base.has_flag(flag_RELOAD_AND_SHOOT)
            ? _("Select ammo for %s")
            : _("Reload %s"),
        base.tname());

    // Construct item names.
    auto names = std::vector<std::string>{};
    std::ranges::
        transform(options, std::back_inserter(names), [&](const item_reload_option& option) {
            const auto ammo_color = [&](const std::string& name) {
                return base.is_gun() && option.ammo->ammo_data()
                            && !base.ammo_types().contains(option.ammo->ammo_data()->ammo->type)
                         ? colorize(name, c_dark_gray)
                         : name;
            };
            if (option.ammo->is_magazine() && option.ammo->ammo_data()) {
                if (option.ammo->ammo_current() == itype_battery) {
                    // This battery ammunition is a pseudo-object representing stored charge.
                    //~ battery storage (charges)
                    return string_format(
                        pgettext("magazine", "%1$s (%2$d)"), option.ammo->type_name(),
                        option.ammo->ammo_remaining());
                }
                //~ magazine with ammo (count)
                return ammo_color(string_format(
                    pgettext("magazine", "%1$s with %2$s (%3$d)"), option.ammo->type_name(),
                    option.ammo->ammo_data()->nname(option.ammo->ammo_remaining()),
                    option.ammo->ammo_remaining()));
            }
            if (option.ammo->is_container()
                || (option.ammo->is_ammo_container() && who.is_worn(*option.ammo))) {
                // Worn ammunition containers are named by their contents; location is updated
                // below.
                return option.ammo->contents.front().display_name();
            }
            return ammo_color(
                (who.ammo_location && who.ammo_location == option.ammo ? "* " : "")
                + option.ammo->display_name());
        });

    // Get location descriptions.
    auto locations = std::vector<std::string>{};
    std::ranges::
        transform(options, std::back_inserter(locations), [&](const item_reload_option& option) {
            const auto is_ammo_container = option.ammo->is_ammo_container();
            if (is_ammo_container || option.ammo->is_container()) {
                if (is_ammo_container && who.is_worn(*option.ammo)) {
                    return option.ammo->type_name();
                }
                return string_format(
                    _("%s, %s"), option.ammo->type_name(),
                    option.ammo->describe_location(who.as_player()));
            }
            return option.ammo->describe_location(who.as_player());
        });

    // Pad elements to match the longest member and return the resulting width.
    const auto pad = [](std::vector<std::string>& values, int width, int trailing) {
        for (const auto& value : values) {
            width = std::max(width, utf8_width(value, true) + trailing);
        }
        for (auto& value : values) { value += std::string(width - utf8_width(value, true), ' '); }
        return width;
    };

    // Pad the first column, including four trailing spaces.
    auto width = pad(names, utf8_width(menu.text, true), 6);
    // Add space for UI hotkeys.
    menu.text.insert(0, 2, ' ');
    menu.text += std::string(width + 2 - utf8_width(menu.text, true), ' ');

    // Pad the location similarly, excluding the leading "| " and trailing space.
    width = pad(locations, utf8_width(_("| Location ")) - 3, 6);
    menu.text += _("| Location ");
    menu.text += std::string(width + 3 - utf8_width(_("| Location ")), ' ');

    menu.text += _("| Amount  ");
    menu.text += _("| Moves   ");
    // Only show ammunition statistics for guns and magazines.
    if (base.is_gun() || base.is_magazine()) { menu.text += _("| Damage   | Pierce   "); }

    const auto draw_row = [&](int index) {
        const auto& selected = options[index];
        auto row = string_format("%s| %s |", names[index], locations[index]);
        row += string_format(
            (selected.ammo->is_ammo() || selected.ammo->is_ammo_container())
                ? " %-7d |"
                : "         |",
            selected.qty());
        row += string_format(" %-7d ", selected.moves());

        if (base.is_gun() || base.is_magazine()) {
            const auto* ammo =
                selected.ammo->is_ammo_container()
                    ? selected.ammo->contents.front().ammo_data()
                    : selected.ammo->ammo_data();
            if (ammo) {
                const auto& damage = ammo->ammo->damage;
                const auto& damage_unit = damage.damage_units.front();
                if (damage_unit.damage_multiplier != 1.0f) {
                    const auto damage_amount = damage_unit.amount;
                    row += string_format(
                        "| %-3d*%3d%% ", static_cast<int>(damage_amount),
                        clamp(static_cast<int>(damage_unit.damage_multiplier * 100), 0, 999));
                } else {
                    auto throwing_damage = 0.0f;
                    if (base.gun_skill() == skill_throw) {
                        auto& temporary = *item::spawn_temporary(item(ammo));
                        throwing_damage += ranged::throw_damage(
                            temporary, who.get_skill_level(skill_throw), who.get_str());
                    }
                    const auto damage_amount = std::max(damage.total_damage(), throwing_damage);
                    row += string_format("| %-8d ", static_cast<int>(damage_amount));
                }
                if (damage_unit.res_mult != 1.0f) {
                    row += string_format(
                        "| %-3d/%3d%%", static_cast<int>(damage_unit.res_pen),
                        static_cast<int>(100 * damage_unit.res_mult));
                } else {
                    row += string_format("| %-8d", static_cast<int>(damage_unit.res_pen));
                }
            } else {
                row += "|          |          ";
            }
        }
        return row;
    };

    const auto base_ammo_type = ammotype(base.ammo_default().str());
    const auto last_ammo = uistate.lastreload[base_ammo_type];
    // Keep the key that opened this menu for hotkey assignment.
    const auto opening_key = inp_mngr.get_previously_pressed_key();
    auto hotkeys = reload_ui::prepare_hotkeys(last_ammo, opening_key);

    for (auto index = 0; index < static_cast<int>(options.size()); ++index) {
        const auto& ammo =
            options[index].ammo->is_ammo_container()
                ? options[index].ammo->contents.front()
                : *options[index].ammo;
        // If the ammo is in the player's possession, use its inventory letter or that of the
        // first parent container that has one.
        char inherited_hotkey = -1;
        if (who.has_item(ammo)) {
            if (ammo.invlet) {
                inherited_hotkey = ammo.invlet;
            } else {
                for (const auto parent : who.parents(ammo)) {
                    if (parent->invlet) {
                        inherited_hotkey = parent->invlet;
                        break;
                    }
                }
            }
        }
        const auto hotkey = reload_ui::assign_hotkey(
            hotkeys,
            {.ammo_type = ammo.typeId(), .inherited_hotkey = inherited_hotkey, .index = index});
        menu.addentry(index, true, hotkey, draw_row(index));
    }

    auto callback = reload_callback(
        {.options = options,
         .draw_row = draw_row,
         .opening_key = hotkeys.opening_key,
         .fallback_index = hotkeys.fallback_index,
         .can_partial_reload = !base.has_flag(flag_RELOAD_ONE)});
    menu.callback = &callback;
    menu.query();
    if (menu.ret < 0 || static_cast<size_t>(menu.ret) >= options.size()) {
        who.add_msg_if_player(m_info, _("Never mind."));
        return item_reload_option();
    }

    const auto* selected = options[menu.ret].ammo;
    uistate.lastreload[ammotype(base.ammo_default().str())] =
        selected->is_ammo_container() ? selected->contents.front().typeId() : selected->typeId();
    return options[menu.ret];
}

auto present(const player& who, item& base, reload_selection::selection_result result)
    -> item_reload_option {
    switch (result.outcome) {
        case reload_selection::selection_outcome::automatic:
            return result.selected;

        case reload_selection::selection_outcome::interaction_required:
            return query_menu(who, base, std::move(result.options));

        case reload_selection::selection_outcome::empty_supplied:
            who.add_msg_if_player(m_info, _("Never mind."));
            break;

        case reload_selection::selection_outcome::missing_magazine:
            who.add_msg_if_player(
                m_info, _("You need a compatible magazine to reload the %s!"), base.tname());
            break;

        case reload_selection::selection_outcome::nothing_to_reload:
            who.add_msg_if_player(m_info, _("Nothing to reload!"));
            break;

        case reload_selection::selection_outcome::missing_ammunition: {
            auto name = std::string{};
            if (base.ammo_data()) {
                name = base.ammo_data()->nname(1);
            } else if (base.is_watertight_container()) {
                name = base.is_container_empty() ? "liquid" : base.contents.front().tname();
            } else if (base.is_container()) {
                name = base.is_container_empty() ? "items" : base.contents.front().tname();
            } else {
                name = enumerate_as_string(
                    base.ammo_types().begin(), base.ammo_types().end(),
                    [](const ammotype& ammo_type) { return ammo_type->name(); },
                    enumeration_conjunction::none);
            }
            who.add_msg_if_player(
                m_info, _("You don't have any %s to reload your %s!"), name, base.tname());
            break;
        }
    }
    return item_reload_option();
}
} // namespace

namespace reload_ui {
auto select_ammo(const player& who, item& base, reload_selection::selection_options options)
    -> item_reload_option {
    return present(who, base, reload_selection::prepare(who, base, options));
}

auto select_ammo(const player& who, item& base, std::vector<item_reload_option> options)
    -> item_reload_option {
    return present(who, base, reload_selection::prepare(who, std::move(options)));
}

auto prepare_hotkeys(const itype_id& last_ammo, int opening_key) -> hotkey_state {
    auto state = hotkey_state{
        .last_ammo = last_ammo,
        .opening_key = opening_key,
        .opening_key_bound = false,
        .fallback_index = 0};
    // If the opening key is RETURN, don't use it to override a hotkey.
    if (opening_key == '\n') {
        state.opening_key_bound = true;
        state.fallback_index = -1;
    }
    return state;
}

auto assign_hotkey(hotkey_state& state, const hotkey_row& row) -> char {
    auto hotkey = row.inherited_hotkey;
    if (state.last_ammo == row.ammo_type) {
        // If this is the first occurrence of the most recently used ammo type and no inventory
        // letter was inherited, use the key that opened this prompt.
        if (!state.opening_key_bound && hotkey == -1) {
            hotkey = state.opening_key;
            state.opening_key_bound = true;
        }
        // Pressing the opening key defaults to the first entry of the compatible type.
        if (!state.opening_key_bound) {
            state.fallback_index = row.index;
            state.opening_key_bound = true;
        }
    }
    // Prevent the fallback from being used when the opening key is already bound.
    if (hotkey == state.opening_key) {
        state.opening_key_bound = true;
        state.fallback_index = -1;
    }
    return hotkey;
}
} // namespace reload_ui
