#include "digital_media.h"
#include "digital_recipes.h"

#include "character.h"
#include "calendar.h"
#include "debug.h"
#include "flag.h"
#include "item.h"
#include "item_contents.h"
#include "item_group.h"
#include "itype.h"
#include "json.h"
#include "recipe.h"
#include "recipe_dictionary.h"
#include "rng.h"
#include "player.h"
#include "morale_types.h"
#include "messages.h"
#include "text_snippets.h"
#include "translations.h"
#include "type_id.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <string_view>
#include <string>
#include <sstream>

namespace
{
auto memory_card_has_data( const item &media ) -> bool
{
    return !media.contents.empty() || media.get_var( "MC_PHOTOS", 0 ) > 0 ||
           media.get_var( "MC_MUSIC", 0 ) > 0 || !media.get_var( "MC_RECIPE" ).empty() ||
           !media.get_var( "MC_EXTENDED_PHOTOS" ).empty() ||
           !media.get_var( "MC_MONSTER_PHOTOS" ).empty();
}

auto digital_property( const item &file, const std::string &key ) -> int
{
    return static_cast<int>( std::clamp<int64_t>( file.get_property_int64_t( key ), 0, INT_MAX ) );
}

auto identify_digital_file( item &file ) -> void
{
    if( !file.has_var( "DIGITAL_FILE_ID" ) ) {
        file.set_var( "DIGITAL_FILE_ID", std::to_string( rng_bits() ) + "-" +
                      std::to_string( rng_bits() ) + "-" + std::to_string( rng_bits() ) + "-" +
                      std::to_string( rng_bits() ) );
    }
}

auto copy_digital_file( item &file, item &destination ) -> bool
{
    namespace ranges = std::ranges;
    if( !is_digital_file( file ) || !destination.can_contain( file ) ) {
        return false;
    }
    initialize_digital_recipe_file( file );
    identify_digital_file( file );
    const auto duplicate = ranges::any_of( destination.contents.all_items_top(),
    [&file]( const auto * installed ) {
        return installed->get_var( "DIGITAL_FILE_ID" ) == file.get_var( "DIGITAL_FILE_ID" );
    } );
    if( duplicate ) {
        return false;
    }
    destination.put_in( item::spawn( file ) );
    return true;
}
} // namespace

auto is_digital_file( const item &file ) -> bool { return file.has_flag( flag_id( "DIGITAL" ) ); }

auto digital_reader_ready( const item &reader, const Character &who ) -> bool
{
    return reader.is_tool() && reader.has_flag( flag_id( "DIGITAL_READER" ) ) &&
           reader.has_flag( flag_id( "DIGITAL_STORAGE" ) ) && who.has_enough_charges( reader, false ) &&
           !reader.has_flag( flag_MC_ENCRYPTED );
}

auto digital_reader_for( const Character &who, const item &file ) -> digital_reader_reference
{
    namespace ranges = std::ranges;
    for( auto *reader : who.all_items_with_flag( flag_id( "DIGITAL_READER" ) ) ) {
        if( digital_reader_ready( *reader, who ) &&
            ranges::contains( reader->contents.all_items_top(), &file ) ) {
            return std::ref( *reader );
        }
    }
    return std::nullopt;
}

auto initialize_digital_media( item &media ) -> void
{
    migrate_legacy_digital_media( media );
    refresh_memory_card_state( media );
    auto group = media.get_property_string( "digital_content" );
    if( group.empty() || media.has_var( "DIGITAL_INITIALIZED" ) ) {
        return;
    }
    media.set_var( "DIGITAL_INITIALIZED", 1 );
    if( media.has_flag( flag_MC_USED ) || media.has_flag( flag_MC_HAS_DATA ) ) {
        return;
    }
    const auto encryption_chance = digital_property( media, "digital_encryption_chance" );
    const auto encrypted_type = media.get_property_string( "digital_encrypted_type" );
    if( encryption_chance > 0 && !encrypted_type.empty() && one_in( encryption_chance ) ) {
        media.convert( itype_id( encrypted_type ) );
        group = media.get_property_string( "digital_content" );
    }
    for( auto &file : item_group::items_from( item_group_id( group ), media.birthday() ) ) {
        if( !is_digital_file( *file ) ) {
            debugmsg( "Digital content group %s contains a physical item: %s", group,
                      file->typeId().str() );
            continue;
        }
        identify_digital_file( *file );
        media.put_in( std::move( file ) );
    }
    if( media.has_flag( flag_MC_MOBILE ) ) {
        media.set_flag( flag_MC_HAS_DATA );
    }
}

