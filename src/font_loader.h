#pragma once

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

#include "debug.h"
#include "filesystem.h"
#include "fstream_utils.h"
#include "json.h"
#include "path_info.h"
#include "cata_utility.h"

// Ensure that unifont is always loaded as a fallback font to prevent users from shooting themselves in the foot
void ensure_unifont_loaded( std::vector<std::string> &font_list );

class font_loader
{
    public:
        bool fontblending = false;
        std::vector<std::string> typeface;
        std::vector<std::string> map_typeface;
        std::vector<std::string> overmap_typeface;
        int fontwidth = 8;
        int fontheight = 16;
        int fontsize = 16;
        int map_fontwidth = 8;
        int map_fontheight = 16;
        int map_fontsize = 16;
        int overmap_fontwidth = 8;
        int overmap_fontheight = 16;
        int overmap_fontsize = 16;

    private:
        void load_throws( const std::string &path );

    public:
        void load();
};


