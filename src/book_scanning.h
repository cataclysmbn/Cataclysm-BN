#pragma once

class Character;
class item;
class player;
class player_activity;

/// Scan a carried physical book onto a powered reader without consuming the book.
auto start_book_scan( Character &who, item &reader, item &book ) -> bool;

namespace activity_handlers
{
auto scan_book_do_turn( player_activity *act, player *who ) -> void;
auto scan_book_finish( player_activity *act, player *who ) -> void;
} // namespace activity_handlers
