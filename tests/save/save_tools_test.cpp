#include "catch/catch.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>

#include "game_constants.h"
#include "save/save_tools.h"
#include "sqlite3.h"

namespace
{

class temp_dir
{
    public:
        temp_dir() {
            const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
            path_ = std::filesystem::temp_directory_path() / ( "cbn_save_tools_test_" + std::to_string(
                        stamp ) );
            std::filesystem::create_directories( path_ );
        }

        temp_dir( const temp_dir & ) = delete;
        auto operator=( const temp_dir & ) -> temp_dir & = delete;
        temp_dir( temp_dir &&other ) noexcept : path_( std::move( other.path_ ) ) {
            other.path_.clear();
        }
        auto operator=( temp_dir &&other ) noexcept -> temp_dir& { // *NOPAD*
            if( this != &other ) {
                std::filesystem::remove_all( path_ );
                path_ = std::move( other.path_ );
                other.path_.clear();
            }
            return *this;
        }

        ~temp_dir() {
            std::filesystem::remove_all( path_ );
        }

        auto path() const -> const std::filesystem::path & { return path_; } // *NOPAD*

    private:
        std::filesystem::path path_;
};

auto exec_sql( sqlite3 *db, const char *sql ) -> void
{
    auto *err = static_cast<char *>( nullptr );
    REQUIRE( sqlite3_exec( db, sql, nullptr, nullptr, &err ) == SQLITE_OK );
    sqlite3_free( err );
}

auto open_test_db( const std::filesystem::path &path ) -> sqlite3 * // *NOPAD*
{
    auto *db = static_cast<sqlite3 *>( nullptr );
    REQUIRE( sqlite3_open( path.string().c_str(), &db ) == SQLITE_OK );
    return db;
}

auto insert_file( sqlite3 *db, const std::string &path, const std::string &data ) -> void
{
    auto *stmt = static_cast<sqlite3_stmt *>( nullptr );
    REQUIRE( sqlite3_prepare_v2( db,
                                 "INSERT INTO files(path, parent, compression, data) VALUES(:path, '', NULL, :data)",
                                 -1, &stmt, nullptr ) == SQLITE_OK );
    REQUIRE( sqlite3_bind_text( stmt, sqlite3_bind_parameter_index( stmt, ":path" ), path.c_str(),
                                -1, SQLITE_TRANSIENT ) == SQLITE_OK );
    REQUIRE( sqlite3_bind_blob( stmt, sqlite3_bind_parameter_index( stmt, ":data" ), data.data(),
                                static_cast<int>( data.size() ), SQLITE_TRANSIENT ) == SQLITE_OK );
    REQUIRE( sqlite3_step( stmt ) == SQLITE_DONE );
    REQUIRE( sqlite3_finalize( stmt ) == SQLITE_OK );
}

auto read_file( const std::filesystem::path &path ) -> std::string
{
    auto input = std::ifstream( path, std::ios::binary );
    auto stream = std::ostringstream{};
    stream << input.rdbuf();
    return stream.str();
}

auto row_count( sqlite3 *db, const std::string &where ) -> int
{
    auto *stmt = static_cast<sqlite3_stmt *>( nullptr );
    const auto sql = "SELECT COUNT(*) FROM files WHERE " + where;
    REQUIRE( sqlite3_prepare_v2( db, sql.c_str(), -1, &stmt, nullptr ) == SQLITE_OK );
    REQUIRE( sqlite3_step( stmt ) == SQLITE_ROW );
    const auto count = sqlite3_column_int( stmt, 0 );
    REQUIRE( sqlite3_finalize( stmt ) == SQLITE_OK );
    return count;
}

auto all_road_overmap() -> std::string
{
    const auto tile_count = std::to_string( OMAPX * OMAPY );
    auto stream = std::ostringstream{};
    stream << "# version 29\n{\"layers\":[";
    for( auto z = 0; z < OVERMAP_LAYERS; z++ ) {
        if( z > 0 ) {
            stream << ',';
        }
        stream << "[[\"road\"," << tile_count << "]]";
    }
    stream << R"(],"region_id":"default","tracked_vehicles":[{"name":"Truck","x":0,"y":0}],"npcs":[{"name":"Bob","abs_pos":[0,0,0]},{"name":"Alice","abs_pos":[1,0,0]}]})";
    stream << "\n";
    return stream.str();
}

auto all_true_visibility() -> std::string
{
    const auto tile_count = OMAPX * OMAPY;
    auto stream = std::ostringstream{};
    const auto write_bool_layers = [&]( const std::string & name, const bool value ) {
        stream << "\"" << name << "\":[";
        for( auto z = 0; z < OVERMAP_LAYERS; z++ ) {
            if( z > 0 ) {
                stream << ',';
            }
            stream << "[[" << ( value ? "true" : "false" ) << ',' << std::to_string( tile_count ) << "]]";
        }
        stream << ']';
    };
    const auto write_empty_layers = [&]( const std::string & name ) {
        stream << "\"" << name << "\":[";
        for( auto z = 0; z < OVERMAP_LAYERS; z++ ) {
            if( z > 0 ) {
                stream << ',';
            }
            stream << "[]";
        }
        stream << ']';
    };
    stream << "# version 29\n{";
    write_bool_layers( "visible", true );
    stream << ',';
    write_bool_layers( "explored", true );
    stream << ',';
    write_bool_layers( "path", false );
    stream << ',';
    write_empty_layers( "notes" );
    stream << ',';
    write_empty_layers( "extras" );
    stream << "}\n";
    return stream.str();
}


auto file_data( sqlite3 *db, const std::string &path ) -> std::string
{
    auto *stmt = static_cast<sqlite3_stmt *>( nullptr );
    REQUIRE( sqlite3_prepare_v2( db, "SELECT data FROM files WHERE path = :path", -1, &stmt,
                                 nullptr ) == SQLITE_OK );
    REQUIRE( sqlite3_bind_text( stmt, sqlite3_bind_parameter_index( stmt, ":path" ), path.c_str(),
                                -1, SQLITE_TRANSIENT ) == SQLITE_OK );
    REQUIRE( sqlite3_step( stmt ) == SQLITE_ROW );
    const auto *blob = sqlite3_column_blob( stmt, 0 );
    const auto blob_size = sqlite3_column_bytes( stmt, 0 );
    auto result = std::string( static_cast<const char *>( blob ),
                               static_cast<std::size_t>( blob_size ) );
    REQUIRE( sqlite3_finalize( stmt ) == SQLITE_OK );
    return result;
}

auto make_world_with_sqlite() -> temp_dir
{
    auto dir = temp_dir{};
    const auto world_path = dir.path() / "World";
    std::filesystem::create_directories( world_path );
    auto *db = open_test_db( world_path / "map.sqlite3" );
    exec_sql( db, "CREATE TABLE files(path TEXT PRIMARY KEY NOT NULL, parent TEXT NOT NULL, "
              "compression TEXT DEFAULT NULL, data BLOB NOT NULL)" );
    insert_file( db, "maps/0.0.0/0.0.0.map", R"([{"vehicles":[{"name":"Car"}]}])" );
    insert_file( db, "o.0.0", all_road_overmap() );
    insert_file( db, ".seen.0.0", all_true_visibility() );
    insert_file( db, "maps/1.0.0/180.0.0.map", R"([{"vehicles":[{"name":"Far car"}]}])" );
    insert_file( db, "unrelated.json", "{}" );
    REQUIRE( sqlite3_close( db ) == SQLITE_OK );
    auto master = std::ofstream( world_path / "master.gsav" );
    master << "# version 29\n{\"seed\": 12345}\n";
    return dir;
}

} // namespace

