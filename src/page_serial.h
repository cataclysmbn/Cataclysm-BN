#pragma once

#include <cstdint>
#include <optional>
#include <string>

struct diary_page;

namespace game_client
{

/// A diary page's identity within the session, for the rows of the diary screen: given the first time the
/// page is published, kept while pages before it are deleted, never saved, never in the state hash, and never
/// reused. Not the page's turn, which two pages can share, nor its place in the diary.
auto diary_page_serial( const diary_page &page ) -> std::uint64_t;

/// The row id of one diary page: `page:<serial>`.
auto diary_page_row_id( const diary_page &page ) -> std::string;

/// The serial a page already holds; unlike `diary_page_serial` it gives none.
auto find_diary_page_serial( const diary_page &page ) -> std::optional<std::uint64_t>;

/// Drops the serial of a page that is being deleted.
auto forget_diary_page_serial( const diary_page *page ) -> void;

/// Drops every serial and starts the numbering again; `reset_item_serials` does it with the items'.
auto reset_diary_page_serials() -> void;

} // namespace game_client
