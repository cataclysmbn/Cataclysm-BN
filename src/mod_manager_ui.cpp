#include "mod_manager.h" // IWYU pragma: associated

#include <algorithm>
#include <exception>

#include "color.h"
#include "debug.h"
#include "catalua.h"
#include "dependency_tree.h"
#include "output.h"
#include "string_formatter.h"
#include "string_id.h"
#include "translations.h"

mod_ui::mod_ui( mod_manager &mman )
    : active_manager( mman )
    , mm_tree( active_manager.get_tree() )
{
}

std::string mod_ui::get_information( const MOD_INFORMATION *mod )
{
    if( mod == nullptr ) {
        return "";
    }

    std::string info;

    if( mod->obsolete ) {
        info += colorize( _( "This mod is marked as obsolete and may contain severe errors." ), c_yellow );
        info += "\n";
    }

    if( !mod->authors.empty() ) {
        info += colorize( vgettext( "Author", "Authors", mod->authors.size() ),
                          c_light_blue ) + ": " + enumerate_as_string( mod->authors );
        if( mod->maintainers.empty() ) {
            info += "\n";
        } else {
            info += "  ";
        }
    }

    if( !mod->maintainers.empty() ) {
        const char8_t *const non_breaking_space = u8":\u00a0";
        info += colorize( vgettext( "Maintainer", "Maintainers", mod->maintainers.size() ),
                          c_light_blue ) +
                // HACK: Cannot fix that without switching whole project to std::u8string.
                + reinterpret_cast<const char *>( non_breaking_space )
                + enumerate_as_string( mod->maintainers ) + "\n";
    }

    if( !mod->dependencies.empty() ) {
        const auto &deps = mod->dependencies;
        auto str = enumerate_as_string( deps.begin(), deps.end(), [&]( const mod_id & e ) {
            if( e.is_valid() ) {
                return string_format( "[%s]", e->name() );
            } else {
                return string_format( "[<color_red>%s</color>]", e.c_str() );
            }
        } );
        info += colorize( vgettext( "Dependency", "Dependencies", deps.size() ),
                          c_light_blue ) + ": " + str + "\n";
    }

    if( !mod->conflicts.empty() ) {
        const auto &list = mod->conflicts;
        auto str = enumerate_as_string( list.begin(), list.end(), [&]( const mod_id & e ) {
            if( e.is_valid() ) {
                return string_format( "[%s]", e->name() );
            } else {
                return string_format( "[<color_dark_gray>%s</color>]", e.str() );
            }
        } );
        //~ Followed by list of mods the current mod conflicts with
        info += colorize( vgettext( "Conflict", "Conflicts", list.size() ),
                          c_light_blue ) + ": " + str + "\n";
    }

    if( !mod->version.empty() ) {
        info += colorize( _( "Mod version" ), c_light_blue ) + ": " + mod->version + "\n";
    }

    if( mod->lua_api_version ) {
        nc_color col_lua = cata::has_lua() ? c_light_blue : c_red;
        int this_api = cata::get_lua_api_version();
        nc_color col_api = this_api == *mod->lua_api_version ? c_white : c_yellow;
        info += string_format(
                    _( "%s: API version %s\n" ),
                    colorize( _( "Needs Lua" ), col_lua ),
                    colorize( string_format( "%d", *mod->lua_api_version ), col_api )
                );
    }

    if( !mod->description().empty() ) {
        info += mod->description() + "\n";
    }

    info += colorize( _( "Mod info path" ), c_light_blue ) + ": " + mod->path_full + "\n";

    std::string note = !mm_tree.is_available( mod->ident ) ? mm_tree.get_node(
                           mod->ident )->s_errors() : "";
    if( !note.empty() ) {
        info += colorize( note, c_red );
    }

    return info;
}

struct conflict_pair {
    mod_id what;
    mod_id with;
};

