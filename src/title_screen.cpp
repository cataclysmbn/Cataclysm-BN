#include "title_screen.h"

#include <algorithm>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "filesystem.h"
#include "language.h"
#include "mod_manager.h"
#include "path_info.h"
#include "rng.h"
#include "string_formatter.h"
#include "translations.h"

namespace
{

constexpr auto title_extension = ".title";
constexpr auto builtin_option_prefix = "builtin:";
constexpr auto mod_option_prefix = "mod:";
constexpr auto directory_key_prefix = "dir:";
constexpr auto file_key_prefix = "file:";

struct title_file {
    std::string language;
    std::string path;
};

struct title_candidate {
    std::string option;
    std::string name;
    std::vector<title_file> files;
};

auto normalize_path( const std::string &path ) -> std::string
{
    return std::filesystem::path( path ).lexically_normal().generic_string();
}

auto parent_path( const std::string &path ) -> std::string
{
    return std::filesystem::path( path ).parent_path().lexically_normal().generic_string();
}

auto get_language_priority() -> std::vector<std::string>
{
    auto result = std::vector<std::string> {};
    for( const auto &language : get_lang_path_substring( get_language().id ) ) {
        if( !language.empty() && !std::ranges::contains( result, language ) ) {
            result.emplace_back( language );
        }
    }
    if( !std::ranges::contains( result, "en" ) ) {
        result.emplace_back( "en" );
    }
    return result;
}

auto get_all_languages() -> std::vector<std::string>
{
    auto result = std::vector<std::string> {};
    for( const auto &info : list_available_languages() ) {
        for( const auto &language : get_lang_path_substring( info.id ) ) {
            if( !language.empty() && !std::ranges::contains( result, language ) ) {
                result.emplace_back( language );
            }
        }
    }
    if( !std::ranges::contains( result, "en" ) ) {
        result.emplace_back( "en" );
    }
    return result;
}

auto humanize_title_name( std::string name ) -> std::string
{
    std::ranges::replace( name, '_', ' ' );
    std::ranges::replace( name, '-', ' ' );
    return name;
}

auto mod_option_id( const mod_id &ident, const std::string &key ) -> std::string
{
    return std::string( mod_option_prefix ) + ident.str() + ":" + key;
}

auto directory_title_name( const std::string &directory, const std::string &root,
                           const std::optional<std::string> &mod_name ) -> std::string
{
    const auto normalized_directory = normalize_path( directory );
    const auto normalized_root = normalize_path( root );
    const auto directory_name = std::filesystem::path(
                                    normalized_directory ).filename().generic_string();
    if( mod_name ) {
        if( normalized_directory == normalized_root || directory_name == "title" ) {
            return *mod_name;
        }
        return string_format( "%s: %s", *mod_name, humanize_title_name( directory_name ) );
    }
    return humanize_title_name( directory_name );
}

auto file_title_name( const std::string &title_name,
                      const std::optional<std::string> &mod_name ) -> std::string
{
    const auto display_name = humanize_title_name( title_name );
    if( mod_name ) {
        return string_format( "%s: %s", *mod_name, display_name );
    }
    return display_name;
}

auto title_name_from_suffix_format( const std::string &stem,
                                    const std::string &language ) -> std::optional<std::string>
{
    const auto suffix = "_" + language;
    if( stem.size() <= suffix.size() || !stem.ends_with( suffix ) ) {
        return std::nullopt;
    }
    return stem.substr( 0, stem.size() - suffix.size() );
}

struct candidate_file_options {
    std::string option;
    std::string name;
    std::string language;
    std::string path;
};

auto add_candidate_file( std::map<std::string, title_candidate> &candidates,
                         const candidate_file_options &opts ) -> void
{
    auto &candidate = candidates[opts.option];
    if( candidate.option.empty() ) {
        candidate.option = opts.option;
        candidate.name = opts.name;
    }
    if( std::ranges::none_of( candidate.files, [&]( const title_file & file ) {
    return file.language == opts.language && file.path == opts.path;
} ) ) {
        candidate.files.emplace_back( opts.language, opts.path );
    }
}

struct title_scan_options {
    std::string option_prefix;
    std::string root;
    const std::vector<std::string> &languages;
    std::optional<mod_id> mod;
    std::optional<std::string> mod_name;
    bool skip_root_language_titles;
};

auto add_title_file( std::map<std::string, title_candidate> &candidates,
                     const title_scan_options &opts, const std::string &path ) -> void
{
    const auto normalized_path = normalize_path( path );
    const auto directory = parent_path( normalized_path );
    const auto stem = std::filesystem::path( normalized_path ).stem().generic_string();
    const auto normalized_root = normalize_path( opts.root );

    for( const auto &language : opts.languages ) {
        if( stem == language ) {
            if( opts.skip_root_language_titles && normalize_path( directory ) == normalized_root ) {
                return;
            }

            const auto key = std::string( directory_key_prefix ) + directory;
            const auto option = opts.mod ? mod_option_id( *opts.mod, key ) : opts.option_prefix + key;
            add_candidate_file( candidates, candidate_file_options{
                .option = option,
                .name = directory_title_name( directory, normalized_root, opts.mod_name ),
                .language = language,
                .path = normalized_path
            } );
            return;
        }

        const auto title_name = title_name_from_suffix_format( stem, language );
        if( title_name ) {
            const auto key = std::string( file_key_prefix ) + directory + "/" + *title_name;
            const auto option = opts.mod ? mod_option_id( *opts.mod, key ) : opts.option_prefix + key;
            add_candidate_file( candidates, candidate_file_options{
                .option = option,
                .name = file_title_name( *title_name, opts.mod_name ),
                .language = language,
                .path = normalized_path
            } );
            return;
        }
    }
}

auto get_title_files_from_root( const std::string &root ) -> std::vector<std::string>
{
    return get_files_from_path( title_extension, root, true, true );
}

auto add_title_files_from_root( std::map<std::string, title_candidate> &candidates,
                                const title_scan_options &opts ) -> void
{
    for( const auto &path : get_title_files_from_root( opts.root ) ) {
        add_title_file( candidates, opts, path );
    }
}

auto get_mod_search_roots( const MOD_INFORMATION &mod ) -> std::vector<std::string>
{
    auto result = std::vector<std::string> {};
    if( !mod.path.empty() ) {
        result.emplace_back( normalize_path( mod.path ) );
    }
    if( !mod.path_full.empty() ) {
        result.emplace_back( parent_path( mod.path_full ) );
    }

    std::ranges::sort( result );
    result.erase( std::ranges::unique( result ).begin(), result.end() );
    return result;
}

auto get_installed_mods() -> std::map<mod_id, MOD_INFORMATION>
{
    auto result = std::map<mod_id, MOD_INFORMATION> {};
    for( auto &mod : mod_management::load_mods_from( PATH_INFO::moddir() ) ) {
        result[mod.ident] = std::move( mod );
    }
    for( auto &mod : mod_management::load_mods_from( PATH_INFO::user_moddir() ) ) {
        result[mod.ident] = std::move( mod );
    }
    return result;
}

auto title_path_for_candidate( const title_candidate &candidate,
                               const std::vector<std::string> &languages ) -> std::optional<std::string>
{
    for( const auto &language : languages ) {
        const auto iter = std::ranges::find_if( candidate.files, [&]( const title_file & file ) {
            return file.language == language;
        } );
        if( iter != candidate.files.end() ) {
            return iter->path;
        }
    }
    return std::nullopt;
}

auto get_title_candidates( const std::vector<std::string> &languages ) ->
std::vector<title_candidate>
{
    auto candidates = std::map<std::string, title_candidate> {};

    add_title_files_from_root( candidates, title_scan_options{
        .option_prefix = builtin_option_prefix,
        .root = PATH_INFO::datadir() + "title/",
        .languages = languages,
        .skip_root_language_titles = true
    } );

    for( auto &[ident, mod] : get_installed_mods() ) {
        auto mod_name = mod.name();
        if( mod_name.empty() ) {
            mod_name = ident.str();
        }
        for( const auto &root : get_mod_search_roots( mod ) ) {
            add_title_files_from_root( candidates, title_scan_options{
                .root = root,
                .languages = languages,
                .mod = ident,
                .mod_name = mod_name,
                .skip_root_language_titles = false
            } );
        }
    }

    auto result = std::vector<title_candidate> {};
    for( auto &[option, candidate] : candidates ) {
        if( title_path_for_candidate( candidate, languages ) ) {
            result.push_back( std::move( candidate ) );
        }
    }
    std::ranges::sort( result, []( const title_candidate & lhs, const title_candidate & rhs ) {
        return localized_compare( std::make_pair( lhs.name, lhs.option ),
                                  std::make_pair( rhs.name, rhs.option ) );
    } );
    return result;
}

auto get_base_title_path( const std::vector<std::string> &languages ) -> std::string
{
    const auto title_dir = PATH_INFO::datadir() + "title/";
    for( const auto &language : languages ) {
        const auto path = title_dir + language + title_extension;
        if( file_exist( path ) ) {
            return path;
        }
    }
    return title_dir + "en.title";
}

auto reset_title_option_to_default() -> void
{
    auto &options = ::get_options();
    if( !options.has_option( title_screen::option_id ) ) {
        return;
    }

    auto &option = options.get_option( title_screen::option_id );
    if( option.getValue() == title_screen::default_option_id ) {
        return;
    }

    option.setValue( title_screen::default_option_id );
    options.save();
}

} // namespace

