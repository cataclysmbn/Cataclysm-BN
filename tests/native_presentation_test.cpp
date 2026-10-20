#include "avatar.h"
#include "ballistics.h"
#include "cached_options.h"
#include "calendar.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "client_animation.h"
#include "color.h"
#include "dispersion.h"
#include "explosion.h"
#include "explosion_queue.h"
#include "explosion_test.h"
#include "game.h"
#include "item.h"
#include "itype.h"
#include "map/map.h"
#include "map_helpers.h"
#include "map_perception.h"
#include "monster.h"
#include "mtype.h"
#include "options_helpers.h"
#include "output.h"
#include "projectile.h"
#include "ranged.h"
#include "rng.h"
#include "skill.h"
#include "sounds.h"
#include "state_helpers.h"
#include "string_formatter.h"
#include "weather/weather.h"

#include <algorithm>
#include <functional>
#include <locale>
#include <memory>
#include <ranges>
#include <sstream>
#include <string>
#include <vector>

namespace {

using trace_t = std::vector<std::string>;

constexpr auto you_at = tripoint_bub_ms(62, 62, 0);

auto rng_state() -> std::string {
    auto stream = std::ostringstream{};
    stream.imbue(std::locale::classic());
    stream << rng_get_engine();
    return stream.str();
}

auto at(const tripoint_bub_ms& p) -> std::string {
    return string_format("(%d,%d,%d)", p.x(), p.y(), p.z());
}

/// Logs each native effect call by value, with the gameplay RNG state at the call.
class recording_animation final: public game_client::animation_service {
public:
    explicit recording_animation(trace_t& trace): trace(trace) {}

