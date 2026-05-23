#include "world.h"

#include <algorithm>
#include <sstream>
#include <cstring>
#include <chrono>

#include "catacharset.h"
#include "game.h"
#include "overmapbuffer_registry.h"
#include "avatar.h"
#include "debug.h"
#include "cata_utility.h"
#include "filesystem.h"
#include "output.h"
#include "worldfactory.h"
#include "mod_manager.h"
#include "path_info.h"
#include "path_utils.h"
#include "compress.h"
#include "sqlite3.h"
#include "zlib.h"

#define dbg(x) DebugLogFL((x),DC::Main)
static auto open_db( const fs::path &path ) -> sqlite3 *
{
    sqlite3 *db = nullptr;
    int ret;
    const auto path_string = cata_files::path_to_generic_utf8( path );

    ret = sqlite3_initialize();
    if( ret != SQLITE_OK ) {
        dbg( DL::Error ) << "Failed to initialize sqlite3 (Error " << ret << ")";
        throw std::runtime_error( "Failed to initialize sqlite3" );
    }

    ret = sqlite3_open_v2( path_string.c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL );
    if( ret != SQLITE_OK ) {
        dbg( DL::Error ) << "Failed to open db" << path_string << " (Error " << ret << ")";
        throw std::runtime_error( "Failed to open db" );
    }

    auto sql = R"sql(
        CREATE TABLE IF NOT EXISTS files (
            path           TEXT PRIMARY KEY NOT NULL,
            parent         TEXT NOT NULL,
            compression    TEXT DEFAULT NULL,
            data           BLOB NOT NULL
        );
    )sql";

    char *sqlErrMsg = 0;
    ret = sqlite3_exec( db, sql, NULL, NULL, &sqlErrMsg );
    if( ret != SQLITE_OK ) {
        dbg( DL::Error ) << "Failed to init db" << path_string << " (" << sqlErrMsg << ")";
        throw std::runtime_error( "Failed to open db" );
    }

    return db;
}

save_t::save_t( const std::string &name ): name( name ) {}

std::string save_t::decoded_name() const
{
    return name;
}

auto save_t::base_path() const -> fs::path
{
    return base64_encode( name );
}

save_t save_t::from_save_id( const std::string &save_id )
{
    return save_t( save_id );
}

save_t save_t::from_base_path( const fs::path &base_path )
{
    return save_t( base64_decode( cata_files::path_to_generic_utf8( base_path ) ) );
}

auto WORLDINFO::folder_path() const -> fs::path
{
    return PATH_INFO::savedir() / world_name;
}

WORLDINFO::WORLDINFO()
{
    world_name = world_generator->get_next_valid_worldname();
    WORLD_OPTIONS = get_options().get_world_defaults();
    world_save_format = save_format::V2_COMPRESSED_SQLITE3;

    world_saves.clear();
    active_mod_order = world_generator->get_mod_manager().get_default_mods();
}

void WORLDINFO::COPY_WORLD( const WORLDINFO *world_to_copy )
{
    world_name = world_to_copy->world_name + "_copy";
    WORLD_OPTIONS = world_to_copy->WORLD_OPTIONS;
    world_save_format = world_to_copy->world_save_format;
    active_mod_order = world_to_copy->active_mod_order;
}

bool WORLDINFO::save_exists( const save_t &name ) const
{
    return std::ranges::contains( world_saves, name );
}

void WORLDINFO::add_save( const save_t &name )
{
    if( !save_exists( name ) ) {
        world_saves.push_back( name );
    }
}

void WORLDINFO::load_options( JsonIn &jsin )
{
    auto &opts = get_options();

    jsin.start_array();
    while( !jsin.end_array() ) {
        JsonObject jo = jsin.get_object();
        jo.allow_omitted_members();
        const std::string name = opts.migrateOptionName( jo.get_string( "name" ) );
        const std::string value = opts.migrateOptionValue( jo.get_string( "name" ),
                                  jo.get_string( "value" ) );

        if( opts.has_option( name ) && opts.get_option( name ).getPage() == "world_default" ) {
            WORLD_OPTIONS[ name ].setValue( value );
        }
    }
}