auto refresh_memory_card_state( item &media ) -> void
{
    if( !media.has_flag( flag_MC_USED ) ) {
        return;
    }
    if( media.typeId() == itype_id( "mobile_memory_card_used" ) && memory_card_has_data( media ) ) {
        media.convert( itype_id( "mobile_memory_card_data" ) );
    } else if( media.typeId() == itype_id( "mobile_memory_card_data" ) &&
               !memory_card_has_data( media ) ) {
        media.convert( itype_id( "mobile_memory_card_used" ) );
    }
}

auto finish_memory_card_download( item &media ) -> void
{
    if( media.has_flag( flag_MC_ENCRYPTED ) ) {
        return;
    }
    if( media.has_flag( flag_MC_TURN_USED ) ) {
        media.unset_flags();
        media.convert( itype_id( memory_card_has_data( media ) ? "mobile_memory_card_data" :
                                 "mobile_memory_card_used" ) );
        media.set_var( "DIGITAL_INITIALIZED", 1 );
    }
    refresh_memory_card_state( media );
}

auto download_digital_files( item &reader, item &media, const Character &who ) -> int
{
    if( &reader == &media || !digital_reader_ready( reader, who ) ||
        !media.has_flag( flag_id( "DIGITAL_STORAGE" ) ) || media.has_flag( flag_MC_ENCRYPTED ) ) {
        return 0;
    }
    migrate_legacy_digital_media( reader );
    initialize_digital_media( media );
    if( media.has_flag( flag_MC_ENCRYPTED ) ) {
        return 0;
    }
    auto copied = 0;
    for( auto *file : media.contents.all_items_top() ) {
        if( copy_digital_file( *file, reader ) ) {
            ++copied;
        }
    }
    return copied;
}

auto is_clean_digital_card( const item &media ) -> bool
{
    return media.has_flag( flag_MC_MOBILE ) && media.has_flag( flag_MC_USED ) &&
           media.has_flag( flag_id( "DIGITAL_STORAGE" ) ) && !media.has_flag( flag_MC_ENCRYPTED ) &&
           !memory_card_has_data( media );
}

auto upload_digital_files( const Character &who, const digital_file_upload_options &opts ) -> int
{
    namespace ranges = std::ranges;
    if( &opts.source == &opts.destination || !who.has_item( opts.source ) ||
        !digital_reader_ready( opts.source, who ) || !is_clean_digital_card( opts.destination ) ) {
        return 0;
    }
    if( opts.file != nullptr ) {
        if( !ranges::contains( opts.source.contents.all_items_top(), opts.file ) ) {
            return 0;
        }
        return copy_digital_file( *opts.file, opts.destination ) ? 1 : 0;
    }
    auto copied = 0;
    for( auto *file : opts.source.contents.all_items_top() ) {
        if( copy_digital_file( *file, opts.destination ) ) {
            ++copied;
        }
    }
    return copied;
}

auto migrate_legacy_digital_media( item &media ) -> void
{
    if( media.has_flag( flag_id( "GENOME_DRIVE" ) ) && !is_digital_file( media ) ) {
        if( media.has_var( "specimen_sample" ) ) {
            auto file = item::spawn( "digital_genome_sample", media.birthday() );
            for( const auto *key : { "specimen_sample", "specimen_sample_progress", "specimen_name" } ) {
                if( media.has_var( key ) ) {
                    file->set_var( key, media.get_var( key ) );
                    media.erase_var( key );
                }
            }
            identify_digital_file( *file );
            media.put_in( std::move( file ) );
        }
        media.convert( itype_id( "usb_drive" ) );
    }
    struct legacy_counter {
        std::string variable;
        std::string file;
    };
    const auto counters = std::array{
        legacy_counter{ .variable = "MC_PHOTOS", .file = "digital_photo_album" },
        legacy_counter{ .variable = "EIPC_PHOTOS", .file = "digital_photo_album" },
        legacy_counter{ .variable = "MC_MUSIC", .file = "digital_music_collection" },
        legacy_counter{ .variable = "EIPC_MUSIC", .file = "digital_music_collection" }
    };
    for( const auto &counter : counters ) {
        if( media.has_var( counter.variable ) ) {
            media.set_var( "DIGITAL_INITIALIZED", 1 );
        }
        const auto quantity = media.get_var( counter.variable, 0 );
        if( quantity > 0 ) {
            auto file = item::spawn( counter.file, media.birthday() );
            file->charges = quantity;
            identify_digital_file( *file );
            media.put_in( std::move( file ) );
        }
        media.erase_var( counter.variable );
    }
    if( media.has_var( "CAMERA_EXTENDED_PHOTOS" ) || media.has_var( "CAMERA_NPC_PHOTOS" ) ||
        media.has_var( "CAMERA_MONSTER_PHOTOS" ) ) {
        auto &album = camera_photo_collection( media );
        auto count = 0;
        for( const auto *key : { "CAMERA_EXTENDED_PHOTOS", "CAMERA_NPC_PHOTOS", "CAMERA_MONSTER_PHOTOS" } ) {
            if( media.has_var( key ) ) {
                album.set_var( key, media.get_var( key ) );
                if( std::string_view( key ) != "CAMERA_MONSTER_PHOTOS" && !media.get_var( key ).empty() ) {
                    auto input = std::istringstream( media.get_var( key ) );
                    auto json = JsonIn( input );
                    count += json.get_array().size();
                }
                media.erase_var( key );
            }
        }
        album.charges = count;
    }
    const auto recipe_token = media.get_var( "MC_RECIPE" );
    if( !recipe_token.empty() ) {
        auto file = item::spawn( recipe_token == "SCIENCE" ? "digital_science_recipe_archive" :
                                 "digital_cooking_recipe_archive", media.birthday() );
        initialize_digital_recipe_file( *file );
        identify_digital_file( *file );
        media.put_in( std::move( file ) );
        media.erase_var( "MC_RECIPE" );
        media.set_var( "DIGITAL_INITIALIZED", 1 );
    }
}

