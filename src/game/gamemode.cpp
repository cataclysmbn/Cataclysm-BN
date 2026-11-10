#include "gamemode.h"

#include "debug.h"
#include "gamemode_defense.h"
#include "gamemode_tutorial.h"
#include "translations.h"

auto special_game_name(special_game_type id) -> std::string {
    switch (id) {
        case special_game_type::NONE:
        case special_game_type::NUM_SPECIAL_GAME_TYPES:
            return "Bug (gamemode.cpp:special_game_name)";
        case special_game_type::TUTORIAL:
            return _("Tutorial");
        case special_game_type::DEFENSE:
            return _("Defense");
        default:
            return "Uncoded (gamemode.cpp:special_game_name)";
    }
}

auto get_special_game(special_game_type id) -> std::unique_ptr<special_game> {
    std::unique_ptr<special_game> ret;
    switch (id) {
        case special_game_type::NONE:
            ret = std::make_unique<special_game>();
            break;
        case special_game_type::TUTORIAL:
            ret = std::make_unique<tutorial_game>();
            break;
        case special_game_type::DEFENSE:
            ret = std::make_unique<defense_game>();
            break;
        default:
            debugmsg("Missing something in gamemode.cpp:get_special_game()?");
            ret = std::make_unique<special_game>();
            break;
    }

    return ret;
}