void WORLDINFO::load_legacy_options( std::istream &fin )
{
    auto &opts = get_options();

    //load legacy txt
    std::string sLine;
    while( !fin.eof() ) {
        getline( fin, sLine );
        if( !sLine.empty() && sLine[0] != '#' && std::count( sLine.begin(), sLine.end(), ' ' ) == 1 ) {
            size_t ipos = sLine.find( ' ' );
            // make sure that the option being loaded is part of the world_default page in OPTIONS
            // In 0.C some lines consisted of a space and nothing else
            const std::string name = opts.migrateOptionName( sLine.substr( 0, ipos ) );
            const std::string value = opts.migrateOptionValue( sLine.substr( 0, ipos ), sLine.substr( ipos + 1,
                                      sLine.length() ) );

            if( ipos != 0 && opts.get_option( name ).getPage() == "world_default" ) {
                WORLD_OPTIONS[name].setValue( value );
            }
        }
    }
}

bool WORLDINFO::load_options()
{
    WORLD_OPTIONS = get_options().get_world_defaults();

    using namespace std::placeholders;
    const auto path = folder_path() / PATH_INFO::worldoptions();
    return read_from_file_json( path, [&]( JsonIn & jsin ) {
        load_options( jsin );
    }, true );
}

bool WORLDINFO::save( const bool is_conversion ) const
{
    if( !assure_dir_exist( folder_path() ) ) {
        DebugLog( DL::Error, DC::Main ) << "Unable to create or open world[" << world_name
                                        << "] directory for saving";
        return false;
    }

    if( !is_conversion ) {
        const auto savefile = folder_path() / PATH_INFO::worldoptions();
        const bool saved = write_to_file( savefile, [&]( std::ostream & fout ) {
            JsonOut jout( fout );

            jout.start_array();

            for( auto &elem : WORLD_OPTIONS ) {
                // Skip hidden option because it is set by mod and should not be saved
                if( !elem.second.getDefaultText().empty() ) {
                    jout.start_object();

                    jout.member( "info", elem.second.getTooltip() );
                    jout.member( "default", elem.second.getDefaultText( false ) );
                    jout.member( "name", elem.first );
                    jout.member( "value", elem.second.getValue( true ) );

                    jout.end_object();
                }
            }

            jout.end_array();
        }, _( "world data" ) );
        if( !saved ) {
            return false;
        }
    }

    world_generator->get_mod_manager().save_mods_list( const_cast<WORLDINFO *>( this ) );

    // If the world is a V2 world and there's no SQLite3 file yet, create a blank one.
    // We infer that the world is V2 if there's a map.sqlite3 file in the world directory.
    // When a world is freshly created, we need to create a file here to remember the users'
    // choice of world save format.
    if( world_save_format == save_format::V2_COMPRESSED_SQLITE3 &&
        !file_exist( folder_path() / "map.sqlite3" ) ) {
        sqlite3 *db = open_db( folder_path() / "map.sqlite3" );
        sqlite3_close( db );
    }
    return true;
}

void load_world_option( const JsonObject &jo )
{
    auto arr = jo.get_array( "options" );
    if( arr.empty() ) {
        jo.throw_error( "no options specified", "options" );
    }
    for( const std::string line : arr ) {
        get_options().get_option( line ).setValue( "true" );
    }
}

//load external option from json
void load_external_option( const JsonObject &jo )
{
    auto name = jo.get_string( "name" );
    auto stype = jo.get_string( "stype" );
    options_manager &opts = get_options();
    if( !opts.has_option( name ) ) {
        auto sinfo = jo.get_string( "info" );
        opts.add_external( name, "external_options", stype, sinfo, sinfo );
    }
    options_manager::cOpt &opt = opts.get_option( name );
    if( stype == "float" ) {
        opt.setValue( static_cast<float>( jo.get_float( "value" ) ) );
    } else if( stype == "int" ) {
        opt.setValue( jo.get_int( "value" ) );
    } else if( stype == "bool" ) {
        if( jo.get_bool( "value" ) ) {
            opt.setValue( "true" );
        } else {
            opt.setValue( "false" );
        }
    } else if( stype == "string" ) {
        opt.setValue( jo.get_string( "value" ) );
    } else {
        jo.throw_error( "Unknown or unsupported stype for external option", "stype" );
    }
    // Just visit this member if it exists
    if( jo.has_member( "info" ) ) {
        jo.get_string( "info" );
    }
}

