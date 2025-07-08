#pragma once

#include <string>

class JsonObject;
class player;
class map;
class Character;
struct tripoint;

namespace gates
{

void load( const JsonObject &jo, const std::string &src );
void check();
void reset();

/** opens/closes the gate via player's activity */
void toggle_gate( const tripoint &pos, Character &who );
/** opens/closes the gate immediately */
void toggle_gate( const tripoint &pos );

} // namespace gates

namespace doors
{

/**
 * Handles deducting moves, printing messages (only non-NPCs cause messages), actually closing it,
 * checking if it can be closed, etc.
*/
void close_door( map &m, Character &who, const tripoint &closep );

} // namespace doors