TEST_CASE( "save_tools can compress and decompress sqlite file blobs", "[save_tools]" )
{
    auto dir = make_world_with_sqlite();
    const auto world_path = dir.path() / "World";

    const auto info_before = save_tools::rewrite_world_blobs( world_path,
                             save_tools::blob_compression_mode::info );
    CHECK( info_before.databases == 1 );
    CHECK( info_before.rows == 5 );
    CHECK( info_before.uncompressed_rows == 5 );

    const auto compressed = save_tools::rewrite_world_blobs( world_path,
                            save_tools::blob_compression_mode::compress );
    CHECK( compressed.changed_rows == 5 );

    auto *db = open_test_db( world_path / "map.sqlite3" );
    CHECK( row_count( db, "compression = 'zlib'" ) == 5 );
    REQUIRE( sqlite3_close( db ) == SQLITE_OK );

    const auto decompressed = save_tools::rewrite_world_blobs( world_path,
                              save_tools::blob_compression_mode::decompress );
    CHECK( decompressed.changed_rows == 5 );

    db = open_test_db( world_path / "map.sqlite3" );
    CHECK( row_count( db, "compression IS NULL" ) == 5 );
    REQUIRE( sqlite3_close( db ) == SQLITE_OK );
}

TEST_CASE( "save_tools previews and prunes map rows outside overmap bubble", "[save_tools]" )
{
    auto dir = make_world_with_sqlite();
    const auto world_path = dir.path() / "World";
    const auto bubble = save_tools::save_prune_bubble{ .center = point_abs_omt( 0, 0 ), .radius = 1 };

    const auto preview = save_tools::preview_prune_world_outside_bubble( world_path, bubble );
    CHECK( preview.databases == 1 );
    CHECK( preview.kept_rows == 2 );
    CHECK( preview.pruned_rows == 2 );
    CHECK( preview.ignored_rows == 1 );
    CHECK_THAT( preview.vehicles, Catch::UnorderedEquals( std::vector<std::string> { "Car", "Truck" } ) );
    CHECK_THAT( preview.npcs, Catch::UnorderedEquals( std::vector<std::string> { "Alice", "Bob" } ) );

    const auto result = save_tools::prune_world_outside_bubble( world_path, bubble );
    CHECK( std::filesystem::exists( result.backup_path / "map.sqlite3" ) );
    CHECK( result.preview.pruned_rows == 2 );
    CHECK( result.old_seed == 12345 );
    CHECK( result.new_seed != 12345 );
    CHECK( read_file( world_path / "master.gsav" ).find( std::to_string( result.new_seed ) ) !=
           std::string::npos );
    CHECK( read_file( result.backup_path / "master.gsav" ).find( "12345" ) != std::string::npos );

    auto *db = open_test_db( world_path / "map.sqlite3" );
    CHECK( row_count( db, "path = 'maps/1.0.0/180.0.0.map'" ) == 0 );
    CHECK( row_count( db, "path = 'o.0.0'" ) == 0 );
    CHECK( row_count( db, "path = 'pruned_overmap/o.0.0'" ) == 1 );
    CHECK( file_data( db, "pruned_overmap/o.0.0" ).find( "Bob" ) != std::string::npos );
    CHECK( file_data( db, "pruned_overmap/o.0.0" ).find( "Alice" ) != std::string::npos );
    CHECK( file_data( db, "pruned_overmap/o.0.0" ).find( ",," ) == std::string::npos );
    CHECK( row_count( db, "path = 'maps/0.0.0/0.0.0.map'" ) == 1 );
    CHECK( row_count( db, "path = '.seen.0.0'" ) == 1 );
    CHECK( file_data( db, ".seen.0.0" ).find( "true," + std::to_string( OMAPX * OMAPY ) ) ==
           std::string::npos );
    CHECK( row_count( db, "path = 'unrelated.json'" ) == 1 );
    REQUIRE( sqlite3_close( db ) == SQLITE_OK );

    db = open_test_db( result.backup_path / "map.sqlite3" );
    CHECK( row_count( db, "path = 'maps/1.0.0/180.0.0.map'" ) == 1 );
    REQUIRE( sqlite3_close( db ) == SQLITE_OK );
}