static auto file_exist_in_db( sqlite3 *db, const fs::path &path ) -> bool
{
    int fileCount = 0;
    const auto path_string = cata_files::path_to_generic_utf8( path );
    const char *sql = "SELECT count() FROM files WHERE path = :path";
    sqlite3_stmt *stmt = nullptr;

    if( sqlite3_prepare_v2( db, sql, -1, &stmt, nullptr ) != SQLITE_OK ) {
        dbg( DL::Error ) << "Failed to prepare statement: " << sqlite3_errmsg( db ) << '\n';
        throw std::runtime_error( "DB query failed" );
    }

    if( sqlite3_bind_text( stmt, sqlite3_bind_parameter_index( stmt, ":path" ), path_string.c_str(), -1,
                           SQLITE_TRANSIENT ) != SQLITE_OK ) {
        dbg( DL::Error ) << "Failed to bind parameter: " << sqlite3_errmsg( db ) << '\n';
        sqlite3_finalize( stmt );
        throw std::runtime_error( "DB query failed" );
    }

    if( sqlite3_step( stmt ) == SQLITE_ROW ) {
        // Retrieve the count result
        fileCount = sqlite3_column_int( stmt, 0 );
    } else {
        dbg( DL::Error ) << "Failed to execute query: " << sqlite3_errmsg( db ) << '\n';
        sqlite3_finalize( stmt );
        throw std::runtime_error( "DB query failed" );
    }

    sqlite3_finalize( stmt );

    return fileCount > 0;
}

static auto write_to_db( sqlite3 *db, const fs::path &path, file_write_fn writer ) -> void
{
    const auto path_string = cata_files::path_to_generic_utf8( path );
    std::ostringstream oss;
    writer( oss );
    auto data = oss.str();

    std::vector<std::byte> compressedData;
    zlib_compress( data, compressedData );

    auto parent = cata_files::path_to_generic_utf8( path.parent_path() );

    auto sql = R"sql(
        INSERT INTO files(path, parent, data, compression)
        VALUES (:path, :parent, :data, 'zlib')
        ON CONFLICT(path) DO UPDATE
            SET data = excluded.data,
                parent = excluded.parent,
                compression = excluded.compression;
    )sql";

    sqlite3_stmt *stmt = nullptr;

    if( sqlite3_prepare_v2( db, sql, -1, &stmt, nullptr ) != SQLITE_OK ) {
        dbg( DL::Error ) << "Failed to prepare statement: " << sqlite3_errmsg( db ) << '\n';
        throw std::runtime_error( "DB query failed" );
    }

    if( sqlite3_bind_text( stmt, sqlite3_bind_parameter_index( stmt, ":path" ), path_string.c_str(), -1,
                           SQLITE_TRANSIENT ) != SQLITE_OK ||
        sqlite3_bind_text( stmt, sqlite3_bind_parameter_index( stmt, ":parent" ), parent.c_str(), -1,
                           SQLITE_TRANSIENT ) != SQLITE_OK ||
        sqlite3_bind_blob( stmt, sqlite3_bind_parameter_index( stmt, ":data" ), compressedData.data(),
                           compressedData.size(), SQLITE_TRANSIENT ) != SQLITE_OK ) {
        dbg( DL::Error ) << "Failed to bind parameters: " << sqlite3_errmsg( db ) << '\n';
        sqlite3_finalize( stmt );
        throw std::runtime_error( "DB query failed" );
    }

    if( sqlite3_step( stmt ) != SQLITE_DONE ) {
        dbg( DL::Error ) << "Failed to execute query: " << sqlite3_errmsg( db ) << '\n';
    }
    sqlite3_finalize( stmt );
}

