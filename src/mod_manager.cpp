#include "mod_manager.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <ostream>
#include <queue>
#include <utility>

#include "assign.h"
#include "cata_utility.h"
#include "debug.h"
#include "dependency_tree.h"
#include "filesystem.h"
#include "fstream_utils.h"
#include "json.h"
#include "path_info.h"
#include "string_formatter.h"
#include "string_id.h"
#include "translations.h"
#include "worldfactory.h"

static const std::string MOD_SEARCH_FILE( "modinfo.json" );

template<>
const MOD_INFORMATION &string_id<MOD_INFORMATION>::obj() const
{
    const auto &map = world_generator->get_mod_manager().mod_map;
    const auto iter = map.find( *this );
    if( iter == map.end() ) {
        debugmsg( "Invalid mod %s requested", str() );
        static const MOD_INFORMATION dummy{};
        return dummy;
    }
    return iter->second;
}

template<>
bool string_id<MOD_INFORMATION>::is_valid() const
{
    return world_generator->get_mod_manager().mod_map.contains( *this );
}

auto MOD_INFORMATION::name() const -> std::string
{
    if( translatable_info.name().empty() ) {
        // "No name" gets confusing if many mods have no name
        //~ name of a mod that has no name entry, (%s is the mods identifier)
        return string_format( _( "No name (%s)" ), ident.c_str() );
    } else {
        return translatable_info.name();
    }
}

auto MOD_INFORMATION::name_raw() const -> std::string
{
    if( translatable_info.name_raw().empty() ) {
        return string_format( "No name (%s)", ident.c_str() );
    } else {
        return translatable_info.name_raw();
    }
}

auto MOD_INFORMATION::description() const -> std::string
{
    return translatable_info.description();
}

const std::vector<std::pair<std::string, std::string> > &get_mod_list_categories()
{
    static const std::vector<std::pair<std::string, std::string> > mod_list_categories = {
        {"core", translate_marker( "CORE GAME DATA" )},
        {"content", translate_marker( "CONTENT PACKS" )},
        {"items", translate_marker( "ITEM ADDITION MODS" )},
        {"creatures", translate_marker( "CREATURE MODS" )},
        {"misc_additions", translate_marker( "MISC ADDITIONS" )},
        {"buildings", translate_marker( "BUILDINGS MODS" )},
        {"vehicles", translate_marker( "VEHICLE MODS" )},
        {"rebalance", translate_marker( "REBALANCING MODS" )},
        {"magical", translate_marker( "MAGICAL MODS" )},
        {"item_exclude", translate_marker( "ITEM EXCLUSION MODS" )},
        {"monster_exclude", translate_marker( "MONSTER EXCLUSION MODS" )},
        {"graphical", translate_marker( "GRAPHICAL MODS" )},
        {"", translate_marker( "NO CATEGORY" )}
    };

    return mod_list_categories;
}

const std::vector<std::pair<std::string, std::string> > &get_mod_list_tabs()
{
    static const std::vector<std::pair<std::string, std::string> > mod_list_tabs = {
        {"tab_default", translate_marker( "Default" )},
        {"tab_blacklist", translate_marker( "Blacklist" )},
        {"tab_balance", translate_marker( "Balance" )}
    };

    return mod_list_tabs;
}

const std::map<std::string, std::string> &get_mod_list_cat_tab()
{
    static const std::map<std::string, std::string> mod_list_cat_tab = {
        {"item_exclude", "tab_blacklist"},
        {"monster_exclude", "tab_blacklist"},
        {"rebalance", "tab_balance"}
    };

    return mod_list_cat_tab;
}

void mod_manager::load_replacement_mods( const std::string &path )
{
    read_from_file_json( path, [&]( JsonIn & jsin ) {
        jsin.start_array();
        while( !jsin.end_array() ) {
            auto arr = jsin.get_array();
            mod_replacements.emplace( mod_id( arr.get_string( 0 ) ),
                                      mod_id( arr.size() > 1 ? arr.get_string( 1 ) : "" ) );
        }
    }, true );
}

mod_manager::mod_manager()
{
    load_replacement_mods( PATH_INFO::mods_replacements() );
    refresh_mod_list();
}

mod_manager::~mod_manager() = default;

