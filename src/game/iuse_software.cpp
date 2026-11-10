#include "iuse_software.h"

#include "cursesdef.h"
#include "iuse_software_kitten.h"
#include "iuse_software_lightson.h"
#include "iuse_software_minesweeper.h"
#include "iuse_software_snake.h"
#include "iuse_software_sokoban.h"
#include "output.h"
#include "point.h"
#include "string_formatter.h"
#include "translations.h"

#include <algorithm>
#include <map>
#include <string>

bool play_videogame(const std::string& function_name, std::string& end_message, int& score) {
    if (function_name == "null") {
        score = 30;
        return true; // generic game
    }
    if (function_name == "robot_finds_kitten") {
        robot_finds_kitten findkitten;
        bool foundkitten = findkitten.ret;
        if (foundkitten) {
            end_message = _("You found kitten!");
            score = 30;
        }

        return foundkitten;
    } else if (function_name == "snake_game") {
        snake_game sg;
        int iScore = sg.start_game();

        if (iScore >= 10000) {
            score = 30;
        } else if (iScore >= 5000) {
            score = 15;
        } else {
            score = 5;
        }

        return true;
    } else if (function_name == "sokoban_game") {
        sokoban_game sg;
        int iScore = sg.start_game();

        if (iScore >= 5000) {
            score = 30;
        } else if (iScore >= 1000) {
            score = 15;
        } else {
            score = 5;
        }

        return true;
    } else if (function_name == "minesweeper_game") {
        minesweeper_game mg;
        score = mg.start_game();

        return true;
    } else if (function_name == "lightson_game") {
        lightson_game lg;
        int iScore = lg.start_game();
        score = std::min(15, iScore * 3);

        return true;
    } else {
        debugmsg("Invalid function_name for play_videogame.");
        score = 0;
        return false;
    }
}
