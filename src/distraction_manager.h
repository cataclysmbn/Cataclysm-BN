#pragma once

#include <string>
#include <array>
#include <map>

#include "creature.h"
#include "enums.h"

namespace distraction_manager
{
class distraction_manager_gui
{
    public:
        bool save();
        void load();
        void show();

        void serialize( JsonOut &json ) const;
        void deserialize( JsonIn &jsin );

        bool is_ignored( distraction_type &distract );

        std::map<distraction_type, bool> distractions;
};
} // namespace distraction_manager

distraction_manager::distraction_manager_gui &get_distraction_manager();