static auto read_from_db( sqlite3 *db, const fs::path &path, file_read_fn reader,
                          bool optional ) -> bool
{
    const auto path_string = cata_files::path_to_generic_utf8( path );
    const char *sql = "SELECT data, compression FROM files WHERE path = :path LIMIT 1";

    sqlite3_stmt *stmt = nullptr;

    if( sqlite3_prepare_v2( db, sql, -1, &stmt, nullptr ) != SQLITE_OK ) {
        dbg( DL::Error ) << "Failed to prepare statement: " << sqlite3_errmsg( db ) << '\n';
        throw std::runtime_error( "DB query failed" );
    }

    if( sqlite3_bind_text( stmt, sqlite3_bind_parameter_index( stmt, ":path" ), path_string.c_str(), -1,
                           SQLITE_TRANSIENT ) != SQLITE_OK ) {
        dbg( DL::Error ) << "Failed to bind parameter: " << sqlite3_errmsg( db ) << '\n';
        sqlite3_finalize( stmt );
        throw std::runtime_error( "DB query failed" );
    }

    if( sqlite3_step( stmt ) == SQLITE_ROW ) {
        // Retrieve the count result
        const void *blobData = sqlite3_column_blob( stmt, 0 );
        int blobSize = sqlite3_column_bytes( stmt, 0 );
        auto compression_raw = sqlite3_column_text( stmt, 1 );
        std::string compression = compression_raw ? reinterpret_cast<const char *>( compression_raw ) : "";

        if( blobData == nullptr ) {
            return false; // Return an empty string if there's no data
        }

        std::string dataString;
        if( compression.empty() ) {
            dataString = std::string( static_cast<const char *>( blobData ), blobSize );
        } else if( compression == "zlib" ) {
            zlib_decompress( blobData, blobSize, dataString );
        } else {
            throw std::runtime_error( "Unknown compression format: " + compression );
        }

        std::istringstream stream( dataString );
        reader( stream );
        sqlite3_finalize( stmt );
    } else {
        auto err = sqlite3_errmsg( db );
        sqlite3_finalize( stmt );

        if( !optional ) {
            dbg( DL::Error ) << "Failed to execute query: " << err << '\n';
            throw std::runtime_error( "DB query failed" );
        }
        return false;
    }

    return true;
}

static auto read_from_db_json( sqlite3 *db, const fs::path &path, file_read_json_fn reader,
                               bool optional ) -> bool
{
    return read_from_db( db, path, [&]( std::istream & fin ) {
        JsonIn jsin( fin, cata_files::path_to_generic_utf8( path ) );
        reader( jsin );
    }, optional );
}

world::world( WORLDINFO *info )
    : info( info )
    , save_tx_start_ts( 0 )
{
    if( !assure_dir_exist( "" ) ) {
        dbg( DL::Error ) << "Unable to create or open world directory structure: " << info->folder_path();
    }

    if( info->world_save_format == save_format::V2_COMPRESSED_SQLITE3 ) {
        map_db = open_db( info->folder_path() / "map.sqlite3" );
    } else {
        if( !assure_dir_exist( "/maps" ) ) {
            dbg( DL::Error ) << "Unable to create or open world directory structure: " << info->folder_path();
        }
    }
}

world::~world()
{
    if( save_tx_start_ts != 0 ) {
        dbg( DL::Error ) << "Save transaction was not committed before world destruction";
    }

    if( map_db ) {
        sqlite3_close( map_db );
    }

    if( save_db ) {
        sqlite3_close( save_db );
    }
}

void world::release_player_db()
{
    if( save_db ) {
        sqlite3_close( save_db );
        save_db = nullptr;
        last_save_id.clear();
    }
}

void world::start_save_tx()
{
    if( save_tx_start_ts != 0 ) {
        throw std::runtime_error( "Attempted to start a save transaction while one was already in progress" );
    }
    save_tx_start_ts = std::chrono::duration_cast< std::chrono::milliseconds >(
                           std::chrono::system_clock::now().time_since_epoch()
                       ).count();

    if( map_db ) {
        sqlite3_exec( map_db, "BEGIN TRANSACTION", NULL, NULL, NULL );
    }

    if( save_db ) {
        sqlite3_exec( save_db, "BEGIN TRANSACTION", NULL, NULL, NULL );
    }
}

int64_t world::commit_save_tx()
{
    if( save_tx_start_ts == 0 ) {
        throw std::runtime_error( "Attempted to commit a save transaction while none was in progress" );
    }

    if( map_db ) {
        sqlite3_exec( map_db, "COMMIT", NULL, NULL, NULL );
    }

    if( save_db ) {
        sqlite3_exec( save_db, "COMMIT", NULL, NULL, NULL );
    }

    int64_t now = std::chrono::duration_cast< std::chrono::milliseconds >(
                      std::chrono::system_clock::now().time_since_epoch()
                  ).count();
    int64_t duration = now - save_tx_start_ts;
    save_tx_start_ts = 0;
    return duration;
}

