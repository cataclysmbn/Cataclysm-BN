#pragma once

#include <functional>
#include <map>
#include <optional>
#include <vector>

#include "type_id.h"

class Character;
class item;
class recipe_subset;
class player;
class time_duration;

struct digital_music_summary {
    int songs = 0;
    int morale_cap = 0;
};

struct digital_file_upload_options {
    item &source;
    item &destination;
    item *file = nullptr;
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
auto is_clean_digital_card( const item &media ) -> bool;
/// Copy one selected file, or all files, from a powered reader to a clean SD card.
auto upload_digital_files( const Character &who, const digital_file_upload_options &opts ) -> int;
/// Convert old photo/music counters to files without changing their quantities.
auto migrate_legacy_digital_media( item &media ) -> void;
/// Migrate old drives and find sample files carried on any storage device.
auto digital_genome_samples( const Character &who ) -> std::vector<item *>;
/// Writable USB drives and SD cards, initialized and excluding encrypted media.
auto digital_genome_storage( const Character &who ) -> std::vector<item *>;
/// The device's own captured photos, separate from imported collections.
auto camera_photo_collection( item &camera ) -> item &; // *NOPAD*
auto find_camera_photo_collection( item &camera ) -> digital_reader_reference;
/// Finish a capture, updating its count and copy identity so newer versions can be copied.
auto update_camera_photo_collection( item &camera, int count ) -> void;
/// Best available photograph of each monster across all collections on a device.
auto digital_monster_photos( const item &reader ) -> std::map<mtype_id, int>;
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
