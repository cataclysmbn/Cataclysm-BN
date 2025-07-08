#include "world.h"

#include <algorithm>
#include <sstream>
#include <cstring>
#include <chrono>

#include "game.h"
#include "avatar.h"
#include "debug.h"
#include "cata_utility.h"
#include "filesystem.h"
#include "output.h"
#include "worldfactory.h"
#include "mod_manager.h"
#include "path_info.h"
#include "compress.h"
#include "sqlite3.h"
#include "zlib.h"

#define dbg(x) DebugLogFL((x),DC::Main)
static sqlite3 *open_db( const std::string &path )
{
    sqlite3 *db = nullptr;
    int ret;

    ret = sqlite3_initialize();
    if( ret != SQLITE_OK ) {
        dbg( DL::Error ) << "Failed to initialize sqlite3 (Error " << ret << ")";
        throw std::runtime_error( "Failed to initialize sqlite3" );
    }

    ret = sqlite3_open_v2( path.c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL );
    if( ret != SQLITE_OK ) {
        dbg( DL::Error ) << "Failed to open db" << path << " (Error " << ret << ")";
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
        dbg( DL::Error ) << "Failed to init db" << path << " (" << sqlErrMsg << ")";
        throw std::runtime_error( "Failed to open db" );
    }

    return db;
}

save_t::save_t( const std::string &name ): name( name ) {}

std::string save_t::decoded_name() const
{
    return name;
}

std::string save_t::base_path() const
{
    return base64_encode( name );
}

save_t save_t::from_save_id( const std::string &save_id )
{
    return save_t( save_id );
}

save_t save_t::from_base_path( const std::string &base_path )
{
    return save_t( base64_decode( base_path ) );
}

