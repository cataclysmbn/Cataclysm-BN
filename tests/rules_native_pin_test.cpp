#if defined(CATA_MCP)

#    include "catch/catch.hpp"
#    include "client_command.h"
#    include "client_input.h"
#    include "client_memory.h"
#    include "client_memory_scope.h"
#    include "color.h"
#    include "cursesdef.h"
#    include "cursesport.h"
#    include "distraction_manager.h"
#    include "game_constants.h"
#    include "input.h"
#    include "output.h"
#    include "path_info.h"
#    include "state_helpers.h"

#    include <filesystem>
#    include <format>
#    include <fstream>
#    include <map>
#    include <string>
#    include <vector>

/// Pins what the native distraction manager and colour manager print, so that publishing them to
/// the engine client leaves the player's screens byte-equal: every row with its text and colour
/// runs.
namespace {

struct screen_guard {
    const game_client::memory::scoped_state memory;
    bool old_test_mode = test_mode;
    int old_termx = TERMX;
    int old_termy = TERMY;
    int old_width = FULL_SCREEN_WIDTH;
    int old_height = FULL_SCREEN_HEIGHT;

    screen_guard() {
        clear_all_state();
        test_mode = false;
        TERMX = 100;
        TERMY = 40;
        FULL_SCREEN_WIDTH = 80;
        FULL_SCREEN_HEIGHT = 24;
        game_client::memory::resize(TERMX, TERMY);
        catacurses::stdscr = catacurses::newwin(TERMY, TERMX, point_zero);
        catacurses::newscr = catacurses::newwin(TERMY, TERMX, point_zero);
    }