namespace title_screen
{

auto options_from_candidates( const std::vector<title_candidate> &candidates ) ->
std::vector<options_manager::id_and_option>
{
    auto result = std::vector<options_manager::id_and_option> {
        { default_option_id, to_translation( "Default" ) },
        { random_option_id, to_translation( "Random" ) },
    };

    for( const auto &candidate : candidates ) {
        result.emplace_back( candidate.option, no_translation( candidate.name ) );
    }

    return result;
}

auto get_options() -> std::vector<options_manager::id_and_option>
{
    return options_from_candidates( get_title_candidates( get_language_priority() ) );
}

auto get_all_options() -> std::vector<options_manager::id_and_option>
{
    return options_from_candidates( get_title_candidates( get_all_languages() ) );
}

auto resolve_path() -> std::string
{
    const auto languages = get_language_priority();
    const auto base_title = get_base_title_path( languages );
    auto &options = ::get_options();
    if( !options.has_option( option_id ) ) {
        return base_title;
    }

    const auto selected = options.get_option( option_id ).getValue();
    if( selected == default_option_id ) {
        return base_title;
    }

    const auto candidates = get_title_candidates( languages );
    if( selected == random_option_id ) {
        auto random_titles = std::vector<std::string> { base_title };
        for( const auto &entry : candidates ) {
            if( const auto path = title_path_for_candidate( entry, languages ) ) {
                random_titles.push_back( *path );
            }
        }
        return random_entry( random_titles, base_title );
    }

    const auto candidate = std::ranges::find_if( candidates, [&selected]( const title_candidate &
    entry ) {
        return entry.option == selected;
    } );
    if( candidate == candidates.end() ) {
        reset_title_option_to_default();
        return base_title;
    }

    auto title_path = title_path_for_candidate( *candidate, languages );
    if( !title_path ) {
        reset_title_option_to_default();
        return base_title;
    }

    return *title_path;
}

} // namespace title_screen
