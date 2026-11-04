#if defined(TILES)
#    include "character_preview.h"

#    include "avatar.h"
#    include "bionics.h"
#    include "cata_tiles.h"
#    include "character.h"
#    include "client/tiles/character_preview_observation.h"
#    include "color.h"
#    include "cursesport.h"
#    include "detached_ptr.h"
#    include "effect.h"
#    include "game.h"
#    include "hsv_color.h"
#    include "magic/magic.h"
#    include "output.h"
#    include "overlay_ordering.h"
#    include "profession.h"
#    include "rng.h"
#    include "sdltiles.h"
#    include "type_id.h"

#    include <algorithm>
#    include <cstdint>
#    include <ranges>
#    include <string_view>
#    include <utility>
#    include <vector>

namespace {
auto preview_observer = game_client::tiles::character_preview_observer{};

/// Native actor-origin output belongs to this temporary presentation actor, not
/// authoritative history. Global/native/Lua callback effects are not isolated here.
class preview_avatar final: public avatar {
public:
    using avatar::add_msg_if_player;
    using avatar::add_msg_player_or_npc;
    using avatar::add_msg_player_or_say;

    auto add_msg_if_player(const std::string& /*msg*/) const -> void override {}
    auto add_msg_if_player(const game_message_params& /*params*/, const std::string& /*msg*/) const
        -> void override {}
    auto add_msg_player_or_npc(
        const std::string& /*player_msg*/, const std::string& /*npc_msg*/) const -> void override {}
    auto add_msg_player_or_npc(
        const game_message_params& /*params*/, const std::string& /*player_msg*/,
        const std::string& /*npc_msg*/) const -> void override {}
    auto add_msg_player_or_say(const std::string& /*player_msg*/, const std::string& /*npc_speech*/)
        const -> void override {}
    auto add_msg_player_or_say(
        const game_message_params& /*params*/, const std::string& /*player_msg*/,
        const std::string& /*npc_speech*/) const -> void override {}
};

/// A stable presentation domain keyed by the native profession-item inputs.
/// Never allocate a simulation stream or depend on render count, zoom or clothing.
auto preview_seed(const avatar& actor) -> unsigned int {
    auto hash = std::uint64_t{14695981039346656037ULL};
    const auto append = [&](const std::string_view token) {
        for (const auto byte : token) {
            hash = (hash ^ static_cast<unsigned char>(byte)) * 1099511628211ULL;
        }
        hash *= 1099511628211ULL; // Separate tokens with a zero byte.
    };
    append(actor.prof.str());
    append(actor.male ? "male" : "female");
    auto traits =
        actor.get_mutations() | std::views::transform([](const auto& id) { return id.str(); })
        | std::ranges::to<std::vector>();
    std::ranges::sort(traits);
    for (const auto& trait : traits) { append(trait); }
    return rng_deterministic_child_seed(0x70726576U, {.stream = 0x70726576696577ULL, .id = hash});
}
} // namespace

namespace game_client::tiles {
auto set_character_preview_observer(character_preview_observer observer)
    -> character_preview_observer {
    return std::exchange(preview_observer, std::move(observer));
}
} // namespace game_client::tiles

class tiles_character_preview_window final: public character_preview_window {
public:
    auto init(Character* character) -> void override;
    auto prepare(const prepare_options& options) -> void override;
    auto zoom_in() -> void override;
    auto zoom_out() -> void override;
    auto toggle_clothes() -> void override;
    auto display() const -> void override;
    auto clear() const -> void override;
    auto clothes_showing() const -> bool override;

private:
    catacurses::window w_preview;
    point pos;
    int termx_pixels = 0;
    int termy_pixels = 0;
    const int MIN_ZOOM = 32;
    const int MAX_ZOOM = 128;
    const int DEFAULT_ZOOM = 128;
    int zoom = DEFAULT_ZOOM;
    int hide_below_ncols = 0;
    int ncols_width = 0;
    int nlines_width = 0;
    Character* character = nullptr;
    std::vector<detached_ptr<item>> clothes;
    std::vector<trait_id> spells;
    bool show_clothes = true;

    auto calc_character_pos() const -> point_bub_ms;
};