/**
 * DOMAIN SPECIFIC: MAP
 */

// Path helpers — dimension-aware.
// Primary dimension (dim_id == ""): maps/<seg>/..., o.<x>.<y>, .seen.<x>.<y>, <x>.<y>.<z>.mmr
// Non-primary (dim_id != ""):      dimensions/<dim_id>/maps/<seg>/..., etc.
// dim_prefix_path() prepends "dimensions/<dim_id>/" when dim_id is non-empty.

static auto dim_prefix_path( const std::string &dim_id ) -> fs::path
{
    return dim_id.empty() ? fs::path{} :
           fs::path( "dimensions" ) / dim_id;
}

static auto get_omt_dirname( const std::string &dim_id,
                             const tripoint_abs_omt &omt_addr ) -> fs::path
{
    const auto segment_addr = project_to<coords::seg>( omt_addr );
    return dim_prefix_path( dim_id ) / "maps" / string_format( "%d.%d.%d",
            segment_addr.x(), segment_addr.y(), segment_addr.z() );
}

static auto get_omt_filename( const tripoint_abs_omt &omt_addr ) -> fs::path
{
    return string_format( "%d.%d.%d.map", omt_addr.x(), omt_addr.y(), omt_addr.z() );
}

static auto get_overmap_terrain_filename( const std::string &dim_id,
        const point_abs_om &p ) -> fs::path
{
    return dim_prefix_path( dim_id ) / string_format( "o.%d.%d", p.x(), p.y() );
}

static auto get_overmap_player_filename( const std::string &dim_id,
        const point_abs_om &p ) -> fs::path
{
    return dim_prefix_path( dim_id ) / string_format( ".seen.%d.%d", p.x(), p.y() );
}

static auto get_mm_filename( const std::string &dim_id, const tripoint_abs_mmr &p ) -> fs::path
{
    return dim_prefix_path( dim_id ) / string_format( "%d.%d.%d.mmr", p.x(), p.y(), p.z() );
}

bool world::read_map_omt( const std::string &dim_id, const tripoint_abs_omt &omt_addr,
                          file_read_json_fn reader ) const
{
    const auto dirname = get_omt_dirname( dim_id, omt_addr );
    auto omt_path = dirname / get_omt_filename( omt_addr );

    if( info->world_save_format == save_format::V2_COMPRESSED_SQLITE3 ) {
        return read_from_db_json( map_db, omt_path, reader, true );
    } else {
        if( !file_exist( omt_path ) ) {
            // Fix for old saves where the path was generated using std::stringstream,
            // which may insert locale-specific thousands separators.
            std::ostringstream buf;
            buf << omt_addr.x() << "." << omt_addr.y() << "." << omt_addr.z() << ".map";
            const auto locale_omt_path = dirname / buf.str();
            if( file_exist( locale_omt_path ) ) {
                omt_path = locale_omt_path;
            }
        }
        return read_from_file_json( omt_path, reader, true );
    }
}

bool world::write_map_omt( const std::string &dim_id, const tripoint_abs_omt &omt_addr,
                           file_write_fn writer ) const
{
    const auto dirname = get_omt_dirname( dim_id, omt_addr );
    const auto omt_path = dirname / get_omt_filename( omt_addr );

    if( info->world_save_format == save_format::V2_COMPRESSED_SQLITE3 ) {
        write_to_db( map_db, omt_path, writer );
        return true;
    } else {
        assure_dir_exist( dirname );
        return write_to_file( omt_path, writer );
    }
}

bool world::overmap_exists( const std::string &dim_id, const point_abs_om &p ) const
{
    const auto fname = get_overmap_terrain_filename( dim_id, p );
    if( info->world_save_format == save_format::V2_COMPRESSED_SQLITE3 ) {
        return file_exist_in_db( map_db, fname );
    } else {
        return file_exist( fname );
    }
}