auto digital_genome_samples( const Character &who ) -> std::vector<item *>
{
    const auto flag = flag_id( "GENOME_DRIVE" );
    for( auto *drive : who.all_items_with_flag( flag ) ) {
        migrate_legacy_digital_media( *drive );
    }
    auto samples = who.all_items_with_flag( flag );
    std::erase_if( samples, [&who]( const auto * sample ) {
        return !is_digital_file( *sample ) || std::ranges::any_of( who.parents( *sample ),
        []( const auto * parent ) { return parent->has_flag( flag_MC_ENCRYPTED ); } );
    } );
    for( auto *sample : samples ) {
        identify_digital_file( *sample );
    }
    return samples;
}

auto digital_genome_storage( const Character &who ) -> std::vector<item *>
{
    auto storage = who.all_items_with_flag( flag_id( "DIGITAL_STORAGE" ) );
    std::erase_if( storage, []( auto * media ) {
        if( media->typeId() != itype_id( "usb_drive" ) && !media->has_flag( flag_MC_MOBILE ) ) {
            return true;
        }
        initialize_digital_media( *media );
        return media->has_flag( flag_MC_ENCRYPTED );
    } );
    return storage;
}

auto find_camera_photo_collection( item &camera ) -> digital_reader_reference
{
    const auto identity = camera.get_var( "CAMERA_COLLECTION_ID" );
    for( auto *file : camera.contents.all_items_top() ) {
        if( !identity.empty() && file->get_var( "DIGITAL_FILE_ID" ) == identity ) {
            return std::ref( *file );
        }
    }
    return std::nullopt;
}

auto camera_photo_collection( item &camera ) -> item & // *NOPAD*
{
    if( const auto file = find_camera_photo_collection( camera ) ) {
        return file->get();
    }
    auto file = item::spawn( "digital_camera_photo_collection", camera.birthday() );
    identify_digital_file( *file );
    camera.set_var( "CAMERA_COLLECTION_ID", file->get_var( "DIGITAL_FILE_ID" ) );
    auto &result = *file;
    camera.put_in( std::move( file ) );
    return result;
}

auto update_camera_photo_collection( item &camera, const int count ) -> void
{
    auto &file = camera_photo_collection( camera );
    file.charges = count;
    file.erase_var( "DIGITAL_FILE_ID" );
    identify_digital_file( file );
    camera.set_var( "CAMERA_COLLECTION_ID", file.get_var( "DIGITAL_FILE_ID" ) );
}

auto digital_monster_photos( const item &reader ) -> std::map<mtype_id, int>
{
    auto result = std::map<mtype_id, int> {};
    const auto append = [&result]( const auto & data ) {
        auto input = std::istringstream( data );
        auto token = std::string{};
        while( std::getline( input, token, ',' ) ) {
            if( token.empty() ) {
                continue;
            }
            const auto id = mtype_id( token );
            if( !std::getline( input, token, ',' ) ) {
                break;
            }
            if( id.is_valid() ) {
                result[id] = std::max( result[id], std::clamp( std::atoi( token.c_str() ), 0, 5 ) );
            }
        }
    };
    append( reader.get_var( "EINK_MONSTER_PHOTOS" ) );
    append( reader.get_var( "CAMERA_MONSTER_PHOTOS" ) );
    for( const auto *file : reader.contents.all_items_top() ) {
        if( is_digital_file( *file ) ) {
            append( file->get_var( "CAMERA_MONSTER_PHOTOS" ) );
        }
    }
    return result;
}

