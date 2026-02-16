#pragma once

#include <string>
#include <utility>
#include <vector>

#include "creature.h"

// Returns pairs of message log type id and untranslated name
const std::vector<std::pair<game_message_type, const char *>> &msg_type_and_names();

// Get message type from translated name, returns true if name is a valid translated name
bool msg_type_from_name( game_message_type &type, const std::string &name );