    ~screen_guard() {
        game_client::memory::set_input_provider({});
        TERMX = old_termx;
        TERMY = old_termy;
        FULL_SCREEN_WIDTH = old_width;
        FULL_SCREEN_HEIGHT = old_height;
        test_mode = old_test_mode;
    }
};

auto act(const std::string& action) -> input_event {
    auto input = game_client::input_command{};
    input.action = action;
    const auto result =
        game_client::resolve_input_command(input, game_client::memory::screen_size());
    REQUIRE(result.has_value());
    return *result;
}

/// Each non-blank row as `<fg>` or `<fg:bg>` runs and text, split where the colours change.
auto rendered() -> std::vector<std::string> {
    const auto shot = game_client::memory::snapshot();
    auto rows = std::vector<std::string>{};
    for (auto y = 0; y < shot.height; ++y) {
        auto line = std::string{};
        auto colors = std::string{};
        for (auto x = 0; x < shot.width; ++x) {
            const auto& cell = shot.cells[static_cast<std::size_t>(y * shot.width + x)];
            if (cell.text == " ") {
                line += ' ';
                continue;
            }
            const auto now =
                cell.background == 0
                    ? std::format("<{}>", cell.foreground)
                    : std::format("<{}:{}>", cell.foreground, cell.background);
            if (now != colors) {
                colors = now;
                line += now;
            }
            line += cell.text;
        }
        while (!line.empty() && line.back() == ' ') { line.pop_back(); }
        if (!line.empty()) { rows.push_back(line); }
    }
    return rows;
}

/// The recorded screens, by name.
auto recorded() -> const std::map<std::string, std::vector<std::string>>& {
    static const auto screens = std::map<std::string, std::vector<std::string>>{
        {"distractions:0",
         {
             R"pin(          <7>┌─────────────────────────────<9>Distractions manager<7>─────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│ <15:4>Noise                                                      <7>│ <10>Enabled         <7>│)pin",
             R"pin(          <7>│ <15>Pain                                                       <7>│ <10>Enabled         <7>│)pin",
             R"pin(          <7>│ <15>Attacked                                                   <7>│ <10>Enabled         <7>│)pin",
             R"pin(          <7>│ <15>Hostile Spotted                                            <7>│ <10>Enabled         <7>│)pin",
             R"pin(          <7>│ <15>Hostile Dangerously Close                                  <7>│ <10>Enabled         <7>│)pin",
             R"pin(          <7>│ <15>Conversation                                               <7>│ <10>Enabled         <7>│)pin",
             R"pin(          <7>│ <15>Asthma                                                     <7>│ <10>Enabled         <7>│)pin",
             R"pin(          <7>│ <15>Weather change                                             <7>│ <10>Enabled         <7>│)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>└────────────────────────────────────────────────────────────┴─────────────────┘)pin",
         }},
        {"distractions:1",
         {
             R"pin(          <7>┌─────────────────────────────<9>Distractions manager<7>─────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│ <15>Noise                                                      <7>│ <10>Enabled         <7>│)pin",
             R"pin(          <7>│ <15:4>Pain                                                       <7>│ <10>Enabled         <7>│)pin",
             R"pin(          <7>│ <15>Attacked                                                   <7>│ <10>Enabled         <7>│)pin",
             R"pin(          <7>│ <15>Hostile Spotted                                            <7>│ <10>Enabled         <7>│)pin",
             R"pin(          <7>│ <15>Hostile Dangerously Close                                  <7>│ <10>Enabled         <7>│)pin",
             R"pin(          <7>│ <15>Conversation                                               <7>│ <10>Enabled         <7>│)pin",
             R"pin(          <7>│ <15>Asthma                                                     <7>│ <10>Enabled         <7>│)pin",
             R"pin(          <7>│ <15>Weather change                                             <7>│ <10>Enabled         <7>│)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>└────────────────────────────────────────────────────────────┴─────────────────┘)pin",
         }},
        {"distractions:2",
         {
             R"pin(          <7>┌─────────────────────────────<9>Distractions manager<7>─────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│ <15>Noise                                                      <7>│ <10>Enabled         <7>│)pin",
             R"pin(          <7>│ <15:4>Pain                                                       <7>│ <1>Disabled        <7>│)pin",
             R"pin(          <7>│ <15>Attacked                                                   <7>│ <10>Enabled         <7>│)pin",
             R"pin(          <7>│ <15>Hostile Spotted                                            <7>│ <10>Enabled         <7>│)pin",
             R"pin(          <7>│ <15>Hostile Dangerously Close                                  <7>│ <10>Enabled         <7>│)pin",
             R"pin(          <7>│ <15>Conversation                                               <7>│ <10>Enabled         <7>│)pin",
             R"pin(          <7>│ <15>Asthma                                                     <7>│ <10>Enabled         <7>│)pin",
             R"pin(          <7>│ <15>Weather change                                             <7>│ <10>Enabled         <7>│)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>│                                                            │                 │)pin",
             R"pin(          <7>└────────────────────────────────────────────────────────────┴─────────────────┘)pin",
         }},
        {"colors:0",
         {
             R"pin(          <7>┌─────────────────────────────── <9>COLOR MANAGER <7>────────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <10>^ <11>> <15>c_black           <0>default  <1>c_red             <7>│   <0:7>default  <10>c_light_green    <7>│)pin",
             R"pin(          <6:6>│   <15>c_black_cyan      <0:6>default                    <7>│   <6>default                   <7>│)pin",
             R"pin(          <6:6>│   <15>c_black_green     <0:2>default                    <7>│   <2>default                   <7>│)pin",
             R"pin(          <15>│   c_black_magenta   <0:5>default                    <7>│   <5>default                   <7>│)pin",
             R"pin(          <15>│   c_black_red       <8:1>default                    <7>│   <1>default                   <7>│)pin",
             R"pin(          <15>│   c_black_white     <0:7>default                    <7>│   default                   │)pin",
             R"pin(          <15>│   c_black_yellow    <0:3>default                    <7>│   <3>default                   <7>│)pin",
             R"pin(          <15>│   c_blue            <4>default                    <7>│   <0:4>default                   <7>│)pin",
             R"pin(          <15>│   c_blue_cyan       <4:6>default                    <7>│   <6:4>default                   <7>│)pin",
             R"pin(          <15>│   c_blue_green      <4:2>default                    <7>│   <2:4>default                   <7>│)pin",
             R"pin(          <15>│   c_blue_magenta    <4:5>default                    <7>│   <5:4>default                   <7>│)pin",
             R"pin(          <15>│   c_blue_red        <4:1>default                    <7>│   <1:4>default                   <7>│)pin",
             R"pin(          <15>│   c_blue_white      <4:7>default                    <7>│   <7:4>default                   <7>│)pin",
             R"pin(          <15>│   c_blue_yellow     <4:3>default                    <7>│   <3:4>default                   <7>│)pin",
             R"pin(          <15>│   c_brown           <3>default                    <7>│   <0:3>default                   <7>│)pin",
             R"pin(          <15>│   c_brown_cyan      <3:6>default                    <7>│   <6:3>default                   <7>│)pin",
             R"pin(          <15>│   c_brown_green     <3:2>default                    <7>│   <2:3>default                   <7>│)pin",
             R"pin(          <10>v   <15>c_brown_magenta   <3:5>default                    <7>│   <5:3>default                   <7>│)pin",
             R"pin(          <7>└────────────────────────────────────────────────┴─────────────────────────────┘)pin",
         }},
        {"colors:1",
         {
             R"pin(          <7>┌─────────────────────────────── <9>COLOR MANAGER <7>────────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <10>^   <15>c_black           <0>default  <1>c_red             <7>│   <0:7>default  <10>c_light_green    <7>│)pin",
             R"pin(          <15>│ <11>> <15>c_black_cyan      <0:6>default                    <7>│   <6>default                   <7>│)pin",
             R"pin(          <6:6>│   <15>c_black_green     <0:2>default                    <7>│   <2>default                   <7>│)pin",
             R"pin(          <6:6>│   <15>c_black_magenta   <0:5>default                    <7>│   <5>default                   <7>│)pin",
             R"pin(          <15>│   c_black_red       <8:1>default                    <7>│   <1>default                   <7>│)pin",
             R"pin(          <15>│   c_black_white     <0:7>default                    <7>│   default                   │)pin",
             R"pin(          <15>│   c_black_yellow    <0:3>default                    <7>│   <3>default                   <7>│)pin",
             R"pin(          <15>│   c_blue            <4>default                    <7>│   <0:4>default                   <7>│)pin",
             R"pin(          <15>│   c_blue_cyan       <4:6>default                    <7>│   <6:4>default                   <7>│)pin",
             R"pin(          <15>│   c_blue_green      <4:2>default                    <7>│   <2:4>default                   <7>│)pin",
             R"pin(          <15>│   c_blue_magenta    <4:5>default                    <7>│   <5:4>default                   <7>│)pin",
             R"pin(          <15>│   c_blue_red        <4:1>default                    <7>│   <1:4>default                   <7>│)pin",
             R"pin(          <15>│   c_blue_white      <4:7>default                    <7>│   <7:4>default                   <7>│)pin",
             R"pin(          <15>│   c_blue_yellow     <4:3>default                    <7>│   <3:4>default                   <7>│)pin",
             R"pin(          <15>│   c_brown           <3>default                    <7>│   <0:3>default                   <7>│)pin",
             R"pin(          <15>│   c_brown_cyan      <3:6>default                    <7>│   <6:3>default                   <7>│)pin",
             R"pin(          <15>│   c_brown_green     <3:2>default                    <7>│   <2:3>default                   <7>│)pin",
             R"pin(          <10>v   <15>c_brown_magenta   <3:5>default                    <7>│   <5:3>default                   <7>│)pin",
             R"pin(          <7>└────────────────────────────────────────────────┴─────────────────────────────┘)pin",
         }},
        {"colors:2",
         {
             R"pin(          <7>┌─────────────────────────────── <9>COLOR MANAGER <7>────────────────────────────────┐)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <7>├                                                                              ┤)pin",
             R"pin(          <7>│                                                                              │)pin",
             R"pin(          <10>^   <15>c_black           <0>default  <1>c_red             <7>│   <0:7>default  <10>c_light_green    <7>│)pin",
             R"pin(          <15>│   c_black_cyan      <0:6>default                    <7>│ <11>> <6>default                   <7>│)pin",
             R"pin(          <6:6>│   <15>c_black_green     <0:2>default                    <7>│   <2>default                   <7>│)pin",
             R"pin(          <6:6>│   <15>c_black_magenta   <0:5>default                    <7>│   <5>default                   <7>│)pin",
             R"pin(          <15>│   c_black_red       <8:1>default                    <7>│   <1>default                   <7>│)pin",
             R"pin(          <15>│   c_black_white     <0:7>default                    <7>│   default                   │)pin",
             R"pin(          <15>│   c_black_yellow    <0:3>default                    <7>│   <3>default                   <7>│)pin",
             R"pin(          <15>│   c_blue            <4>default                    <7>│   <0:4>default                   <7>│)pin",
             R"pin(          <15>│   c_blue_cyan       <4:6>default                    <7>│   <6:4>default                   <7>│)pin",
             R"pin(          <15>│   c_blue_green      <4:2>default                    <7>│   <2:4>default                   <7>│)pin",
             R"pin(          <15>│   c_blue_magenta    <4:5>default                    <7>│   <5:4>default                   <7>│)pin",
             R"pin(          <15>│   c_blue_red        <4:1>default                    <7>│   <1:4>default                   <7>│)pin",
             R"pin(          <15>│   c_blue_white      <4:7>default                    <7>│   <7:4>default                   <7>│)pin",
             R"pin(          <15>│   c_blue_yellow     <4:3>default                    <7>│   <3:4>default                   <7>│)pin",
             R"pin(          <15>│   c_brown           <3>default                    <7>│   <0:3>default                   <7>│)pin",
             R"pin(          <15>│   c_brown_cyan      <3:6>default                    <7>│   <6:3>default                   <7>│)pin",
             R"pin(          <15>│   c_brown_green     <3:2>default                    <7>│   <2:3>default                   <7>│)pin",
             R"pin(          <10>v   <15>c_brown_magenta   <3:5>default                    <7>│   <5:3>default                   <7>│)pin",
             R"pin(          <7>└────────────────────────────────────────────────┴─────────────────────────────┘)pin",
         }},
    };
    return screens;
}

/// Whether the rows are the recorded screen `name`, naming the first row that differs.
auto matches(const std::string& name, const std::vector<std::string>& rows) -> bool {
    const auto& expected = recorded().at(name);
    INFO(name);
    CHECK(rows.size() == expected.size());
    for (auto i = std::size_t{}; i < std::min(rows.size(), expected.size()); ++i) {
        INFO("row " << i);
        CHECK(rows[i] == expected[i]);
    }
    return rows == expected;
}

/// Every distraction enabled, whatever the profile holds; the profile's own are put back.
struct distractions_guard {
    const std::map<distraction_type, bool> saved = get_distraction_manager().distractions;

