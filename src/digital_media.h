#pragma once

#include <functional>
#include <optional>

class Character;
class item;
class recipe_subset;
class player;
class time_duration;

struct digital_music_summary {
    int songs = 0;
    int morale_cap = 0;
};

using digital_reader_reference = std::optional<std::reference_wrapper<item>>;

/// Digital files are only usable while installed on a powered reader.
auto is_digital_file( const item &file ) -> bool;
auto digital_reader_ready( const item &reader, const Character &who ) -> bool;
auto digital_reader_for( const Character &who, const item &file ) -> digital_reader_reference;
/// Roll the media's JSON item group once, preserving legacy initialized cards.
auto initialize_digital_media( item &media ) -> void;
/// Keep the clean/used SD-card state consistent with its actual stored data.
auto refresh_memory_card_state( item &media ) -> void;
auto finish_memory_card_download( item &media ) -> void;
/// Copy files without consuming the source or duplicating identical files.
auto download_digital_files( item &reader, item &media, const Character &who ) -> int;
/// Convert old photo/music counters to files without changing their quantities.
auto migrate_legacy_digital_media( item &media ) -> void;
auto digital_music_on( const item &reader ) -> digital_music_summary;
auto digital_music_playing( const item &reader ) -> digital_music_summary;
auto digital_photos_on( const item &reader ) -> int;
auto view_digital_photos( player &who, item &album ) -> bool;
auto digital_photo_cooldown_remaining( const Character &who, const item &album ) -> time_duration;
auto digital_music_cooldown_remaining( const Character &who, const item &file ) -> time_duration;
auto digital_music_ready( const Character &who, item &reader ) -> bool;
auto listen_to_digital_music( player &who, item &reader, item &file ) -> bool;
auto finish_digital_music( Character &who, const item &reader ) -> void;
/// Recipe references on a reader, including copies in a crafting inventory.
auto digital_recipe_references( const item &reader, const Character &who ) -> recipe_subset;