bool world::read_overmap( const std::string &dim_id, const point_abs_om &p,
                          file_read_fn reader ) const
{
    const auto fname = get_overmap_terrain_filename( dim_id, p );
    if( info->world_save_format == save_format::V2_COMPRESSED_SQLITE3 ) {
        return read_from_db( map_db, fname, reader, true );
    } else {
        return read_from_file( fname, reader, true );
    }
}

bool world::read_overmap_player_visibility( const std::string &dim_id, const point_abs_om &p,
        file_read_fn reader )
{
    const auto fname = get_overmap_player_filename( dim_id, p );
    if( info->world_save_format == save_format::V2_COMPRESSED_SQLITE3 ) {
        sqlite3 *playerdb = get_player_db();
        return read_from_db( playerdb, fname, reader, true );
    } else {
        return read_from_player_file( fname, reader, true );
    }
}

bool world::write_overmap( const std::string &dim_id, const point_abs_om &p,
                           file_write_fn writer ) const
{
    const auto fname = get_overmap_terrain_filename( dim_id, p );
    if( info->world_save_format == save_format::V2_COMPRESSED_SQLITE3 ) {
        write_to_db( map_db, fname, writer );
        return true;
    } else {
        return write_to_file( fname, writer );
    }
}

bool world::write_overmap_player_visibility( const std::string &dim_id, const point_abs_om &p,
        file_write_fn writer )
{
    const auto fname = get_overmap_player_filename( dim_id, p );
    if( info->world_save_format == save_format::V2_COMPRESSED_SQLITE3 ) {
        sqlite3 *playerdb = get_player_db();
        write_to_db( playerdb, fname, writer );
        return true;
    } else {
        return write_to_player_file( fname, writer );
    }
}

bool world::read_player_mm_omt( const std::string &dim_id, const tripoint_abs_mmr &p,
                                file_read_json_fn reader )
{
    const auto fname = get_mm_filename( dim_id, p );
    if( info->world_save_format == save_format::V2_COMPRESSED_SQLITE3 ) {
        sqlite3 *playerdb = get_player_db();
        return read_from_db_json( playerdb, fname, reader, true );
    } else {
        return read_from_player_file_json( fs::path( ".mm1" ) / fname, reader, true );
    }
}

// Legacy overloads — do not call from background threads.

static std::string legacy_dim_id()
{
    // Use the overmapbuffer-registry global rather than g->get_dimension_prefix().
    // Both are kept in sync during normal gameplay, but save_all_overmapbuffers()
    // temporarily overrides g_active_dimension_id so that each dimension's overmaps
    // are written to their own path.  Reading the global here makes that override work.
    return g_active_dimension_id;
}

bool world::read_map_omt( const tripoint_abs_omt &omt_addr, file_read_json_fn reader ) const
{
    return read_map_omt( legacy_dim_id(), omt_addr, reader );
}

bool world::write_map_omt( const tripoint_abs_omt &omt_addr, file_write_fn writer ) const
{
    return write_map_omt( legacy_dim_id(), omt_addr, writer );
}

/**
 * DOMAIN SPECIFIC: OVERMAP
 */

auto world::overmap_terrain_filename( const point_abs_om &p ) const -> fs::path
{
    return get_overmap_terrain_filename( legacy_dim_id(), p );
}

auto world::overmap_player_filename( const point_abs_om &p ) const -> fs::path
{
    return get_overmap_player_filename( legacy_dim_id(), p );
}

bool world::overmap_exists( const point_abs_om &p ) const
{
    return overmap_exists( legacy_dim_id(), p );
}

bool world::read_overmap( const point_abs_om &p, file_read_fn reader ) const
{
    return read_overmap( legacy_dim_id(), p, reader );
}

bool world::read_overmap_player_visibility( const point_abs_om &p, file_read_fn reader )
{
    return read_overmap_player_visibility( legacy_dim_id(), p, reader );
}

bool world::write_overmap( const point_abs_om &p, file_write_fn writer ) const
{
    return write_overmap( legacy_dim_id(), p, writer );
}

bool world::write_overmap_player_visibility( const point_abs_om &p, file_write_fn writer )
{
    return write_overmap_player_visibility( legacy_dim_id(), p, writer );
}