auto digital_music_on( const item &reader ) -> digital_music_summary
{
    auto result = digital_music_summary{};
    for( const auto *file : reader.contents.all_items_top() ) {
        if( file->has_flag( flag_id( "DIGITAL_MUSIC" ) ) ) {
            result.songs += std::max( 0, static_cast<int>( file->charges ) );
            result.morale_cap = std::max( result.morale_cap,
                                          digital_property( *file, "digital_music_morale_cap" ) );
        }
    }
    result.morale_cap = std::min( result.songs, result.morale_cap );
    return result;
}

auto digital_music_playing( const item &reader ) -> digital_music_summary
{
    const auto selected = reader.get_var( "DIGITAL_SELECTED_MUSIC" );
    if( selected.empty() ) {
        return {};
    }
    for( const auto *file : reader.contents.all_items_top() ) {
        if( file->has_flag( flag_id( "DIGITAL_MUSIC" ) ) &&
            file->get_var( "DIGITAL_FILE_ID" ) == selected ) {
            const auto songs = std::max( 0, static_cast<int>( file->charges ) );
            return { .songs = songs, .morale_cap = std::min( songs,
                                                   digital_property( *file, "digital_music_morale_cap" ) ) };
        }
    }
    return {};
}

auto digital_photos_on( const item &reader ) -> int
{
    auto photos = 0;
    for( const auto *file : reader.contents.all_items_top() ) {
        if( file->has_flag( flag_id( "DIGITAL_PHOTOS" ) ) ) {
            photos += std::max( 0, static_cast<int>( file->charges ) );
        }
    }
    return photos;
}

auto digital_photo_cooldown_remaining( const Character &who, const item &album ) -> time_duration
{
    const auto until = who.get_value_as_int( "digital_photos_ready_" +
                       album.get_var( "DIGITAL_FILE_ID" ) ).value_or( 0 );
    return std::max( 0_turns, time_point::from_turn( until ) - calendar::turn );
}

auto digital_music_cooldown_remaining( const Character &who, const item &file ) -> time_duration
{
    const auto until = who.get_value_as_int( "digital_music_ready_" +
                       file.get_var( "DIGITAL_FILE_ID" ) ).value_or( 0 );
    return std::max( 0_turns, time_point::from_turn( until ) - calendar::turn );
}

auto digital_music_ready( const Character &who, item &reader ) -> bool
{
    initialize_digital_media( reader );
    auto *choice = static_cast<item *>( nullptr );
    const auto selected = reader.get_var( "DIGITAL_SELECTED_MUSIC" );
    for( auto *file : reader.contents.all_items_top() ) {
        if( !file->has_flag( flag_id( "DIGITAL_MUSIC" ) ) || file->charges <= 0 ) {
            continue;
        }
        identify_digital_file( *file );
        if( digital_music_cooldown_remaining( who, *file ) > 0_turns ) {
            continue;
        }
        if( choice == nullptr || file->get_var( "DIGITAL_FILE_ID" ) == selected ) {
            choice = file;
        }
    }
    if( choice == nullptr ) {
        who.add_msg_if_player( m_info,
                               _( "No music collection is ready to play.  Try another file or wait for its cooldown." ) );
        return false;
    }
    reader.set_var( "DIGITAL_SELECTED_MUSIC", choice->get_var( "DIGITAL_FILE_ID" ) );
    return true;
}