std::vector<mod_id> mod_manager::all_mods() const
{
    std::vector<mod_id> result;
    std::transform( mod_map.begin(), mod_map.end(),
    std::back_inserter( result ), []( const decltype( mod_manager::mod_map )::value_type & pair ) {
        return pair.first;
    } );
    return result;
}

dependency_tree &mod_manager::get_tree()
{
    return *tree;
}

void mod_manager::clear()
{
    tree->clear();
    mod_map.clear();
    default_mods.clear();
}

void mod_manager::refresh_mod_list()
{
    clear();

    add_mods( mod_management::load_mods_from( PATH_INFO::moddir() ) );
    add_mods( mod_management::load_mods_from( PATH_INFO::user_moddir() ) );

    std::optional<t_mod_list> default_list = mod_management::load_mod_list(
                PATH_INFO::mods_user_default()
            );
    if( !default_list ) {
        default_list = mod_management::load_mod_list( PATH_INFO::mods_dev_default() );
    }
    if( default_list ) {
        t_mod_list list = std::move( *default_list );
        remove_invalid_mods( list );
        default_mods = list;
    }

    std::map<mod_id, std::vector<mod_id>> mod_dependency_map;
    std::map<mod_id, std::vector<mod_id>> mod_conflict_map;
    for( const auto &elem : mod_map ) {
        const t_mod_list &deps = elem.second.dependencies;
        mod_dependency_map[elem.second.ident] = std::vector<mod_id>( deps.begin(), deps.end() );
        const t_mod_list &confs = elem.second.conflicts;
        mod_conflict_map[elem.second.ident] = std::vector<mod_id>( confs.begin(), confs.end() );
    }
    tree->init( mod_dependency_map, mod_conflict_map );
}

void mod_manager::remove_mod( const mod_id &ident )
{
    const auto a = mod_map.find( ident );
    if( a != mod_map.end() ) {
        mod_map.erase( a );
    }
}

void mod_manager::remove_invalid_mods( t_mod_list &mods ) const
{
    mods.erase( std::remove_if( mods.begin(), mods.end(), [this]( const mod_id & mod ) {
        return !mod_map.contains( mod );
    } ), mods.end() );
}