// @brief adapter to get access to protected functions of cata_tiles
// exclusively for use by character_preview ui
class char_preview_adapter: public cata_tiles {
public:
    static char_preview_adapter* convert(cata_tiles* ct) {
        return static_cast<char_preview_adapter*>(ct);
    }

    void display_avatar_preview_with_overlays(
        const avatar& ch, const point_bub_ms& p, bool with_clothing) {
        // Enter before construction and leave after overlay drawing and actor teardown.
        const auto presentation_rng = rng_deterministic_task_scope{preview_seed(ch)};
        std::string ent_name = ch.male ? "player_male" : "player_female";

        int height_3d = 0;
        int prev_height_3d = 0;
        if (ch.facing == FD_RIGHT) {
            const tile_search_params tile{ent_name, C_NONE, "", corner, 0};
            draw_from_id_string(
                tile, tripoint_bub_ms(p, 0), std::nullopt, std::nullopt, lit_level::BRIGHT, false,
                0, true, height_3d);
        } else if (ch.facing == FD_LEFT) {
            const tile_search_params tile{ent_name, C_NONE, "", corner, 4};
            draw_from_id_string(
                tile, tripoint_bub_ms(p, 0), std::nullopt, std::nullopt, lit_level::BRIGHT, false,
                0, true, height_3d);
        }

        auto get_overlay_color = [&]<typename T>(T&& arg) {
            using Decayed = std::remove_reference_t<T>;
            using PtrBase = std::remove_const_t<std::remove_pointer_t<Decayed>>;
            if constexpr (std::is_same_v<PtrBase, item>) {
                return get_item_color(*arg, g->m, tripoint_bub_ms::zero());
            } else if constexpr (std::is_same_v<PtrBase, effect>) {
                return get_effect_color(*arg, ch, g->m, tripoint_bub_ms::zero());
            } else if constexpr (std::is_same_v<PtrBase, bionic>) {
                return get_bionic_color(*arg, ch, g->m, tripoint_bub_ms::zero());
            } else if constexpr (std::is_same_v<PtrBase, mutation>) {
                return get_mutation_color(*arg, ch, g->m, tripoint_bub_ms::zero());
            } else {
                return color_tint_pair{std::nullopt, std::nullopt};
            }
        };

        auto should_override = [&]<typename T>(T&& arg) {
            auto check = [&](const mutation& mut) {
                mutation_branch branch = mut.first.obj();
                for (const std::string& mut_type : branch.types) {
                    auto controller = tileset_ptr->get_tint_controller(mut_type);
                    if (!controller.first.empty()) { return controller.second; }
                }
                for (const trait_flag_str_id& mut_flag : branch.flags) {
                    auto controller = tileset_ptr->get_tint_controller(mut_flag.str());
                    if (!controller.first.empty()) { return controller.second; }
                }
                return false;
            };
            using Decayed = std::remove_reference_t<T>;
            using PtrBase = std::remove_const_t<std::remove_pointer_t<Decayed>>;
            if constexpr (std::is_same_v<PtrBase, mutation>) { return check(*arg); }
            return false;
        };

        auto is_hair_style = [&]<typename T>(T&& arg) {
            auto check = [&](const mutation& mut) {
                if (mut.first.obj().types.contains("hair_style")) { return true; }
                return false;
            };
            using Decayed = std::remove_reference_t<T>;
            using PtrBase = std::remove_const_t<std::remove_pointer_t<Decayed>>;
            if constexpr (std::is_same_v<PtrBase, mutation>) { return check(*arg); }
            return false;
        };

        auto result = get_overlay_ids(ch, with_clothing);
        if (preview_observer) { preview_observer(result.temp_avatar, result.overlays); }
        for (const auto& [overlay_id, entry] : result.overlays) {
            tint_config overlay_bg_color = std::nullopt;
            tint_config overlay_fg_color = std::nullopt;
            std::string draw_id = overlay_id;
            bool found = false;

            if (!std::visit(should_override, entry)) {
                // Legacy hair color injection: try to find a tile with the hair color in the name
                if (std::visit(is_hair_style, entry)) {
                    for (const trait_id& other_mut : ch.get_mutations()) {
                        if (!other_mut.obj().types.contains("hair_color")) { continue; }
                        const std::string color_id = other_mut.str();
                        if (draw_id.find(color_id) != std::string::npos) { break; }
                        const size_t hair_pos = draw_id.find("hair_");
                        if (hair_pos == std::string::npos) { continue; }
                        const std::string prefix = draw_id.substr(0, hair_pos);
                        std::string suffix = draw_id.substr(hair_pos);
                        suffix = suffix.substr(suffix.find('_'));
                        const std::string new_id = prefix + color_id + suffix;
                        // draw_id is set to the resolved tile ID if found
                        found = find_overlay_looks_like(ch.male, new_id, draw_id);
                        break;
                    }
                }
            }

            if (!found) {
                auto pair = std::visit(get_overlay_color, entry);
                overlay_bg_color = pair.first;
                overlay_fg_color = pair.second;
                found = find_overlay_looks_like(ch.male, overlay_id, draw_id);
            }

            if (found) {
                int overlay_height_3d = prev_height_3d;
                if (ch.facing == FD_RIGHT) {
                    const tile_search_params tile{draw_id, C_NONE, "", corner, /*rota*/ 0};
                    draw_from_id_string(
                        tile, tripoint_bub_ms(p, 0), overlay_bg_color, overlay_fg_color,
                        lit_level::BRIGHT, false, 0, true, overlay_height_3d);
                } else if (ch.facing == FD_LEFT) {
                    const tile_search_params tile{draw_id, C_NONE, "", corner, /*rota*/ 4};
                    draw_from_id_string(
                        tile, tripoint_bub_ms(p, 0), overlay_bg_color, overlay_fg_color,
                        lit_level::BRIGHT, false, 0, true, overlay_height_3d);
                }
                height_3d = std::max(height_3d, overlay_height_3d);
            }
        }
    }

private:
    using overlay_entry = Character::overlay_entry;
    struct overlay_result {
        std::vector<overlay_entry> overlays;
        preview_avatar temp_avatar;
    };

