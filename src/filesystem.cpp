#include "filesystem.h"

// FILE I/O
#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "catacharset.h"
#include "debug.h"
#include "fstream_utils.h"
#include "path_utils.h"
#include "string_utils.h"

#if defined(_WIN32)
#   include "platform_win.h"
#   include <direct.h>
#else
#   include <unistd.h>
#endif

#define dbg(x) DebugLog((x), DC::Main)

namespace
{

auto path_text( const fs::path &path ) -> std::string
{
    return cata_files::path_to_generic_utf8( path );
}

auto name_contains( const fs::path &path, const std::string &match, const bool at_end ) -> bool
{
    const auto filename = path_text( path.filename() );
    if( match.size() > filename.size() ) {
        return false;
    }

    return at_end ? filename.ends_with( match ) : filename.find( match ) != std::string::npos;
}

template <typename Predicate>
auto find_file_if_bfs( const fs::path &root_path, const bool recursive_search,
                       Predicate predicate ) -> std::vector<fs::path>
{
    auto directories = std::deque<fs::path> { root_path.empty() ? fs::path( "." ) : root_path };
    auto results = std::vector<fs::path> {};
    if( !dir_exist( directories.front() ) ) {
        return results;
    }

    while( !directories.empty() ) {
        const auto path = std::move( directories.front() );
        directories.pop_front();

        const auto n_dirs = static_cast<std::ptrdiff_t>( directories.size() );
        const auto n_results = static_cast<std::ptrdiff_t>( results.size() );

        auto entries = std::vector<fs::directory_entry> {};
        auto ec = std::error_code{};
        for( const auto &entry : fs::directory_iterator( path,
                fs::directory_options::skip_permission_denied,
                ec ) ) {
            entries.emplace_back( entry );
        }
        if( ec ) {
            dbg( DL::Warn ) << "directory iteration [" << path_text( path ) << "] failed with \"" <<
                            ec.message() << "\".";
            continue;
        }

        std::ranges::sort( entries, {}, []( const fs::directory_entry & entry ) { return entry.path(); } );

        for( const auto &entry : entries ) {
            const auto full_path = entry.path();
            if( path_text( full_path.filename() ).ends_with( '~' ) ) {
                continue;
            }

            auto is_dir_ec = std::error_code{};
            const auto is_dir = entry.is_directory( is_dir_ec );
            if( is_dir_ec ) {
                dbg( DL::Warn ) << "stat [" << path_text( full_path ) << "] failed with \"" <<
                                is_dir_ec.message() << "\".";
                continue;
            }
            if( recursive_search && is_dir ) {
                directories.emplace_back( full_path );
            }

            if( predicate( full_path, is_dir ) ) {
                results.emplace_back( full_path );
            }
        }

        // Keep files and directories to recurse ordered consistently.
        // NOLINTNEXTLINE(cata-use-localized-sorting)
        std::sort( std::begin( directories ) + n_dirs, std::end( directories ) );
        // NOLINTNEXTLINE(cata-use-localized-sorting)
        std::sort( std::begin( results ) + n_results, std::end( results ) );
    }

    return results;
}

auto directory_matches( const fs::path &file ) -> fs::path
{
    return file.parent_path();
}

} // namespace

auto assure_dir_exist( const fs::path &path ) -> bool
{
    if( dir_exist( path ) ) {
        return true;
    }
    auto ec = std::error_code{};
    return fs::create_directory( path, ec ) && !ec;
}

auto dir_exist( const fs::path &path ) -> bool
{
    auto ec = std::error_code{};
    return fs::is_directory( path, ec ) && !ec;
}

auto file_exist( const fs::path &path ) -> bool
{
    auto ec = std::error_code{};
    return fs::is_regular_file( path, ec ) && !ec;
}

auto as_norm_dir( const fs::path &path ) -> fs::path
{
    if( path.empty() ) {
        return {};
    }
    const auto norm = ( path / fs::path{} ).lexically_normal();
    return norm == "." ? fs::path( "." ) : norm;
}

auto remove_file( const fs::path &path ) -> bool
{
    if( !file_exist( path ) ) {
        return false;
    }
    auto ec = std::error_code{};
    return fs::remove( path, ec ) && !ec;
}

auto rename_file( const fs::path &old_path, const fs::path &new_path ) -> bool
{
    if( !file_exist( old_path ) || dir_exist( new_path ) ) {
        return false;
    }
    auto ec = std::error_code{};
    if( file_exist( new_path ) ) {
        fs::remove( new_path, ec );
        if( ec ) {
            return false;
        }
    }
    fs::rename( old_path, new_path, ec );
    return !ec;
}