std::string WORLDINFO::folder_path() const
{
    return PATH_INFO::savedir() + world_name;
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

bool WORLDINFO::needs_lua() const
{
    for( const mod_id &mod : active_mod_order ) {
        if( mod.is_valid() && mod->lua_api_version ) {
            return true;
        }
    }
    return false;
}

bool WORLDINFO::save_exists( const save_t &name ) const
{
    return std::ranges::find( world_saves, name ) != world_saves.end();
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
    const auto path = folder_path() + "/" + PATH_INFO::worldoptions();
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
        const auto savefile = folder_path() + "/" + PATH_INFO::worldoptions();
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
        !file_exist( folder_path() + "/map.sqlite3" ) ) {
        sqlite3 *db = open_db( folder_path() + "/map.sqlite3" );
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

static bool file_exist_in_db( sqlite3 *db, const std::string &path )
{
    int fileCount = 0;
    const char *sql = "SELECT count() FROM files WHERE path = :path";
    sqlite3_stmt *stmt = nullptr;

    if( sqlite3_prepare_v2( db, sql, -1, &stmt, nullptr ) != SQLITE_OK ) {
        dbg( DL::Error ) << "Failed to prepare statement: " << sqlite3_errmsg( db ) << '\n';
        throw std::runtime_error( "DB query failed" );
    }

    if( sqlite3_bind_text( stmt, sqlite3_bind_parameter_index( stmt, ":path" ), path.c_str(), -1,
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

static void write_to_db( sqlite3 *db, const std::string &path, file_write_fn writer )
{
    std::ostringstream oss;
    writer( oss );
    auto data = oss.str();

    std::vector<std::byte> compressedData;
    zlib_compress( data, compressedData );

    size_t basePos = path.find_last_of( "/\\" );
    auto parent = ( basePos == std::string::npos ) ? "" : path.substr( 0, basePos );

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

    if( sqlite3_bind_text( stmt, sqlite3_bind_parameter_index( stmt, ":path" ), path.c_str(), -1,
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

static bool read_from_db( sqlite3 *db, const std::string &path, file_read_fn reader,
                          bool optional )
{
    const char *sql = "SELECT data, compression FROM files WHERE path = :path LIMIT 1";

    sqlite3_stmt *stmt = nullptr;

    if( sqlite3_prepare_v2( db, sql, -1, &stmt, nullptr ) != SQLITE_OK ) {
        dbg( DL::Error ) << "Failed to prepare statement: " << sqlite3_errmsg( db ) << '\n';
        throw std::runtime_error( "DB query failed" );
    }

    if( sqlite3_bind_text( stmt, sqlite3_bind_parameter_index( stmt, ":path" ), path.c_str(), -1,
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

static bool read_from_db_json( sqlite3 *db, const std::string &path, file_read_json_fn reader,
                               bool optional )
{
    return read_from_db( db, path, [&]( std::istream & fin ) {
        JsonIn jsin( fin, path );
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
        map_db = open_db( info->folder_path() + "/map.sqlite3" );
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

static std::string get_quad_dirname( const tripoint &om_addr )
{
    const tripoint segment_addr = omt_to_seg_copy( om_addr );
    return string_format( "maps/%d.%d.%d", segment_addr.x, segment_addr.y, segment_addr.z );
}

static std::string get_quad_filename( const tripoint &om_addr )
{
    return string_format( "%d.%d.%d.map", om_addr.x, om_addr.y, om_addr.z );
}

bool world::read_map_quad( const tripoint &om_addr, file_read_json_fn reader ) const
{
    const std::string dirname = get_quad_dirname( om_addr );
    std::string quad_path = dirname + "/" + get_quad_filename( om_addr );

    // V2 logic
    if( info->world_save_format == save_format::V2_COMPRESSED_SQLITE3 ) {
        return read_from_db_json( map_db, quad_path, reader, true );
    } else {
        if( !file_exist( quad_path ) ) {
            // Fix for old saves where the path was generated using std::stringstream, which
            // did format the number using the current locale. That formatting may insert
            // thousands separators, so the resulting path is "map/1,234.7.8.map" instead
            // of "map/1234.7.8.map".
            std::ostringstream buffer;
            buffer << dirname << "/" << om_addr.x << "." << om_addr.y << "." << om_addr.z << ".map";
            if( file_exist( buffer.str() ) ) {
                quad_path = buffer.str();
            }
        }

        return read_from_file_json( quad_path, reader, true );
    }
}

bool world::write_map_quad( const tripoint &om_addr, file_write_fn writer ) const
{
    const std::string dirname = get_quad_dirname( om_addr );
    std::string quad_path = dirname + "/" + get_quad_filename( om_addr );

    // V2 logic
    if( info->world_save_format == save_format::V2_COMPRESSED_SQLITE3 ) {
        write_to_db( map_db, quad_path, writer );
        return true;
    } else {
        assure_dir_exist( dirname );
        return write_to_file( quad_path, writer );
    }
}

/**
 * DOMAIN SPECIFIC: OVERMAP
 */

std::string world::overmap_terrain_filename( const point_abs_om &p ) const
{
    return string_format( "o.%d.%d", p.x(), p.y() );
}

std::string world::overmap_player_filename( const point_abs_om &p ) const
{
    return string_format( ".seen.%d.%d", p.x(), p.y() );
}

bool world::overmap_exists( const point_abs_om &p ) const
{
    if( info->world_save_format == save_format::V2_COMPRESSED_SQLITE3 ) {
        return file_exist_in_db( map_db, overmap_terrain_filename( p ) );
    } else {
        return file_exist( overmap_terrain_filename( p ) );
    }
}

bool world::read_overmap( const point_abs_om &p, file_read_fn reader ) const
{
    if( info->world_save_format == save_format::V2_COMPRESSED_SQLITE3 ) {
        return read_from_db( map_db, overmap_terrain_filename( p ), reader, true );
    } else {
        return read_from_file( overmap_terrain_filename( p ), reader, true );
    }
}

bool world::read_overmap_player_visibility( const point_abs_om &p, file_read_fn reader )
{
    if( info->world_save_format == save_format::V2_COMPRESSED_SQLITE3 ) {
        sqlite3 *playerdb = get_player_db();
        return read_from_db( playerdb, overmap_player_filename( p ), reader, true );
    } else {
        return read_from_player_file( overmap_player_filename( p ), reader, true );
    }
}

bool world::write_overmap( const point_abs_om &p, file_write_fn writer ) const
{
    if( info->world_save_format == save_format::V2_COMPRESSED_SQLITE3 ) {
        write_to_db( map_db, overmap_terrain_filename( p ), writer );
        return true;
    } else {
        return write_to_file( overmap_terrain_filename( p ), writer );
    }
}

bool world::write_overmap_player_visibility( const point_abs_om &p, file_write_fn writer )
{
    if( info->world_save_format == save_format::V2_COMPRESSED_SQLITE3 ) {
        sqlite3 *playerdb = get_player_db();
        write_to_db( playerdb, overmap_player_filename( p ), writer );
        return true;
    } else {
        return write_to_player_file( overmap_player_filename( p ), writer );
    }
}

/**
 * DOMAIN SPECIFIC: MAP MEMORY
 */
static std::string get_mm_filename( const tripoint &p )
{
    return string_format( "%d.%d.%d.mmr", p.x, p.y, p.z );
}

bool world::read_player_mm_quad( const tripoint &p, file_read_json_fn reader )
{
    if( info->world_save_format == save_format::V2_COMPRESSED_SQLITE3 ) {
        sqlite3 *playerdb = get_player_db();
        return read_from_db_json( playerdb, get_mm_filename( p ), reader, true );
    } else {
        return read_from_player_file_json( ".mm1/" + get_mm_filename( p ), reader, true );
    }
}

bool world::write_player_mm_quad( const tripoint &p, file_write_fn writer )
{
    if( info->world_save_format == save_format::V2_COMPRESSED_SQLITE3 ) {
        sqlite3 *playerdb = get_player_db();
        write_to_db( playerdb, get_mm_filename( p ), writer );
        return true;
    } else {
        const std::string descr = string_format(
                                      _( "memory map region for (%d,%d,%d)" ),
                                      p.x, p.y, p.z
                                  );
        assure_dir_exist( get_player_path() + ".mm1" );
        return write_to_player_file( ".mm1/" + get_mm_filename( p ), writer, descr.c_str() );
    }
}

/**
 * PLAYER OPERATIONS
 */

std::string world::get_player_path() const
{
    return base64_encode( g->u.get_save_id() );
}

sqlite3 *world::get_player_db()
{
    if( !save_db ) {
        save_db = open_db( info->folder_path() + "/" + get_player_path() + ".sqlite3" );
        last_save_id = g->u.get_save_id();
    }

    if( last_save_id != g->u.get_save_id() ) {
        throw std::runtime_error( "Save ID changed without reloading the world object" );
    }

    return save_db;
}

bool world::player_file_exist( const std::string &path )
{
    return file_exist( get_player_path() + path );
}

bool world::write_to_player_file( const std::string &path, file_write_fn writer,
                                  const char *fail_message )
{
    return write_to_file( get_player_path() + path, writer, fail_message );
}

bool world::read_from_player_file( const std::string &path, file_read_fn reader,
                                   bool optional )
{
    return read_from_file( get_player_path() + path, reader, optional );
}

bool world::read_from_player_file_json( const std::string &path, file_read_json_fn reader,
                                        bool optional )
{
    return read_from_file_json( get_player_path() + path, reader, optional );
}

/**
 * GENERIC OPERATIONS
 */

bool world::assure_dir_exist( const std::string &path ) const
{
    return ::assure_dir_exist( info->folder_path() + "/" + path );
}

bool world::file_exist( const std::string &path ) const
{
    return ::file_exist( info->folder_path() + "/" + path );
}

bool world::write_to_file( const std::string &path, file_write_fn writer,
                           const char *fail_message ) const
{
    return ::write_to_file( info->folder_path() + "/" + path, writer, fail_message );
}

bool world::read_from_file( const std::string &path, file_read_fn reader,
                            bool optional ) const
{
    return ::read_from_file( info->folder_path() + "/" + path, reader, optional );
}

bool world::read_from_file_json( const std::string &path, file_read_json_fn reader,
                                 bool optional ) const
{
    return ::read_from_file_json( info->folder_path() + "/" + path, reader, optional );
}

static void replaceBackslashes( std::string &input )
{
    std::size_t pos = 0;
    while( ( pos = input.find( '\\', pos ) ) != std::string::npos ) {
        input.replace( pos, 1, "/" );
        pos++; // Move past the replaced character
    }
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
    auto old_world_path = old_world->folder_path() + "/";
    auto root_paths = get_files_from_path( "", old_world->folder_path(), false, true );
    for( auto &file_path : root_paths ) {
        // Remove the old world path prefix from the file path
        std::string part = file_path.substr( old_world_path.size() );
        replaceBackslashes( part );

        // Migrate contents of the maps/ directory into the map database
        if( part == "maps" ) {
            // Recurse down the directory tree and migrate files into sqlite.
            auto subpaths = get_files_from_path( "", file_path, true, true );
            for( auto &subpath : subpaths ) {
                std::string map_path = "maps/" + subpath.substr( file_path.size() + 1 );
                replaceBackslashes( map_path );
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
                last_save_db = open_db( info->folder_path() + "/" + save_id + ".sqlite3" );
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
                for( auto &subpath : subpaths ) {
                    std::string map_path = subpath.substr( file_path.size() + 1 );
                    replaceBackslashes( map_path );
                    if( map_path.ends_with( '/' ) ) {
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
        if( !part.ends_with( "/" ) ) {
            copy_file( file_path, info->folder_path() + "/" + part );
        }
    }

    if( last_save_db ) {
        sqlite3_exec( last_save_db, "COMMIT", NULL, NULL, NULL );
        sqlite3_close( last_save_db );
    }

    sqlite3_exec( map_db, "COMMIT", NULL, NULL, NULL );
}