    overlay_result get_overlay_ids(const avatar& av, bool with_clothing) {
        overlay_result result;
        std::multimap<int, overlay_entry> mutation_sorting;

        for (const auto& [eff_type, eff_by_part] : av.get_effects()) {
            const effect& eff = eff_by_part.begin()->second;
            if (eff.is_removed()) { continue; }
            result.overlays.emplace_back(overlay_entry{"effect_" + eff_type.str(), &eff});
        }

        for (const mutation& mut : av.my_mutations) {
            if (!mut.second.show_sprite) { continue; }
            std::string overlay_id = (mut.second.powered ? "active_" : "") + mut.first.str();
            int order = get_overlay_order_of_mutation(overlay_id);
            mutation_sorting.insert({order, overlay_entry{overlay_id, &mut}});
        }

        for (const bionic_id& bio : av.prof->CBMs()) { result.temp_avatar.add_bionic(bio); }
        for (const bionic& bio : *av.my_bionics) {
            if (!bio.id->included) { result.temp_avatar.add_bionic(bio.id); }
        }
        for (const bionic& bio : *result.temp_avatar.my_bionics) {
            if (!bio.show_sprite) { continue; }
            std::string overlay_id = (bio.powered ? "active_" : "") + bio.id.str();
            int order = get_overlay_order_of_mutation(overlay_id);
            mutation_sorting.insert({order, overlay_entry{overlay_id, &bio}});
        }

        for (auto& [order, entry] : mutation_sorting) {
            result.overlays.emplace_back(overlay_entry{"mutation_" + entry.id, entry.entry});
        }

        if (with_clothing) {
            static const flag_id json_flag_auto_wield("auto_wield");
            std::vector<itype_id> wielded_items;
            for (const auto& it : av.prof->items(av.male, av.get_mutations())) {
                if (it->has_flag(json_flag_auto_wield)) {
                    wielded_items.push_back(it->typeId());
                } else if (it->is_armor() && av.can_wear(*it).success()) {
                    result.temp_avatar.wear_item(item::spawn(*std::move(it)), false);
                }
            }
            for (const item* const& worn_item : result.temp_avatar.worn) {
                result.overlays.emplace_back(
                    overlay_entry{"worn_" + worn_item->typeId().str(), worn_item});
            }
            for (const itype_id& wielded : wielded_items) {
                result.overlays.emplace_back(
                    overlay_entry{"wielded_" + wielded.str(), std::monostate{}});
            }
        }
        return result;
    }
};