static std::string fmt_conflicts( const std::vector<conflict_pair> &list )
{
    std::string ret;

    for( const conflict_pair &elem : list ) {
        //~ Single entry in the list of which mods conflict with which mods.
        //~ Please be mindful of spaces.
        //~ "%s [%s]" parts are mod names with internal ids.
        //~ The final result reads as "Mod X [id_x] with Mod Y [id_y]"
        ret += string_format( _( "  %s [%s]\n    with %s [%s]\n" ),
                              elem.what->name(), elem.what,
                              elem.with->name(), elem.with );
    }

    return ret;
}

static void check_conflict( const mod_id &what, const mod_id &with,
                            std::vector<conflict_pair> &ret )
{
    auto it = std::find( what->conflicts.begin(), what->conflicts.end(), with );
    if( it != what->conflicts.end() ) {
        ret.emplace_back( conflict_pair{ what, with } );
        return;
    }

    it = std::find( with->conflicts.begin(), with->conflicts.end(), what );
    if( it != with->conflicts.end() ) {
        ret.emplace_back( conflict_pair{ what, with } );
    }
}

static void check_conflicts( const mod_id &mod, const std::vector<mod_id> &active_list,
                             std::vector<conflict_pair> &ret )
{
    for( const auto &elem : active_list ) {
        check_conflict( mod, elem, ret );
    }
}

ret_val<bool> mod_ui::try_add( const mod_id &mod_to_add, std::vector<mod_id> &active_list )
{
    if( std::ranges::find( active_list, mod_to_add ) != active_list.end() ) {
        // The same mod can not be added twice. That makes no sense.
        return ret_val<bool>::make_failure( _( "The mod is already on the list." ) );
    }
    if( !mod_to_add.is_valid() ) {
        return ret_val<bool>::make_failure( _( "Unable to find mod with id \"%s\"." ), mod_to_add );
    }
    const MOD_INFORMATION &mod = *mod_to_add;
    dependency_node *checknode = mm_tree.get_node( mod.ident );
    if( !checknode ) {
        return ret_val<bool>::make_failure( _( "Failed to build dependency tree for the mod." ) );
    }
    if( checknode->has_errors() ) {
        return ret_val<bool>::make_failure( _( "The mod has dependency problem(s):\n\n%s" ),
                                            checknode->s_errors() );
    }

    // get dependencies of selection in the order that they would appear from the top of the active list
    std::vector<mod_id> dependencies = mm_tree.get_dependencies_of_X_as_strings( mod.ident );

    std::vector<conflict_pair> conflicts;
    check_conflicts( mod_to_add, active_list, conflicts );
    for( const auto &dep : dependencies ) {
        check_conflicts( dep, active_list, conflicts );
    }
    if( !conflicts.empty() ) {
        return ret_val<bool>::make_failure(
                   _( "The mod or some of its dependencies has conflict(s) with active mods:\n\n%s" ),
                   fmt_conflicts( conflicts ) );
    }

    // check to see if mod is a core, and if so check to see if there is already a core in the mod list
    if( mod.core ) {
        //  (more than 0 active elements) && (active[0] is a CORE) && active[0] is not the add candidate
        if( !active_list.empty() && active_list[0]->core && active_list[0] != mod_to_add ) {
            // remove existing core
            try_rem( 0, active_list );
        }

        // add to start of active_list if it doesn't already exist in it
        active_list.insert( active_list.begin(), mod_to_add );
    } else {
        // now check dependencies and add them as necessary
        std::vector<mod_id> mods_to_add;
        bool new_core = false;
        for( auto &i : dependencies ) {
            if( std::ranges::find( active_list, i ) == active_list.end() ) {
                if( i->core ) {
                    mods_to_add.insert( mods_to_add.begin(), i );
                    new_core = true;
                } else {
                    mods_to_add.push_back( i );
                }
            }
        }

        if( new_core && !active_list.empty() ) {
            try_rem( 0, active_list );
            active_list.insert( active_list.begin(), mods_to_add[0] );
            mods_to_add.erase( mods_to_add.begin() );
        }
        // now add the rest of the dependencies serially to the end
        for( auto &i : mods_to_add ) {
            active_list.push_back( i );
        }
        // and finally add the one we are trying to add!
        active_list.push_back( mod.ident );
    }

    return ret_val<bool>::make_success();
}