    auto draw_explosion(const game_client::explosion_animation_options& options) -> bool override {
        return log(string_format(
            "explosion at=%s r=%d color=%s name=%s", at(options.position), options.radius,
            string_from_color(options.color), options.name));
    }
    auto draw_custom_explosion(const game_client::custom_explosion_animation_options& options)
        -> bool override {
        return log(string_format(
            "custom_explosion at=%s cells=%d layers=%d name=%s", at(options.position),
            options.area->size(), options.layers->size(), options.name));
    }
    auto draw_bullet(const game_client::bullet_animation_options& options) -> bool override {
        return log(string_format(
            "bullet at=%s i=%d glyph=%c sprite=%s", at(options.position), options.index,
            options.bullet, options.custom_sprite));
    }
    auto draw_bullet_trajectories(const game_client::bullet_trajectories_animation_options& options)
        -> bool override {
        const auto& shots = *options.trajectories;
        auto paths = std::string{};
        for (const auto& path : shots.trajectories) {
            paths +=
                string_format(" path=%d:%s", path.size(), path.empty() ? "-" : at(path.back()));
        }
        return log(string_format(
            "trajectories glyph=%c line=%d sprite=%s%s", shots.bullet,
            static_cast<int>(shots.draw_as_line), shots.custom_sprite, paths));
    }
    auto draw_hit_mon(const game_client::hit_mon_animation_options& options) -> bool override {
        return log(string_format(
            "hit_mon at=%s mon=%s dead=%d", at(options.position), options.target->type->id.str(),
            static_cast<int>(options.dead)));
    }
    auto draw_hit_player(const game_client::hit_player_animation_options& options)
        -> bool override {
        return log(string_format("hit_player at=%s dam=%d", at(options.position), options.damage));
    }
    auto draw_line(const game_client::line_animation_options& options) -> bool override {
        return log(string_format(
            "line at=%s points=%d no_reveal=%d", at(options.position), options.points->size(),
            static_cast<int>(options.no_reveal)));
    }
    auto draw_trail_line(const game_client::line_animation_options& options) -> bool override {
        return log(string_format(
            "trail_line at=%s points=%d", at(options.position), options.points->size()));
    }
    auto draw_line_of(const draw_sprite_line_options& options) -> bool override {
        return log(string_format(
            "line_of at=%s points=%d first=%s sprite=%s rotate=%d", at(options.p),
            options.points.size(), options.points.empty() ? "-" : at(options.points.front()),
            options.sprite, static_cast<int>(options.rotate)));
    }
    auto draw_cone_aoe(const game_client::cone_aoe_animation_options& options) -> bool override {
        return log(
            string_format("cone_aoe at=%s cells=%d", at(options.origin), options.coverage->size()));
    }

private:
    auto log(const std::string& call) -> bool {
        trace.push_back(call + " rng=" + rng_state());
        return true;
    }
    trace_t& trace;
};

/// Daylight over open floor with the avatar at `you_at`, so every square around it is seen.
auto lit_floor() -> void {
    clear_all_state();
    build_test_map(ter_id("t_floor"));
    calendar::turn = calendar::turn_zero + 12_hours;
    get_weather().weather_id = weather_type_id("clear");
    g->reset_light_level();
    sounds::reset_sounds();
    auto& you = get_avatar();
    you.setpos(you_at);
    // Earlier runs leave a spent focus pool and stale maximum HP, which shift later rolls.
    you.focus_pool = 100;
    you.recalc_hp();
    get_map().build_map_cache(0);
    map_perception::acquire();
}

/// Runs `action` from a fixed seed with animations on outside test mode, recording the native
/// effect calls, each explosion redraw, and the final RNG state.
auto trace_of(const std::string& delay, const std::function<auto()->void>& action) -> trace_t {
    auto trace = trace_t{};
    const override_option animations("ANIMATIONS", "true");
    const override_option projectiles("ANIMATION_PROJECTILES", "true");
    const override_option sct("ANIMATION_SCT", "true");
    const override_option stepwise("BULLETS_AS_LASERS", "false");
    const override_option old_explosions("OLD_EXPLOSIONS", "false");
    const override_option skip("SKIP_EXPLOSION_ANIMATION_AFTER", "0");
    const override_option pace("ANIMATION_DELAY", delay);
    // The clock bypasses test mode for explosion pacing and charges no wall time.
    const auto clock = explosion_handler::testing::scoped_explosion_clock({
        .clock_read_ms = 1,
        .redraw_ms = 0,
        .bypass_test_mode = true,
        .render = [&trace] { trace.push_back("redraw rng=" + rng_state()); },
    });
    const auto live = restore_on_out_of_scope(test_mode);
    test_mode = false;
    game_client::set_animation(std::make_unique<recording_animation>(trace));
    const auto uninstall = on_out_of_scope([] {
        game_client::set_animation(nullptr);
        explosion_handler::get_explosion_queue().clear();
        SCT.vSCT.clear();
    });
    rng_set_engine_seed(424242);
    action();
    trace.push_back("end rng=" + rng_state());
    return trace;
}

auto is_redraw(const std::string& call) -> bool { return call.starts_with("redraw "); }

auto without_redraws(const trace_t& trace) -> trace_t {
    return trace | std::views::filter(std::not_fn(is_redraw)) | std::ranges::to<std::vector>();
}

auto bullet_projectile() -> projectile {
    auto proj = projectile{};
    proj.speed = 1000;
    proj.range = 20;
    proj.impact.add_damage(DT_BULLET, 10);
    return proj;
}

auto shoot(projectile proj, const tripoint_bub_ms& target) -> void {
    auto& you = get_avatar();
    projectile_attack(proj, you_at, target, dispersion_sources{}, &you, &you.primary_weapon());
}

auto single_shot() -> void {
    spawn_test_monster("mon_zombie", tripoint_bub_ms(68, 62, 0));
    shoot(bullet_projectile(), tripoint_bub_ms(68, 62, 0));
}

auto line_shot() -> void {
    spawn_test_monster("mon_zombie", tripoint_bub_ms(68, 62, 0));
    auto proj = bullet_projectile();
    proj.add_effect(ammo_effect_str_id("DRAW_AS_LINE"));
    shoot(proj, tripoint_bub_ms(68, 62, 0));
}

auto shotgun() -> void {
    spawn_test_monster("mon_zombie", tripoint_bub_ms(66, 62, 0));
    auto& you = get_avatar();
    you.set_skill_level(skill_id("gun"), 10);
    you.set_skill_level(skill_id("shotgun"), 10);
    auto gun = item::spawn("test_shotgun");
    gun->ammo_set(itype_id("test_buckshot_8"));
    you.wield(std::move(gun));
    REQUIRE(ranged::fire_gun(you, tripoint_bub_ms(66, 62, 0), 1) == 1);
}

auto thrown_grenade() -> void {
    const auto landing = tripoint_bub_ms(70, 62, 0);
    spawn_test_monster("mon_zombie", landing + tripoint_east);
    auto& you = get_avatar();
    auto& here = get_map();
    you.set_moves(1000);
    auto grenade = item::spawn("grenade");
    REQUIRE(grenade->type->invoke(you, *grenade, you_at) == 0);
    ranged::throw_item(you, landing, std::move(grenade), std::nullopt);
    for (auto turn = 0; turn < 7; ++turn) {
        calendar::turn += 1_turns;
        here.process_items();
        explosion_handler::get_explosion_queue().execute();
    }
}

auto acidsplash() -> void {
    auto& target = spawn_test_monster("mon_zombie_corrosive", you_at + tripoint_east);
    auto& you = get_avatar();
    you.set_skill_level(skill_id("melee"), 10);
    you.set_skill_level(skill_id("unarmed"), 10);
    you.melee_attack(target, false);
}

auto bounce() -> void {
    spawn_test_monster("mon_zombie", tripoint_bub_ms(66, 62, 0));
    spawn_test_monster("mon_zombie", tripoint_bub_ms(66, 65, 0));
    auto proj = bullet_projectile();
    proj.add_effect(ammo_effect_str_id("BOUNCE"));
    shoot(proj, tripoint_bub_ms(66, 62, 0));
}

auto flashbang() -> void {
    spawn_test_monster("mon_zombie", tripoint_bub_ms(67, 63, 0));
    explosion_handler::flashbang(tripoint_bub_ms(66, 62, 0), false, "explosion");
    explosion_handler::get_explosion_queue().execute();
}

auto chain_reaction() -> void {
    auto& here = get_map();
    const auto center = tripoint_bub_ms(72, 62, 0);
    here.ter_set(center + tripoint_rel_ms(2, 0, 0), ter_id("test_t_clock_chain_wall"));
    here.build_map_cache(0);
    explosion_handler::explosion(center, {.damage = 50, .radius = 4.0f}, nullptr);
    auto& queue = explosion_handler::get_explosion_queue();
    queue.execute();
    REQUIRE(queue.get_count() == 2);
}

struct trace_case {
    std::string name;
    std::function<auto()->void> action;
    trace_t golden;
    /// Explosion redraws the effect paces at ANIMATION_DELAY 10; none at 0.
    int redraws = 0;
};

auto trace_cases() -> const std::vector<trace_case>& { // *NOPAD*
    static const auto cases = std::vector<trace_case>{
        {.name = "single shot",
         .action = single_shot,
         .golden =
             {
                 "bullet at=(63,62,0) i=1 glyph=* sprite= rng=1394291270",
                 "bullet at=(64,62,0) i=2 glyph=* sprite= rng=1394291270",
                 "bullet at=(65,62,0) i=3 glyph=* sprite= rng=1394291270",
                 "bullet at=(66,62,0) i=4 glyph=* sprite= rng=1394291270",
                 "bullet at=(67,62,0) i=5 glyph=* sprite= rng=1394291270",
                 "bullet at=(68,62,0) i=6 glyph=* sprite= rng=1394291270",
                 "end rng=182054921",
             }},
        {.name = "line shot",
         .action = line_shot,
         .golden =
             {
                 "line_of at=(68,62,0) points=18 first=(63,62,0) sprite= rotate=0 rng=182054921",
                 "end rng=182054921",
             }},
        {.name = "shotgun",
         .action = shotgun,
         .golden =
             {
                 "trajectories glyph=* line=0 sprite= path=9:(70,58,0) path=9:(70,58,0) path=9:(70,58,0) path=9:(70,58,0) path=9:(70,58,0) path=9:(70,58,0) path=9:(70,58,0) path=9:(70,58,0) rng=1404214305",
                 "end rng=1932027252",
             }},
        {.name = "thrown grenade",
         .action = thrown_grenade,
         .golden =
             {
                 "bullet at=(63,62,0) i=1 glyph=* sprite= rng=2112178433",
                 "bullet at=(64,62,0) i=2 glyph=* sprite= rng=2112178433",
                 "bullet at=(65,62,0) i=3 glyph=* sprite= rng=2112178433",
                 "bullet at=(66,62,0) i=4 glyph=* sprite= rng=2112178433",
                 "bullet at=(67,62,0) i=5 glyph=* sprite= rng=2112178433",
                 "bullet at=(68,62,0) i=6 glyph=* sprite= rng=2112178433",
                 "bullet at=(69,62,0) i=7 glyph=* sprite= rng=2112178433",
                 "bullet at=(70,62,0) i=8 glyph=* sprite= rng=2112178433",
                 "end rng=1233166100",
             },
         .redraws = 252},
        {.name = "acidsplash",
         .action = acidsplash,
         .golden =
             {
                 "hit_player at=(62,62,0) dam=2 rng=1598934436",
                 "hit_player at=(62,62,0) dam=1 rng=1456065311",
                 "hit_player at=(62,62,0) dam=1 rng=336515966",
                 "end rng=1980845073",
             }},
        {.name = "bounce",
         .action = bounce,
         .golden =
             {
                 "bullet at=(63,62,0) i=1 glyph=* sprite= rng=648694822",
                 "bullet at=(64,62,0) i=2 glyph=* sprite= rng=648694822",
                 "bullet at=(65,62,0) i=3 glyph=* sprite= rng=648694822",
                 "bullet at=(66,62,0) i=4 glyph=* sprite= rng=648694822",
                 "bullet at=(66,63,0) i=1 glyph=* sprite= rng=1146677107",
                 "end rng=1146677107",
             }},
        {.name = "flashbang",
         .action = flashbang,
         .golden =
             {
                 "explosion at=(66,62,0) r=8 color=c_white name=explosion rng=1101870993",
                 "end rng=648694822",
             }},
        {.name = "chain reaction",
         .action = chain_reaction,
         .golden =
             {
                 "end rng=1967261782",
             },
         .redraws = 479},
    };
    return cases;
}

} // namespace

TEST_CASE("native effects call the animation service in a pinned order", "[native_presentation]") {
    for (const auto& entry : trace_cases()) {
        CAPTURE(entry.name);
        lit_floor();
        CHECK(trace_of("0", entry.action) == entry.golden);
    }
}

TEST_CASE("a slower animation delay only adds explosion redraws", "[native_presentation]") {
    for (const auto& entry : trace_cases()) {
        CAPTURE(entry.name);
        lit_floor();
        const auto trace = trace_of("10", entry.action);
        CHECK(without_redraws(trace) == entry.golden);
        CHECK(std::ranges::count_if(trace, is_redraw) == entry.redraws);
    }
}