void tiles_character_preview_window::init(Character* character) { this->character = character; }


void tiles_character_preview_window::prepare(const prepare_options& options) {
    zoom = DEFAULT_ZOOM;
    tilecontext->set_draw_scale(zoom);
    termx_pixels = termx_to_pixel_value();
    termy_pixels = termy_to_pixel_value();
    this->hide_below_ncols = options.hide_below_ncols;

    // Trying to ensure that tile will fit in border
    const int win_width = options.ncols * termx_pixels;
    const int win_height = options.nlines * termy_pixels;
    int t_width = tilecontext->get_tile_width();
    int t_height = tilecontext->get_tile_height();
    while (zoom != MIN_ZOOM && (win_width < t_width || win_height < t_height)) {
        zoom_out();
        t_width = tilecontext->get_tile_width();
        t_height = tilecontext->get_tile_height();
    }

    // Final size of character preview window
    const int box_ncols = t_width / termx_pixels + 4;
    const int box_nlines = t_height / termy_pixels + 3;

    // Setting window just a little bit more than a tile itself
    point start;
    switch (options.orientation->type) {
        case (TOP_LEFT):
            start = point_zero;
            break;
        case (TOP_RIGHT):
            start = point{TERMX - box_ncols, 0};
            break;
        case (BOTTOM_LEFT):
            start = point{0, TERMY - box_nlines};
            break;
        case (BOTTOM_RIGHT):
            start = point{TERMX - box_ncols, TERMY - box_nlines};
            break;
    }

    start.x += options.orientation->margin.left - options.orientation->margin.right;
    start.y += options.orientation->margin.top - options.orientation->margin.bottom;
    w_preview = catacurses::newwin(box_nlines, box_ncols, start);
    ncols_width = box_ncols;
    nlines_width = box_nlines;
    pos = start;
}

auto tiles_character_preview_window::calc_character_pos() const -> point_bub_ms {
    const int t_width = tilecontext->get_tile_width();
    const int t_height = tilecontext->get_tile_height();
    return point_bub_ms(
        pos.x * termx_pixels + ncols_width * termx_pixels / 2 - t_width / 2,
        pos.y * termy_pixels + nlines_width * termy_pixels / 2 - t_height / 2);
}

void tiles_character_preview_window::zoom_in() {
    zoom = zoom * 2 % (MAX_ZOOM * 2);
    if (zoom == 0) { zoom = MIN_ZOOM; }
    tilecontext->set_draw_scale(zoom);
}

void tiles_character_preview_window::zoom_out() {
    zoom = zoom / 2;
    if (zoom < MIN_ZOOM) { zoom = MAX_ZOOM; }
    tilecontext->set_draw_scale(zoom);
}

void tiles_character_preview_window::toggle_clothes() { show_clothes = !show_clothes; }

void tiles_character_preview_window::display() const {
    // If device width is too small - ignore display
    if (TERMX - ncols_width < hide_below_ncols) { return; }

    // Drawing UI across character tile
    werase(w_preview);
    draw_border(w_preview, BORDER_COLOR, _("CHARACTER PREVIEW"), BORDER_COLOR);
    wnoutrefresh(w_preview);

    // Drawing character itself
    const auto pos = calc_character_pos();
    // tilecontext->display_character( *character, pos );
    char_preview_adapter::convert(&*tilecontext)
        ->display_avatar_preview_with_overlays(*(character->as_avatar()), pos, show_clothes);
}

void tiles_character_preview_window::clear() const {
    tilecontext->set_draw_scale(DEFAULT_TILESET_ZOOM);
}

auto tiles_character_preview_window::clothes_showing() const -> bool { return !show_clothes; }

namespace game_client {
auto install_tiles_character_preview() -> void {
    set_character_preview_factory([]() -> std::unique_ptr<character_preview_window> {
        return std::make_unique<tiles_character_preview_window>();
    });
}
} // namespace game_client

#endif // TILES