/**
 * DOMAIN SPECIFIC: MAP MEMORY
 */

bool world::read_player_mm_omt( const tripoint_abs_mmr &p, file_read_json_fn reader )
{
    return read_player_mm_omt( legacy_dim_id(), p, reader );
}

bool world::write_player_mm_omt( const std::string &dim_id, const tripoint_abs_mmr &p,
                                 file_write_fn writer )
{
    const auto fname = get_mm_filename( dim_id, p );
    if( info->world_save_format == save_format::V2_COMPRESSED_SQLITE3 ) {
        sqlite3 *playerdb = get_player_db();
        write_to_db( playerdb, fname, writer );
        return true;
    } else {
        auto mm_dir_suffix = fs::path( ".mm1" ) / dim_prefix_path( dim_id );
        auto mm_dir = get_player_path();
        mm_dir += cata_files::path_to_generic_utf8( mm_dir_suffix );
        assure_dir_exist( mm_dir );
        const std::string descr = string_format(
                                      _( "memory map region for (%d,%d,%d)" ),
                                      p.x(), p.y(), p.z() );
        return write_to_player_file( fs::path( ".mm1" ) / fname, writer, descr.c_str() );
    }
}

bool world::write_player_mm_omt( const tripoint_abs_mmr &p, file_write_fn writer )
{
    return write_player_mm_omt( legacy_dim_id(), p, writer );
}

/**
 * PLAYER OPERATIONS
 */

auto world::get_player_path() const -> fs::path
{
    return base64_encode( g->u.get_save_id() );
}

sqlite3 *world::get_player_db()
{
    if( !save_db ) {
        auto player_db_path = info->folder_path() / get_player_path();
        player_db_path += ".sqlite3";
        save_db = open_db( player_db_path );
        last_save_id = g->u.get_save_id();
    }

    if( last_save_id != g->u.get_save_id() ) {
        ::copy_file(
            info->folder_path() / ( base64_encode( last_save_id ) + ".sqlite3" ),
            info->folder_path() / ( base64_encode( g->u.get_save_id() ) + ".sqlite3" )
        );
        auto player_db_path = info->folder_path() / get_player_path();
        player_db_path += ".sqlite3";
        save_db = open_db( player_db_path );
    }

    return save_db;
}

namespace
{

auto append_player_path( const fs::path &player_path, const fs::path &path ) -> fs::path
{
    auto result = player_path;
    result += cata_files::path_to_generic_utf8( path );
    return result;
}

auto world_relative_path( const fs::path &path ) -> fs::path
{
    return path.is_absolute() ? path.relative_path() : path;
}

} // namespace

auto world::player_file_exist( const fs::path &path ) -> bool
{
    return file_exist( append_player_path( get_player_path(), path ) );
}

auto world::write_to_player_file( const fs::path &path, file_write_fn writer,
                                  const char *fail_message ) -> bool
{
    return write_to_file( append_player_path( get_player_path(), path ), writer, fail_message );
}

auto world::read_from_player_file( const fs::path &path, file_read_fn reader,
                                   bool optional ) -> bool
{
    return read_from_file( append_player_path( get_player_path(), path ), reader, optional );
}

auto world::read_from_player_file_json( const fs::path &path, file_read_json_fn reader,
                                        bool optional ) -> bool
{
    return read_from_file_json( append_player_path( get_player_path(), path ), reader, optional );
}

/**
 * GENERIC OPERATIONS
 */

auto world::assure_dir_exist( const fs::path &path ) const -> bool
{
    return ::assure_dir_exist( info->folder_path() / world_relative_path( path ) );
}

auto world::file_exist( const fs::path &path ) const -> bool
{
    return ::file_exist( info->folder_path() / world_relative_path( path ) );
}

auto world::write_to_file( const fs::path &path, file_write_fn writer,
                           const char *fail_message ) const -> bool
{
    return ::write_to_file( info->folder_path() / world_relative_path( path ), writer, fail_message );
}

auto world::read_from_file( const fs::path &path, file_read_fn reader,
                            bool optional ) const -> bool
{
    return ::read_from_file( info->folder_path() / world_relative_path( path ), reader, optional );
}

