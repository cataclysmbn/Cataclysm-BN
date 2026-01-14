#pragma once

#include <functional>
#include <string>
#include "json.h"
#include "options.h"
#include "type_id.h"
#include "coordinates.h"
#include "fstream_utils.h"

class avatar;
class sqlite3;

class save_t
{
    private:
        std::string name;

        save_t( const std::string &name );

    public:
        std::string decoded_name() const;
        std::string base_path() const;

        static save_t from_save_id( const std::string &save_id );
        static save_t from_base_path( const std::string &base_path );

        bool operator==( const save_t &rhs ) const {
            return name == rhs.name;
        }
        bool operator!=( const save_t &rhs ) const {
            return !operator==( rhs );
        }
        save_t( const save_t & ) = default;
        save_t &operator=( const save_t & ) = default;
};

enum save_format : int {
    /** Legacy save layout; uncompressed JSON as loose files */
    V1 = 0,

    /** Current save format - compressed tuples in SQLite3 */
    V2_COMPRESSED_SQLITE3 = 1,
};

/**
 * Structure containing metadata about a world. No actual world data is processed here.
 *
 * The actual instances are owned by the worldfactory class. All other classes should
 * only have a pointer to one of these owned instances.
 */
struct WORLDINFO {
    public:
        /**
         * @returns A path to a folder in the file system that should contain
         * all the world specific files. It depends on @ref world_name,
         * changing that will also change the result of this function.
         */
        std::string folder_path() const;

        std::string world_name;
        options_manager::options_container WORLD_OPTIONS;
        std::vector<save_t> world_saves;
        save_format world_save_format;

        /**
         * A (possibly empty) list of (idents of) mods that
         * should be loaded for this world.
         */
        std::vector<mod_id> active_mod_order;

        WORLDINFO();
        void COPY_WORLD( const WORLDINFO *world_to_copy );

        bool save_exists( const save_t &name ) const;
        void add_save( const save_t &name );

        bool save( bool is_conversion = false ) const;

        void load_options( JsonIn &jsin );
        bool load_options();
        void load_legacy_options( std::istream &fin );
};

class world
{
    public:
        world( WORLDINFO *info );
        ~world();

        WORLDINFO *info;

        /**
         * Right now, each file write is independent of the others. Once we start
         * migrating to SQLite, each save would happen in a single transaction. This
         * gives two benefits: (1) atomicity, and (2) performance.
         *
         * When using the V1 non-sqlite save system, this merely records some metadata
         * so we can print how long the save took.
         */
        /**@{*/
        void start_save_tx();
        int64_t commit_save_tx();
        /**@}*/

        /*
         * Targeted/domain-specific file operations. Different save formats may choose to
         * lay out files differently, so centralize file placement logic here rather than
         * scattering it throughout the codebase.
         */
        bool read_map_quad( const tripoint &om_addr, file_read_json_fn reader ) const;
        bool write_map_quad( const tripoint &om_addr, file_write_fn writer ) const;

        bool overmap_exists( const point_abs_om &p ) const;
        bool read_overmap( const point_abs_om &p, file_read_fn reader ) const;
        bool read_overmap_player_visibility( const point_abs_om &p, file_read_fn reader );
        bool write_overmap( const point_abs_om &p, file_write_fn writer ) const;
        bool write_overmap_player_visibility( const point_abs_om &p, file_write_fn writer );

        bool read_player_mm_quad( const tripoint &p, file_read_json_fn reader );
        bool write_player_mm_quad( const tripoint &p, file_write_fn writer );

        /*
         * Player-specific file operations. Paths will be prefixed with the player's save ID.
         */
        bool player_file_exist( const std::string &path );
        bool write_to_player_file( const std::string &path, file_write_fn writer,
                                   const char *fail_message = nullptr );
        bool read_from_player_file( const std::string &path, file_read_fn reader,
                                    bool optional = true );
        bool read_from_player_file_json( const std::string &path, file_read_json_fn reader,
                                         bool optional = true );

        /*
         * Generic file operations, acting as a catch-all for miscellaneous save files
         * living in the root of the world directory.
         */
        bool assure_dir_exist( const std::string &path ) const;
        bool file_exist( const std::string &path ) const;

        /**
         * If fail_message is provided, this method will eat any exceptions and display a popup with the
         * exception detail and the message. If fail_message is not provided, the exception will be
         * propagated.
         *
         * To eat any exceptions and not display a popup, pass the empty string as fail_message.
         *
         * @param path The path to write to.
         * @param writer The function that writes to the file.
         * @param fail_message The message to display if the write fails.
         * @return True if the write was successful, false otherwise.
         */
        bool write_to_file( const std::string &path, file_write_fn writer,
                            const char *fail_message = nullptr ) const;
        bool read_from_file( const std::string &path, file_read_fn reader,
                             bool optional = true ) const;
        bool read_from_file_json( const std::string &path, file_read_json_fn reader,
                                  bool optional = true ) const;

        /**
         * Convert (copy) the save data from the old format to the new format.
         * This should only be called from `worldfactory`.
         */
        void convert_from_v1( const std::unique_ptr<WORLDINFO> &old_world );

    private:
        /** If non-zero, indicates we're in the middle of a save event */
        int64_t save_tx_start_ts = 0;

        std::string overmap_terrain_filename( const point_abs_om &p ) const;
        std::string overmap_player_filename( const point_abs_om &p ) const;
        std::string get_player_path() const;

        sqlite3 *map_db = nullptr;

        sqlite3 *save_db = nullptr;
        std::string last_save_id = "";
        sqlite3 *get_player_db();
};