namespace mod_management
{

std::vector<MOD_INFORMATION> load_mods_from( const std::string &path )
{
    std::vector<MOD_INFORMATION> out;

    for( auto &mod_file : get_files_from_path( MOD_SEARCH_FILE, path, true ) ) {
        load_mod_info( mod_file, out );
    }

    std::set<mod_id> idents;
    std::set<mod_id> has_dupes;

    for( const MOD_INFORMATION &it : out ) {
        if( idents.contains( it.ident ) ) {
            has_dupes.emplace( it.ident );
        } else {
            idents.emplace( it.ident );
        }
    }

    for( const mod_id &ident : has_dupes ) {
        std::string msg = string_format(
                              _( "The are multiple mods with same id [%s] found in folder \"%s\":\n" ),
                              ident, path );

        for( auto it = out.begin(); it != out.end(); ) {
            if( it->ident == ident ) {
                msg += "  - ";
                msg += it->path_full;
                msg += "\n";
                it = out.erase( it );
            } else {
                it++;
            }
        }

        msg += _( "None of them will be loaded to avoid data corruption." );

        debugmsg( msg );
    }

    return out;
}

std::optional<MOD_INFORMATION> load_modfile( const JsonObject &jo, const std::string &path )
{
    if( !jo.has_string( "type" ) || jo.get_string( "type" ) != "MOD_INFO" ) {
        // Ignore anything that is not a mod-info
        jo.allow_omitted_members();
        return std::nullopt;
    }

    // TEMPORARY until 0.G: Remove "ident" support
    const mod_id m_ident( jo.has_string( "ident" ) ? jo.get_string( "ident" ) : jo.get_string( "id" ) );

    std::string m_cat = jo.get_string( "category", "" );
    std::pair<int, std::string> p_cat = { -1, "" };
    bool bCatFound = false;

    do {
        for( size_t i = 0; i < get_mod_list_categories().size(); ++i ) {
            if( get_mod_list_categories()[i].first == m_cat ) {
                p_cat = { static_cast<int>( i ), get_mod_list_categories()[i].second };
                bCatFound = true;
                break;
            }
        }

        if( !bCatFound && !m_cat.empty() ) {
            m_cat.clear();
        } else {
            break;
        }
    } while( !bCatFound );

    MOD_INFORMATION modfile;
    modfile.ident = m_ident;
    modfile.category = p_cat;

    if( assign( jo, "path", modfile.path ) ) {
        modfile.path = path + "/" + modfile.path;
    } else {
        modfile.path = path;
    }

    const std::string m_name = jo.get_string( "name", "" );
    const std::string m_descr = jo.get_string( "description", "" );
    modfile.set_translatable_info( translatable_mod_info( m_name, m_descr, modfile.path ) );

    assign( jo, "license", modfile.license );
    assign( jo, "authors", modfile.authors );
    assign( jo, "maintainers", modfile.maintainers );
    assign( jo, "version", modfile.version );
    assign( jo, "lua_api_version", modfile.lua_api_version );
    assign( jo, "dependencies", modfile.dependencies );
    assign( jo, "conflicts", modfile.conflicts );
    assign( jo, "core", modfile.core );
    assign( jo, "obsolete", modfile.obsolete );

    if( std::find( modfile.dependencies.begin(), modfile.dependencies.end(),
                   modfile.ident ) != modfile.dependencies.end() ) {
        jo.throw_error( "mod specifies self as a dependency", "dependencies" );
    }
    for( const auto &conf : modfile.conflicts ) {
        if( conf == modfile.ident ) {
            jo.throw_error( "mod specifies self as a conflict", "conflicts" );
        }
        if( std::find( modfile.dependencies.begin(), modfile.dependencies.end(),
                       conf ) != modfile.dependencies.end() ) {
            jo.throw_error( string_format( "mod specifies \"%s\" as both a dependency and a conflict", conf ),
                            "conflicts" );
        }
    }

    return { std::move( modfile ) };
}

void load_mod_info( const std::string &info_file_path, std::vector<MOD_INFORMATION> &out )
{
    const std::string main_path = info_file_path.substr( 0, info_file_path.find_last_of( "/\\" ) );
    read_from_file_json( info_file_path, [&]( JsonIn & jsin ) {
        if( jsin.test_object() ) {
            // find type and dispatch single object
            JsonObject jo = jsin.get_object();
            std::optional<MOD_INFORMATION> mf = load_modfile( jo, main_path );
            if( mf ) {
                mf->path_full = info_file_path;
                out.push_back( std::move( *mf ) );
            }
            jo.finish();
        } else if( jsin.test_array() ) {
            jsin.start_array();
            // find type and dispatch each object until array close
            while( !jsin.end_array() ) {
                JsonObject jo = jsin.get_object();
                std::optional<MOD_INFORMATION> mf = load_modfile( jo, main_path );
                if( mf ) {
                    mf->path_full = info_file_path;
                    out.push_back( std::move( *mf ) );
                }
                jo.finish();
            }
        } else {
            // not an object or an array?
            jsin.error( "expected array or object" );
        }
    }, true );
}

bool save_mod_list( const t_mod_list &list, const std::string &path )
{
    return write_to_file( path, [&]( std::ostream & fout ) {
        JsonOut json( fout, true ); // pretty-print
        json.write( list );
    }, _( "list of default mods" ) );
}

std::optional<t_mod_list> load_mod_list( const std::string &path )
{
    t_mod_list res;

    auto reader = [&]( JsonIn & jsin ) {
        jsin.read( res, true );
    };

    if( read_from_file_json( path, reader, true ) ) {
        return { std::move( res ) };
    } else {
        return std::nullopt;
    }
}

mod_id get_default_core_content_pack()
{
    return mod_id( "bn" );
}

} // namespace mod_management

bool is_strict_enabled( const std::string &src )
{
    return src == mod_management::get_default_core_content_pack().str();
}

void mod_manager::add_mods( std::vector<MOD_INFORMATION> &&list )
{
    struct replacement {
        mod_id id;
        std::string path_old;
        std::string path_new;
    };

    std::vector<replacement> replaced;

    for( MOD_INFORMATION &info : list ) {
        auto it = mod_map.find( info.ident );
        if( it == mod_map.end() ) {
            mod_map.emplace( info.ident, std::move( info ) );
        } else {
            replacement r;
            r.id = info.ident;
            r.path_old = it->second.path_full;
            r.path_new = info.path_full;
            replaced.push_back( std::move( r ) );
            it->second = std::move( info );
        }
    }

    if( !replaced.empty() ) {
        DebugLog( DL::Info, DC::Main ) << "[Mod manager] Replaced " << replaced.size() <<
                                       " mod(s) with user overrides.";
        std::string msg;
        for( const replacement &it : replaced ) {
            msg += string_format( "[%s] from \"%s\" with \"%s\"\n", it.id, it.path_old, it.path_new );
        }
        DebugLog( DL::Debug, DC::Main ) << msg;
    }
}