auto world::read_from_file_json( const fs::path &path, file_read_json_fn reader,
                                 bool optional ) const -> bool
{
    return ::read_from_file_json( info->folder_path() / world_relative_path( path ), reader, optional );
}

/**
 * Save Conversion
 */
void world::convert_from_v1( const std::unique_ptr<WORLDINFO> &old_world )
{
    dbg( DL::Info ) << "Converting world '" << info->world_name << "' from v1 to v2 format";

    // The map database should already be loaded via the constructor.
    // The save database(s) will need to be created separately here.
    // Transactions are mostly being used for performance reasons rather than consistency.
    sqlite3_exec( map_db, "BEGIN TRANSACTION", NULL, NULL, NULL );

    // Keep track of the last used save DB
    sqlite3 *last_save_db = nullptr;
    std::string last_save_id;

    // Begin copying files to the new world folder.
    // This method is BFS, so we'll need to run two passes to keep player-specific
    // files together.
    const auto old_world_path = old_world->folder_path();
    auto root_paths = get_files_from_path( "", old_world_path, false, true );
    for( const auto &file_path : root_paths ) {
        // Remove the old world path prefix from the file path
        const auto part = cata_files::path_to_generic_utf8( fs::relative( file_path, old_world_path ) );

        // Migrate contents of the maps/ directory into the map database
        if( part == "maps" ) {
            // Recurse down the directory tree and migrate files into sqlite.
            auto subpaths = get_files_from_path( "", file_path, true, true );
            for( const auto &subpath : subpaths ) {
                const auto map_path = cata_files::path_to_generic_utf8( fs::path( "maps" ) /
                                      fs::relative( subpath, file_path ) );
                if( !map_path.ends_with( ".map" ) ) {
                    continue;
                }
                ::read_from_file( subpath, [&]( std::istream & fin ) {
                    write_to_db( map_db, map_path, [&]( std::ostream & fout ) {
                        fout << fin.rdbuf();
                    } );
                } );
            }
            continue;
        }

        // Migrate o.* files into the map database
        if( part.starts_with( "o." ) ) {
            ::read_from_file( file_path, [&]( std::istream & fin ) {
                write_to_db( map_db, part, [&]( std::ostream & fout ) {
                    fout << fin.rdbuf();
                } );
            } );
            continue;
        }

        // Handle player-specific prefixed files
        if( part.find( ".seen." ) != std::string::npos || part.find( ".mm1" ) != std::string::npos ) {
            auto save_id = part.substr( 0, part.find( '.' ) );
            if( save_id != last_save_id ) {
                if( last_save_db ) {
                    sqlite3_exec( last_save_db, "COMMIT", NULL, NULL, NULL );
                    sqlite3_close( last_save_db );
                }
                last_save_db = open_db( info->folder_path() / ( save_id + ".sqlite3" ) );
                last_save_id = save_id;
                sqlite3_exec( last_save_db, "BEGIN TRANSACTION", NULL, NULL, NULL );
            }

            if( part.find( ".seen." ) != std::string::npos ) {
                ::read_from_file( file_path, [&]( std::istream & fin ) {
                    write_to_db( last_save_db, part.substr( save_id.size() ), [&]( std::ostream & fout ) {
                        fout << fin.rdbuf();
                    } );
                } );
            } else {
                // Recurse down the directory tree and migrate files into sqlite.
                auto subpaths = get_files_from_path( "", file_path, true, true );
                for( const auto &subpath : subpaths ) {
                    const auto map_path = cata_files::path_to_generic_utf8( fs::relative( subpath,
                                          file_path ) );
                    if( dir_exist( subpath ) ) {
                        continue;
                    }
                    ::read_from_file( subpath, [&]( std::istream & fin ) {
                        write_to_db( last_save_db, map_path, [&]( std::ostream & fout ) {
                            fout << fin.rdbuf();
                        } );
                    } );
                }
            }

            continue;
        }

        // Copy all other files as-is
        if( file_exist( file_path ) ) {
            ::copy_file( file_path, info->folder_path() / part );
        }
    }

    if( last_save_db ) {
        sqlite3_exec( last_save_db, "COMMIT", NULL, NULL, NULL );
        sqlite3_close( last_save_db );
    }

    sqlite3_exec( map_db, "COMMIT", NULL, NULL, NULL );
}
