#pragma once

#include <string>
#include <vector>

#include "type_id.h"

class JsonObject;


class ascii_art
{
    public:
        static void reset();
        static void load_ascii_art( const JsonObject &jo, const std::string &src );
        void load( const JsonObject &jo, const std::string & );
        bool was_loaded = false;

        ascii_art_id id;
        std::vector<std::string> picture;
};