bool mod_manager::set_default_mods( const t_mod_list &mods )
{
    default_mods = mods;
    return mod_management::save_mod_list( mods, PATH_INFO::mods_user_default() );
}

std::string mod_manager::get_mods_list_file( WORLDINFO *world )
{
    return world->folder_path() + "/mods.json";
}

void mod_manager::save_mods_list( WORLDINFO *world ) const
{
    if( world == nullptr ) {
        return;
    }
    const std::string path = get_mods_list_file( world );
    if( world->active_mod_order.empty() ) {
        // If we were called from load_mods_list to prune the list,
        // and it's empty now, delete the file.
        remove_file( path );
        return;
    }
    write_to_file( path, [&]( std::ostream & fout ) {
        JsonOut json( fout, true ); // pretty-print
        json.write( world->active_mod_order );
    }, _( "list of mods" ) );
}

void mod_manager::load_mods_list( WORLDINFO *world ) const
{
    if( world == nullptr ) {
        return;
    }
    std::vector<mod_id> &amo = world->active_mod_order;
    amo.clear();
    bool obsolete_mod_found = false;
    read_from_file_json( get_mods_list_file( world ), [&]( JsonIn & jsin ) {
        for( const std::string line : jsin.get_array() ) {
            const mod_id mod( line );
            if( std::find( amo.begin(), amo.end(), mod ) != amo.end() ) {
                continue;
            }
            const auto iter = mod_replacements.find( mod );
            if( iter != mod_replacements.end() ) {
                if( !iter->second.is_empty() ) {
                    amo.push_back( iter->second );
                }
                obsolete_mod_found = true;
                continue;
            }
            amo.push_back( mod );
        }
    }, true );
    if( obsolete_mod_found ) {
        // If we found an obsolete mod, overwrite the mod list without the obsolete one.
        save_mods_list( world );
    }
}

const mod_manager::t_mod_list &mod_manager::get_default_mods() const
{
    return default_mods;
}

inline bool compare_mod_by_name_and_category( const MOD_INFORMATION *const a,
        const MOD_INFORMATION *const b )
{
    return localized_compare( std::make_pair( a->category, a->name() ),
                              std::make_pair( b->category, b->name() ) );
}

std::vector<mod_id> mod_manager::get_all_sorted() const
{
    std::vector<mod_id> available_cores, available_supplementals;
    std::vector<mod_id> ordered_mods;

    std::vector<const MOD_INFORMATION *> mods;
    mods.reserve( mod_map.size() );
    for( const auto &pair : mod_map ) {
        mods.push_back( &pair.second );
    }
    std::sort( mods.begin(), mods.end(), &compare_mod_by_name_and_category );

    for( const MOD_INFORMATION *const modinfo : mods ) {
        if( modinfo->core ) {
            available_cores.push_back( modinfo->ident );
        } else {
            available_supplementals.push_back( modinfo->ident );
        }
    }
    ordered_mods.insert( ordered_mods.begin(), available_supplementals.begin(),
                         available_supplementals.end() );
    ordered_mods.insert( ordered_mods.begin(), available_cores.begin(), available_cores.end() );

    return ordered_mods;
}

translatable_mod_info::translatable_mod_info()
{
    language_version = INVALID_LANGUAGE_VERSION;
}

translatable_mod_info::translatable_mod_info( std::string name,
        std::string description, std::string mod_path ) :
    mod_path( std::move( mod_path ) ), name_raw_( std::move( name ) ),
    description_raw( std::move( description ) )
{
    language_version = INVALID_LANGUAGE_VERSION;
}

auto translatable_mod_info::name() -> std::string
{
    if( name_raw_.empty() ) {
        return "";
    }
    if( language_version != detail::get_current_language_version() ) {
        update();
    }
    return name_tr;
}

auto translatable_mod_info::name_raw() const -> std::string
{
    return name_raw_;
}

auto translatable_mod_info::description() -> std::string
{
    if( description_raw.empty() ) {
        return "";
    }
    if( language_version != detail::get_current_language_version() ) {
        update();
    }
    return description_tr;
}