auto remove_directory( const fs::path &path ) -> bool
{
    if( !dir_exist( path ) ) {
        return false;
    }
    auto ec = std::error_code{};
    return fs::remove( path, ec ) && !ec;
}

auto remove_tree( const fs::path &path ) -> bool
{
    auto ec = std::error_code{};
    fs::remove_all( path, ec );
    if( ec ) {
        dbg( DL::Error ) << "remove_tree [" << path_text( path ) << "] failed with \"" << ec.message() <<
                         "\".";
        return false;
    }
    return true;
}

auto cata_files::eol() -> const char *
{
#if defined(_WIN32)
    // NOLINTNEXTLINE(cata-text-style): carriage return is necessary here
    static const char local_eol[] = "\r\n";
#else
    static const char local_eol[] = "\n";
#endif
    return local_eol;
}

auto read_entire_file( const fs::path &path ) -> std::string
{
    auto infile = cata_ifstream{};
    infile.mode( cata_ios_mode::binary ).open( path );
    if( !infile.is_open() ) {
        return "";
    }
    auto ret = std::string( std::istreambuf_iterator<char>( *infile ),
                            std::istreambuf_iterator<char>() );
    if( infile.fail() ) {
        return "";
    }
    return ret;
}

auto get_files_from_path( const fs::path &pattern, const fs::path &root_path,
                          const bool recursive_search, const bool match_extension ) -> std::vector<fs::path>
{
    const auto pattern_name = path_text( pattern );
    return find_file_if_bfs( root_path, recursive_search, [&]( const fs::path & path, bool ) {
        return name_contains( path, pattern_name, match_extension );
    } );
}

auto get_directories_with( const fs::path &pattern, const fs::path &root_path,
                           const bool recursive_search ) -> std::vector<fs::path>
{
    const auto pattern_name = path_text( pattern );
    if( pattern_name.empty() ) {
        return {};
    }

    auto files = find_file_if_bfs( root_path, recursive_search, [&]( const fs::path & path, bool ) {
        return name_contains( path, pattern_name, true );
    } );

    std::ranges::transform( files, files.begin(), directory_matches );
    files.erase( std::unique( std::begin( files ), std::end( files ) ), std::end( files ) );

    return files;
}

auto get_directories_with( const std::vector<fs::path> &patterns, const fs::path &root_path,
                           const bool recursive_search ) -> std::vector<fs::path>
{
    if( patterns.empty() ) {
        return {};
    }

    auto files = find_file_if_bfs( root_path, recursive_search, [&]( const fs::path & path, bool ) {
        return std::ranges::any_of( patterns, [&]( const fs::path & ext ) {
            return name_contains( path, path_text( ext ), true );
        } );
    } );

    std::ranges::transform( files, files.begin(), directory_matches );
    files.erase( std::unique( std::begin( files ), std::end( files ) ), std::end( files ) );

    return files;
}

auto copy_file( const fs::path &source_path, const fs::path &dest_path ) -> bool
{
    if( !file_exist( source_path ) || dir_exist( dest_path ) ) {
        return false;
    }
    auto ec = std::error_code{};
    fs::copy_file( source_path, dest_path, fs::copy_options::overwrite_existing, ec );
    return !ec;
}

auto ensure_valid_file_name( const std::string &file_name ) -> std::string
{
    const auto replacement_char = ' ';
    const auto invalid_chars = std::string( "\\/:?\"<>|" );

    // do any replacement in the file name, if needed.
    auto new_file_name = file_name;
    std::ranges::transform( new_file_name, new_file_name.begin(), [&]( const char c ) {
        return invalid_chars.find( c ) != std::string::npos ? replacement_char : c;
    } );

    return new_file_name;
}

// This string is 'CataclysmBrightNights' encoded as base64
const auto *CBN = "Q2F0YWNseXNtQnJpZ2h0TmlnaHRz";

auto can_write_to_dir( const fs::path &dir_path ) -> bool
{
    const auto dummy_file = dir_path / CBN;

    if( file_exist( dummy_file ) && !remove_file( dummy_file ) ) {
        return false;
    }

    const auto writer = []( std::ostream & s ) { s << CBN << '\n'; };

    if( !write_to_file( dummy_file, writer, "" ) ) {
        return false;
    }

    return remove_file( dummy_file );
}

auto get_pid_string() -> std::string
{
#if defined _WIN32
    return std::to_string( GetCurrentProcessId() );
#else
    return std::to_string( getpid() );
#endif
}