    distractions_guard() {
        for (auto i = 0; i < static_cast<int>(distraction_type::num_distraction_type); ++i) {
            get_distraction_manager().distractions[static_cast<distraction_type>(i)] = false;
        }
    }

    ~distractions_guard() { get_distraction_manager().distractions = saved; }
};

} // namespace

TEST_CASE("the native distraction manager keeps its text and colors", "[rules_native_pin]") {
    const auto guard = screen_guard{};
    const auto distractions = distractions_guard{};
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        // The first row, the cursor on the second, then that one switched off.
        CHECK(matches(std::format("distractions:{}", step), rendered()));
        switch (step++) {
            case 0:
                return act("DOWN");
            case 1:
                return act("CONFIRM");
            default:
                return act("QUIT");
        }
    });
    get_distraction_manager().show();
    CHECK(step == 3);
}

TEST_CASE("the native colour manager keeps its text and colors", "[rules_native_pin]") {
    const auto guard = screen_guard{};
    // A manager of its own, with two custom colours, so the profile's colours stay as they are.
    const auto custom = std::filesystem::path{PATH_INFO::user_dir()} / "rules_pin_colors.json";
    {
        auto out = std::ofstream{custom};
        out << R"([{"name":"c_black","custom":"c_red","invertcustom":"c_light_green"}])";
    }
    auto colors = color_manager{};
    colors.load_default();
    colors.load_custom(custom.string());
    std::filesystem::remove(custom);
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        // The first row, the cursor on the second, then in the other column.
        CHECK(matches(std::format("colors:{}", step), rendered()));
        switch (step++) {
            case 0:
                return act("DOWN");
            case 1:
                return act("RIGHT");
            default:
                return act("QUIT");
        }
    });
    colors.show_gui();
    CHECK(step == 3);
}

#endif // CATA_MCP