void mod_ui::try_rem( size_t selection, std::vector<mod_id> &active_list )
{
    // first make sure that what we are looking for exists in the list
    if( selection >= active_list.size() ) {
        // trying to access an out of bounds value! quit early
        return;
    }
    const mod_id sel_string = active_list[selection];

    const MOD_INFORMATION &mod = *active_list[selection];

    std::vector<mod_id> dependents = mm_tree.get_dependents_of_X_as_strings( mod.ident );

    // search through the rest of the active list for mods that depend on this one
    for( auto &i : dependents ) {
        auto rem = std::ranges::find( active_list, i );
        if( rem != active_list.end() ) {
            active_list.erase( rem );
        }
    }
    std::vector<mod_id>::iterator rem = std::ranges::find( active_list,
                                        sel_string );
    if( rem != active_list.end() ) {
        active_list.erase( rem );
    }
}

void mod_ui::try_shift( char direction, size_t &selection, std::vector<mod_id> &active_list )
{
    // error catch for out of bounds
    if( selection >= active_list.size() ) {
        return;
    }

    // eliminates 'uninitialized variable' warning
    size_t newsel = 0;
    size_t oldsel = 0;
    int selshift = 0;

    // shift up (towards 0)
    if( direction == '+' && can_shift_up( selection, active_list ) ) {
        // see if the mod at selection-1 is a) a core, or b) is depended on by this mod
        newsel = selection - 1;
        oldsel = selection;

        selshift = -1;
    }
    // shift down (towards active_list.size()-1)
    else if( direction == '-' && can_shift_down( selection, active_list ) ) {
        newsel = selection;
        oldsel = selection + 1;

        selshift = +1;
    } else {
        return;
    }

    mod_id modstring = active_list[newsel];
    mod_id selstring = active_list[oldsel];

    // we can shift!
    // switch values!
    active_list[newsel] = selstring;
    active_list[oldsel] = modstring;

    selection += selshift;
}

bool mod_ui::can_shift_up( size_t selection, const std::vector<mod_id> &active_list )
{
    // error catch for out of bounds
    if( selection >= active_list.size() ) {
        return false;
    }
    // dependencies of this active element
    std::vector<mod_id> dependencies = mm_tree.get_dependencies_of_X_as_strings(
                                           active_list[selection] );

    // figure out if we can move up!
    if( selection == 0 ) {
        // can't move up, don't bother trying
        return false;
    }
    // see if the mod at selection-1 is a) a core, or b) is depended on by this mod
    int newsel = selection - 1;

    mod_id newsel_id = active_list[newsel];
    bool newsel_is_dependency =
        std::ranges::find( dependencies, newsel_id ) != dependencies.end();

    return !newsel_id->core && !newsel_is_dependency;
}

bool mod_ui::can_shift_down( size_t selection, const std::vector<mod_id> &active_list )
{
    // error catch for out of bounds
    if( selection >= active_list.size() ) {
        return false;
    }
    std::vector<mod_id> dependents = mm_tree.get_dependents_of_X_as_strings(
                                         active_list[selection] );

    // figure out if we can move down!
    if( selection == active_list.size() - 1 ) {
        // can't move down, don't bother trying
        return false;
    }
    int newsel = selection;
    int oldsel = selection + 1;

    mod_id modstring = active_list[newsel];
    mod_id selstring = active_list[oldsel];
    bool sel_is_dependency =
        std::ranges::find( dependents, selstring ) != dependents.end();

    return !modstring->core && !sel_is_dependency;
}