auto listen_to_digital_music( player &who, item &reader, item &file ) -> bool
{
    const auto installed_on = digital_reader_for( who, file );
    if( !file.has_flag( flag_id( "DIGITAL_MUSIC" ) ) || file.charges <= 0 ||
        !installed_on || &installed_on->get() != &reader ) {
        return false;
    }
    identify_digital_file( file );
    const auto remaining = digital_music_cooldown_remaining( who, file );
    if( remaining > 0_turns ) {
        who.add_msg_if_player( m_info, _( "You recently listened to this collection.  Try again in %s." ),
                               to_string( remaining ) );
        return false;
    }
    const auto playing = reader.type->can_use( "MP3_ON" ) ||
                         !reader.get_var( "EIPC_MUSIC_ON" ).empty();
    if( reader.is_active() && !playing && reader.type->tool->revert_to ) {
        reader.convert( *reader.type->tool->revert_to );
        reader.deactivate();
    }
    if( reader.is_active() ) {
        if( reader.get_var( "DIGITAL_SELECTED_MUSIC" ) == file.get_var( "DIGITAL_FILE_ID" ) ) {
            return true;
        }
        if( reader.type->can_use( "MP3_ON" ) ) {
            who.invoke_item( &reader, "MP3_ON" );
        } else {
            finish_digital_music( who, reader );
            reader.deactivate();
            reader.erase_var( "EIPC_MUSIC_ON" );
        }
    }
    reader.set_var( "DIGITAL_SELECTED_MUSIC", file.get_var( "DIGITAL_FILE_ID" ) );
    if( reader.type->can_use( "music_player" ) ) {
        who.invoke_item( &reader, "music_player" );
    } else {
        reader.activate();
        reader.set_var( "EIPC_MUSIC_ON", "1" );
    }
    return reader.is_active();
}

auto finish_digital_music( Character &who, const item &reader ) -> void
{
    const auto selected = reader.get_var( "DIGITAL_SELECTED_MUSIC" );
    if( selected.empty() ) {
        return;
    }
    for( const auto *file : reader.contents.all_items_top() ) {
        if( file->has_flag( flag_id( "DIGITAL_MUSIC" ) ) &&
            file->get_var( "DIGITAL_FILE_ID" ) == selected ) {
            const auto cooldown = time_duration::from_seconds( digital_property( *file,
                                  "digital_music_cooldown_seconds" ) );
            const auto until = calendar::turn + std::max( cooldown,
                               digital_music_cooldown_remaining( who, *file ) );
            who.set_value( "digital_music_ready_" + file->get_var( "DIGITAL_FILE_ID" ),
                           std::to_string( to_turn<int>( until ) ) );
        }
    }
}

auto view_digital_photos( player &who, item &album ) -> bool
{
    if( !album.has_flag( flag_id( "DIGITAL_PHOTOS" ) ) || album.charges <= 0 ||
        !digital_reader_for( who, album ) ) {
        return false;
    }
    identify_digital_file( album );
    const auto remaining = digital_photo_cooldown_remaining( who, album );
    if( remaining > 0_turns ) {
        who.add_msg_if_player( m_info, _( "You recently viewed this album.  Try again in %s." ),
                               to_string( remaining ) );
        return false;
    }
    who.mod_moves( -rng( digital_property( album, "digital_photos_moves_min" ),
                         digital_property( album, "digital_photos_moves_max" ) ) );
    const auto unaffected = who.has_trait( trait_id( "PSYCHOPATH" ) );
    if( !unaffected ) {
        who.add_morale( MORALE_PHOTOS, rng( digital_property( album, "digital_photos_morale_min" ),
                                            digital_property( album, "digital_photos_morale_max" ) ),
                        digital_property( album, "digital_photos_morale_cap" ) );
    }
    const auto category = album.get_property_string( unaffected ? "digital_photos_unaffected_snippets" :
                          "digital_photos_snippets" );
    if( const auto message = SNIPPET.random_from_category( category ) ) {
        who.add_msg_if_player( unaffected ? m_info : m_good, "%s", message->translated() );
    }
    const auto cooldown = time_duration::from_seconds( digital_property( album,
                          "digital_photos_cooldown_seconds" ) );
    who.set_value( "digital_photos_ready_" + album.get_var( "DIGITAL_FILE_ID" ),
                   std::to_string( to_turn<int>( calendar::turn + cooldown ) ) );
    return true;
}

auto digital_recipe_references( const item &reader, const Character &who ) -> recipe_subset
{
    auto result = recipe_subset{};
    if( !digital_reader_ready( reader, who ) ) {
        return result;
    }
    for( const auto *file : reader.contents.all_items_top() ) {
        if( !is_digital_file( *file ) ) {
            continue;
        }
        if( file->is_book() ) {
            for( const auto &entry : file->type->book->recipes ) {
                if( who.get_skill_level( entry.recipe->skill_used ) >= entry.skill_level ) {
                    result.include( entry.recipe, entry.skill_level );
                }
            }
        }
        for( const auto &entry : digital_file_recipes( *file ) ) {
            if( who.get_skill_level( entry.recipe->skill_used ) >= entry.skill_level ) {
                result.include( &*entry.recipe, entry.skill_level );
            }
        }
    }
    return result;
}
