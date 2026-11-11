#pragma once

class Character;
class item;
class player;
class player_activity;

/// Physical books and items with snippet text can be scanned.
auto is_scannable_text( const item &text ) -> bool;
/// Scan carried physical reading material onto a powered reader without consuming it.
auto start_book_scan( Character &who, item &reader, item &book ) -> bool;

namespace activity_handlers
{
auto scan_book_do_turn( player_activity *act, player *who ) -> void;
auto scan_book_finish( player_activity *act, player *who ) -> void;
} // namespace activity_handlers
