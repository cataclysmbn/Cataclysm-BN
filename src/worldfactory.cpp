#include "worldfactory.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <memory>
#include <set>
#include <unordered_map>
#include <utility>

#include "cata_utility.h"
#include "catacharset.h"
#include "catalua.h"
#include "color.h"
#include "cursesdef.h"
#include "debug.h"
#include "enums.h"
#include "filesystem.h"
#include "fstream_utils.h"
#include "game.h"
#include "ime.h"
#include "input.h"
#include "json.h"
#include "mod_manager.h"
#include "output.h"
#include "path_info.h"
#include "point.h"
#include "string_formatter.h"
#include "string_id.h"
#include "string_input_popup.h"
#include "string_utils.h"
#include "translations.h"
#include "ui_manager.h"
#include "name.h"

using namespace std::placeholders;

// single instance of world generator
std::unique_ptr<worldfactory> world_generator;

/**
  * Max utf-8 character worldname length.
  * 0 index is inclusive.
  */
static const int max_worldname_len = 32;

worldfactory::worldfactory()
    : active_world( nullptr )
    , mman_ui( *mman )
{
    // prepare tab display order
    tabs.emplace_back( std::bind( &worldfactory::show_worldgen_tab_modselection, this, _1, _2, _3 ) );
    tabs.emplace_back( std::bind( &worldfactory::show_worldgen_tab_options, this, _1, _2, _3 ) );
    tabs.emplace_back( std::bind( &worldfactory::show_worldgen_tab_confirm, this, _1, _2, _3 ) );
}

worldfactory::~worldfactory() = default;

WORLDINFO *worldfactory::add_world( std::unique_ptr<WORLDINFO> retworld )
{
    if( !retworld->save() ) {
        return nullptr;
    }
    return ( all_worlds[ retworld->world_name ] = std::move( retworld ) ).get();
}

WORLDINFO *worldfactory::make_new_world( const std::vector<mod_id> &mods )
{
    std::unique_ptr<WORLDINFO> retworld = std::make_unique<WORLDINFO>();
    retworld->active_mod_order = mods;
    return add_world( std::move( retworld ) );
}

WORLDINFO *worldfactory::make_new_world( bool show_prompt, const std::string &world_to_copy )
{
    // World to return after generating
    std::unique_ptr<WORLDINFO> retworld = std::make_unique<WORLDINFO>();

    if( !world_to_copy.empty() ) {
        retworld->COPY_WORLD( world_generator->get_world( world_to_copy ) );
    }

    if( show_prompt ) {
        // set up window
        catacurses::window wf_win;
        ui_adaptor ui;

        const auto init_windows = [&]( ui_adaptor & ui ) {
            const int iMinScreenWidth = std::max( FULL_SCREEN_WIDTH, TERMX / 2 );
            const int iOffsetX = TERMX > FULL_SCREEN_WIDTH ? ( TERMX - iMinScreenWidth ) / 2 : 0;
            wf_win = catacurses::newwin( TERMY, iMinScreenWidth, point( iOffsetX, 0 ) );
            ui.position_from_window( wf_win );
        };
        init_windows( ui );
        ui.on_screen_resize( init_windows );

        int curtab = 0;

        ui.on_redraw( [&]( const ui_adaptor & ) {
            draw_worldgen_tabs( wf_win, static_cast<size_t>( curtab ) );
            wnoutrefresh( wf_win );
        } );

        const size_t numtabs = tabs.size();
        while( static_cast<size_t>( curtab ) < numtabs ) {
            ui_manager::redraw();
            curtab += tabs[curtab]( wf_win, retworld.get(), []() -> bool {
                return query_yn( _( "Do you want to abort World Generation?" ) );
            } );
        }
        if( curtab < 0 ) {
            return nullptr;
        }
    }

    return add_world( std::move( retworld ) );
}

WORLDINFO *worldfactory::make_new_world( special_game_type special_type )
{
    std::string worldname;
    switch( special_type ) {
        case special_game_type::TUTORIAL:
            worldname = "TUTORIAL";
            break;
        case special_game_type::DEFENSE:
            worldname = "DEFENSE";
            break;
        default:
            return nullptr;
    }

    // Look through all worlds and see if a world named worldname already exists. If so, then just return it instead of
    // making a new world.
    if( has_world( worldname ) ) {
        return all_worlds[worldname].get();
    }

    std::unique_ptr<WORLDINFO> special_world = std::make_unique<WORLDINFO>();
    special_world->world_name = worldname;

    special_world->WORLD_OPTIONS["WORLD_END"].setValue( "delete" );

    if( !special_world->save() ) {
        return nullptr;
    }

    return ( all_worlds[worldname] = std::move( special_world ) ).get();
}

void worldfactory::set_active_world( WORLDINFO *new_world )
{
    DebugLog( DL::Info, DC::Main ) << "Setting active world to " << ( new_world ?
                                   new_world->folder_path() : "NULL" );

    if( new_world ) {
        get_options().set_world_options( &new_world->WORLD_OPTIONS );
        active_world = std::make_unique<world>( new_world );
    } else {
        get_options().set_world_options( nullptr );
        active_world = nullptr;
    }
}

void worldfactory::init()
{
    load_last_world_info();

    std::vector<std::string> qualifiers;
    qualifiers.push_back( PATH_INFO::worldoptions() );
    qualifiers.push_back( SAVE_MASTER );

    all_worlds.clear();

    // get the master files. These determine the validity of a world
    // worlds exist by having an option file
    // create worlds
    for( const auto &world_dir : get_directories_with( qualifiers, PATH_INFO::savedir(), true ) ) {
        // get the save files
        auto world_sav_files = get_files_from_path( SAVE_EXTENSION, world_dir, false );
        // split the save file names between the directory and the extension
        for( auto &world_sav_file : world_sav_files ) {
            size_t save_index = world_sav_file.find( SAVE_EXTENSION );
            world_sav_file = world_sav_file.substr( world_dir.size() + 1,
                                                    save_index - ( world_dir.size() + 1 ) );
        }
        // the directory name is the name of the world
        std::string worldname;
        size_t name_index = world_dir.find_last_of( "/\\" );
        worldname = world_dir.substr( name_index + 1 );

        // create and store the world
        all_worlds[worldname] = std::make_unique<WORLDINFO>();
        // give the world a name
        all_worlds[worldname]->world_name = worldname;
        // Record the world save format. V2 is identified by the presence of a map.sqlite3 file.
        if( file_exist( world_dir + "/map.sqlite3" ) ) {
            all_worlds[worldname]->world_save_format = save_format::V2_COMPRESSED_SQLITE3;
        } else {
            all_worlds[worldname]->world_save_format = save_format::V1;
        }
        // add sav files
        for( auto &world_sav_file : world_sav_files ) {
            all_worlds[worldname]->world_saves.push_back( save_t::from_base_path( world_sav_file ) );
        }
        mman->load_mods_list( all_worlds[worldname].get() );

        // load options into the world
        if( !all_worlds[worldname]->load_options() ) {
            all_worlds[worldname]->WORLD_OPTIONS = get_options().get_world_defaults();
            all_worlds[worldname]->WORLD_OPTIONS["WORLD_END"].setValue( "delete" );
            all_worlds[worldname]->save();
        }
    }

    // check to see if there exists a worldname "save" which denotes that a world exists in the save
    // directory and not in a sub-world directory
    if( has_world( "save" ) ) {
        const WORLDINFO &old_world = *all_worlds["save"];

        std::unique_ptr<WORLDINFO> newworld = std::make_unique<WORLDINFO>();
        newworld->world_name = get_next_valid_worldname();

        // save world as conversion world
        if( newworld->save( true ) ) {
            const std::string origin_path = old_world.folder_path();
            // move files from origin_path into new world path
            for( auto &origin_file : get_files_from_path( ".", origin_path, false ) ) {
                std::string filename = origin_file.substr( origin_file.find_last_of( "/\\" ) );

                rename( origin_file.c_str(), ( newworld->folder_path() + filename ).c_str() );
            }
            newworld->world_saves = old_world.world_saves;
            newworld->WORLD_OPTIONS = old_world.WORLD_OPTIONS;

            all_worlds.erase( "save" );

            all_worlds[newworld->world_name] = std::move( newworld );
        } else {
            debugmsg( "worldfactory::convert_to_world -- World Conversion Failed!" );
        }
    }
}

bool worldfactory::has_world( const std::string &name ) const
{
    return all_worlds.contains( name );
}

std::vector<std::string> worldfactory::all_worldnames() const
{
    std::vector<std::string> result;
    result.reserve( all_worlds.size() );
    for( auto &elem : all_worlds ) {
        result.push_back( elem.first );
    }
    return result;
}

WORLDINFO *worldfactory::pick_world( bool show_prompt, bool empty_only )
{
    std::vector<std::string> world_names = all_worldnames();

    // Filter out special worlds (TUTORIAL | DEFENSE) from world_names.
    for( std::vector<std::string>::iterator it = world_names.begin(); it != world_names.end(); ) {
        if( *it == "TUTORIAL" || *it == "DEFENSE" ||
            ( empty_only && !get_world( *it )->world_saves.empty() ) ) {
            it = world_names.erase( it );
        } else {
            ++it;
        }
    }
    // If there is only one world to pick from, autoreturn it.
    if( world_names.size() == 1 ) {
        return get_world( world_names[0] );
    }
    // If there are no worlds to pick from, immediately try to make one.
    else if( world_names.empty() ) {
        return make_new_world( show_prompt );
    }
    // If we're skipping prompts, return the world with 0 save if there is one
    else if( !show_prompt ) {
        for( const auto &name : world_names ) {
            if( get_world( name )->world_saves.empty() ) {
                return get_world( name );
            }
        }
        // if there isn't any, adhere to old logic: return the alphabetically first one
        return get_world( world_names[0] );
    }

    const int iTooltipHeight = 3;
    int iContentHeight = 0;
    int iMinScreenWidth = 0;
    size_t num_pages = 1;

    std::map<int, bool> mapLines;
    mapLines[3] = true;

    std::map<int, std::vector<std::string> > world_pages;
    size_t sel = 0, selpage = 0;

    catacurses::window w_worlds_border;
    catacurses::window w_worlds_tooltip;
    catacurses::window w_worlds_header;
    catacurses::window w_worlds;

    ui_adaptor ui;

    const auto init_windows = [&]( ui_adaptor & ui ) {
        iContentHeight = TERMY - 3 - iTooltipHeight;
        iMinScreenWidth = std::max( FULL_SCREEN_WIDTH, TERMX / 2 );
        const int iOffsetX = TERMX > FULL_SCREEN_WIDTH ? ( TERMX - iMinScreenWidth ) / 2 : 0;
        num_pages = world_names.size() / iContentHeight + 1; // at least 1 page

        world_pages.clear();
        size_t worldnum = 0;
        for( size_t i = 0; i < num_pages; ++i ) {
            for( int j = 0; j < iContentHeight && worldnum < world_names.size(); ++j ) {
                world_pages[i].push_back( world_names[ worldnum++ ] );
            }
        }

        w_worlds_border  = catacurses::newwin( TERMY, iMinScreenWidth,
                                               point( iOffsetX, 0 ) );
        w_worlds_tooltip = catacurses::newwin( iTooltipHeight, iMinScreenWidth - 2,
                                               point( 1 + iOffsetX, 1 ) );
        w_worlds_header  = catacurses::newwin( 1, iMinScreenWidth - 2,
                                               point( 1 + iOffsetX, 1 + iTooltipHeight ) );
        w_worlds         = catacurses::newwin( iContentHeight, iMinScreenWidth - 2,
                                               point( 1 + iOffsetX, iTooltipHeight + 2 ) );

        ui.position_from_window( w_worlds_border );
    };
    init_windows( ui );
    ui.on_screen_resize( init_windows );

    ui.on_redraw( [&]( const ui_adaptor & ) {
        draw_border( w_worlds_border, BORDER_COLOR, _( " WORLD SELECTION " ) );
        mvwputch( w_worlds_border, point( 0, 4 ), BORDER_COLOR, LINE_XXXO ); // |-
        mvwputch( w_worlds_border, point( iMinScreenWidth - 1, 4 ), BORDER_COLOR, LINE_XOXX ); // -|

        for( auto &mapLine : mapLines ) {
            if( mapLine.second ) {
                mvwputch( w_worlds_border, point( mapLine.first + 1, TERMY - 1 ), BORDER_COLOR,
                          LINE_XXOX ); // _|_
            }
        }

        wnoutrefresh( w_worlds_border );

        for( int i = 0; i < getmaxx( w_worlds_border ); i++ ) {
            if( mapLines[i] ) {
                mvwputch( w_worlds_header, point( i, 0 ), BORDER_COLOR, LINE_OXXX );
            } else {
                mvwputch( w_worlds_header, point( i, 0 ), BORDER_COLOR, LINE_OXOX ); // Draw header line
            }
        }

        wnoutrefresh( w_worlds_header );

        //Clear the lines
        for( int i = 0; i < iContentHeight; i++ ) {
            for( int j = 0; j < getmaxx( w_worlds ); j++ ) {
                if( mapLines[j] ) {
                    mvwputch( w_worlds, point( j, i ), BORDER_COLOR, LINE_XOXO );
                } else {
                    mvwputch( w_worlds, point( j, i ), c_black, ' ' );
                }

                if( i < iTooltipHeight ) {
                    mvwputch( w_worlds_tooltip, point( j, i ), c_black, ' ' );
                }
            }
        }

        //Draw World Names
        for( size_t i = 0; i < world_pages[selpage].size(); ++i ) {
            mvwprintz( w_worlds, point( 0, static_cast<int>( i ) ), c_white, "%d", i + 1 );
            wmove( w_worlds, point( 4, static_cast<int>( i ) ) );

            std::string world_name = ( world_pages[selpage] )[i];
            WORLDINFO *world = get_world( world_name );
            size_t saves_num = world->world_saves.size();

            std::string text = string_format( "%s (%d)", world_name, saves_num );
            nc_color col = c_white;

            if( i == sel ) {
                wprintz( w_worlds, c_yellow, ">> " );
            } else {
                wprintz( w_worlds, c_yellow, "   " );
            }

            wprintz( w_worlds, col, text );
        }

        //Draw Tabs
        wmove( w_worlds_header, point( 7, 0 ) );

        for( size_t i = 0; i < num_pages; ++i ) {
            //skip empty pages
            if( !world_pages[i].empty() ) {
                nc_color tabcolor = ( selpage == i ) ? hilite( c_white ) : c_white;
                wprintz( w_worlds_header, c_white, "[" );
                wprintz( w_worlds_header, tabcolor, _( "Page %lu" ), i + 1 );
                wprintz( w_worlds_header, c_white, "]" );
                wputch( w_worlds_header, BORDER_COLOR, LINE_OXOX );
            }
        }

        wnoutrefresh( w_worlds_header );

        fold_and_print( w_worlds_tooltip, point_zero, 78, c_white, _( "Pick a world to enter game" ) );
        wnoutrefresh( w_worlds_tooltip );

        wnoutrefresh( w_worlds );
    } );

    input_context ctxt( "PICK_WORLD_DIALOG" );
    ctxt.register_updown();
    ctxt.register_action( "HELP_KEYBINDINGS" );
    ctxt.register_action( "QUIT" );
    ctxt.register_action( "NEXT_TAB" );
    ctxt.register_action( "PREV_TAB" );
    ctxt.register_action( "CONFIRM" );

    while( true ) {
        ui_manager::redraw();

        const std::string action = ctxt.handle_input();

        if( action == "QUIT" ) {
            break;
        } else if( !world_pages[selpage].empty() && action == "DOWN" ) {
            sel++;
            if( sel >= world_pages[selpage].size() ) {
                sel = 0;
            }
        } else if( !world_pages[selpage].empty() && action == "UP" ) {
            if( sel == 0 ) {
                sel = world_pages[selpage].size() - 1;
            } else {
                sel--;
            }
        } else if( action == "NEXT_TAB" ) {
            sel = 0;

            do {
                //skip empty pages
                selpage++;
                if( selpage >= world_pages.size() ) {
                    selpage = 0;
                }
            } while( world_pages[selpage].empty() );
        } else if( action == "PREV_TAB" ) {
            sel = 0;
            do {
                //skip empty pages
                if( selpage != 0 ) {
                    selpage--;
                } else {
                    selpage = world_pages.size() - 1;
                }
            } while( world_pages[selpage].empty() );
        } else if( action == "CONFIRM" ) {
            WORLDINFO *world = get_world( world_pages[selpage][sel] );
            return world;
        }
    }

    return nullptr;
}

void worldfactory::remove_world( const std::string &worldname )
{
    auto it = all_worlds.find( worldname );
    if( it != all_worlds.end() ) {
        WORLDINFO *wptr = it->second.get();
        if( active_world && active_world->info == wptr ) {
            set_active_world( nullptr );
        }
        all_worlds.erase( it );
    }
}

void worldfactory::load_last_world_info()
{
    cata_ifstream file = std::move( cata_ifstream().mode( cata_ios_mode::binary ).open(
                                        PATH_INFO::lastworld() ) );
    if( !file.is_open() ) {
        return;
    }

    JsonIn jsin( *file, PATH_INFO::lastworld() );
    try {
        JsonObject data = jsin.get_object();
        last_world_name = data.get_string( "world_name" );
        last_character_name = data.get_string( "character_name" );
    } catch( const std::exception &e ) {
        debugmsg( e.what() );
    }
}

void worldfactory::save_last_world_info()
{
    write_to_file( PATH_INFO::lastworld(), [&]( std::ostream & file ) {
        JsonOut jsout( file, true );
        jsout.start_object();
        jsout.member( "world_name", last_world_name );
        jsout.member( "character_name", last_character_name );
        jsout.end_object();
    }, _( "last world info" ) );
}

std::string worldfactory::pick_random_name()
{
    // TODO: add some random worldname parameters to name generator
    return get_next_valid_worldname();
}

std::string worldfactory::get_next_valid_worldname()
{
    std::string worldname = Name::get( nameIsWorldName );

    return worldname;
}

int worldfactory::show_worldgen_tab_options( const catacurses::window &, WORLDINFO *world,
        const std::function<bool()> &on_quit )
{
    get_options().set_world_options( &world->WORLD_OPTIONS );
    const std::string action = get_options().show( false, true, on_quit );
    get_options().set_world_options( nullptr );
    if( action == "PREV_TAB" ) {
        return -1;

    } else if( action == "NEXT_TAB" ) {
        return 1;

    } else if( action == "QUIT" ) {
        return -999;
    }

    return 0;
}

void worldfactory::draw_mod_list( const catacurses::window &w, int &start, size_t cursor,
                                  const std::vector<mod_id> &mods, bool is_active_list,
                                  const std::string &text_if_empty, const catacurses::window &w_shift )
{
    werase( w );
    werase( w_shift );

    const int iMaxRows = getmaxy( w );
    size_t iModNum = mods.size();
    size_t iActive = cursor;
    bool first_line_is_category = false;

    if( mods.empty() ) {
        center_print( w, 0, c_red, text_if_empty );
    } else {
        int iCatSortNum = 0;
        std::string sLastCategoryName;
        std::map<int, std::string> mSortCategory;
        mSortCategory[0] = sLastCategoryName;

        for( size_t i = 0; i < mods.size(); ++i ) {
            std::string category_name = _( "MISSING MODS" );
            if( mods[i].is_valid() ) {
                category_name = _( mods[i]->category.second );
            }
            if( sLastCategoryName != category_name ) {
                sLastCategoryName = category_name;
                mSortCategory[ i + iCatSortNum++ ] = sLastCategoryName;
                iModNum++;
                if( i == 0 ) {
                    first_line_is_category = true;
                }
            }
        }

        const int wwidth = getmaxx( w ) - 1 - 3; // border (1) + ">> " (3)

        unsigned int iNum = 0;
        bool bKeepIter = false;

        for( size_t i = 0; i <= iActive; i++ ) {
            if( !mSortCategory[i].empty() ) {
                iActive++;
            }
        }

        calcStartPos( start, iActive, iMaxRows, iModNum );

        for( int i = 0; i < start; i++ ) {
            if( !mSortCategory[i].empty() ) {
                iNum++;
            }
        }

        int larger = ( iMaxRows > static_cast<int>( iModNum ) ) ? static_cast<int>( iModNum ) : iMaxRows;
        for( auto iter = mods.begin(); iter != mods.end(); ) {
            if( iNum >= static_cast<size_t>( start ) && iNum < static_cast<size_t>( start + larger ) ) {
                if( !mSortCategory[iNum].empty() ) {
                    bKeepIter = true;
                    trim_and_print( w, point( 1, iNum - start ), wwidth, c_magenta, mSortCategory[iNum] );

                } else {
                    if( iNum == iActive ) {
                        if( is_active_list ) {
                            mvwprintz( w, point( 1, iNum - start ), c_yellow, ">> " );
                        } else {
                            mvwprintz( w, point( 1, iNum - start ), c_blue, ">> " );
                        }
                    }

                    const mod_id &mod_entry_id = *iter;
                    std::string mod_entry_name = string_format( _( " [%s]" ), mod_entry_id.str() );
                    nc_color mod_entry_color = c_white;
                    if( mod_entry_id.is_valid() ) {
                        const MOD_INFORMATION &mod = *mod_entry_id;
                        mod_entry_name = mod.name() + mod_entry_name;
                        if( mod.obsolete ) {
                            mod_entry_color = c_dark_gray;
                            mod_entry_name = remove_color_tags( mod_entry_name ) + "*";
                        }
                    } else {
                        mod_entry_color = c_light_red;
                        mod_entry_name = _( "N/A" ) + mod_entry_name;

                    }
                    trim_and_print( w, point( 4, iNum - start ), wwidth, mod_entry_color, mod_entry_name );

                    if( w_shift ) {
                        // get shift information for the active item
                        std::string shift_display;
                        const size_t iPos = std::distance( mods.begin(), iter );

                        if( mman_ui->can_shift_up( iPos, mods ) ) {
                            shift_display += "<color_blue>+</color> ";
                        } else {
                            shift_display += "<color_dark_gray>+</color> ";
                        }

                        if( mman_ui->can_shift_down( iPos, mods ) ) {
                            shift_display += "<color_blue>-</color>";
                        } else {
                            shift_display += "<color_dark_gray>-</color>";
                        }

                        trim_and_print( w_shift, point( 1, 2 + iNum - start ), 3, c_white, shift_display );
                    }
                }
            }

            if( bKeepIter ) {
                bKeepIter = false;
            } else {
                ++iter;
            }

            iNum++;
        }
    }

    // Ensure that the scrollbar starts at zero position
    if( first_line_is_category && iActive == 1 ) {
        draw_scrollbar( w, 0, iMaxRows, static_cast<int>( iModNum ), point_zero );
    } else {
        draw_scrollbar( w, static_cast<int>( iActive ), iMaxRows, static_cast<int>( iModNum ), point_zero );
    }

    wnoutrefresh( w );
    wnoutrefresh( w_shift );
}

void worldfactory::show_active_world_mods( const std::vector<mod_id> &world_mods )
{
    ui_adaptor ui;
    catacurses::window w_border;
    catacurses::window w_mods;

    const auto init_windows = [&]( ui_adaptor & ui ) {
        const int iMinScreenWidth = std::max( FULL_SCREEN_WIDTH, TERMX / 2 );
        const int iOffsetX = TERMX > FULL_SCREEN_WIDTH ? ( TERMX - iMinScreenWidth ) / 2 : 0;

        w_border = catacurses::newwin( TERMY - 11, iMinScreenWidth / 2 - 3,
                                       point( iOffsetX, 4 ) );
        w_mods   = catacurses::newwin( TERMY - 13, iMinScreenWidth / 2 - 4,
                                       point( iOffsetX, 5 ) );

        ui.position_from_window( w_border );
    };
    init_windows( ui );
    ui.on_screen_resize( init_windows );

    int start = 0;
    int cursor = 0;
    const size_t num_mods = world_mods.size();

    input_context ctxt( "DEFAULT" );
    ctxt.register_updown();
    ctxt.register_action( "QUIT" );
    ctxt.register_action( "CONFIRM" );
    ctxt.register_action( "HELP_KEYBINDINGS" );

    ui.on_redraw( [&]( const ui_adaptor & ) {
        draw_border( w_border, BORDER_COLOR, _( " ACTIVE WORLD MODS " ) );
        wnoutrefresh( w_border );

        draw_mod_list( w_mods, start, static_cast<size_t>( cursor ), world_mods,
                       true, _( "--NO ACTIVE MODS--" ), catacurses::window() );
        wnoutrefresh( w_mods );
    } );

    while( true ) {
        ui_manager::redraw();

        const std::string action = ctxt.handle_input();

        if( action == "UP" ) {
            cursor--;
            // If it went under 0, loop back to the end of the list.
            if( cursor < 0 ) {
                cursor = static_cast<int>( num_mods - 1 );
            }

        } else if( action == "DOWN" ) {
            cursor++;
            // If it went over the end of the list, loop back to the start of the list.
            if( cursor > static_cast<int>( num_mods - 1 ) ) {
                cursor = 0;
            }

        } else if( action == "QUIT" || action == "CONFIRM" ) {
            break;
        }
    }
}

void worldfactory::edit_active_world_mods( WORLDINFO *world )
{
    // set up window
    catacurses::window wf_win;
    ui_adaptor ui;

    const auto init_windows = [&]( ui_adaptor & ui ) {
        const int iMinScreenWidth = std::max( FULL_SCREEN_WIDTH, TERMX / 2 );
        const int iOffsetX = TERMX > FULL_SCREEN_WIDTH ? ( TERMX - iMinScreenWidth ) / 2 : 0;
        wf_win = catacurses::newwin( TERMY, iMinScreenWidth, point( iOffsetX, 0 ) );
        ui.position_from_window( wf_win );
    };
    init_windows( ui );
    ui.on_screen_resize( init_windows );

    ui.on_redraw( [&]( const ui_adaptor & ) {
        draw_empty_worldgen_tabs( wf_win );
        wnoutrefresh( wf_win );
    } );

    bool save_changes = false;
    mod_manager::t_mod_list new_mod_order = world->active_mod_order;
    show_modselection_window( wf_win, new_mod_order, [&save_changes]() {
        save_changes = query_yn( _( "Save changes?" ) );
        return true;
    }, nullptr, true );

    if( save_changes ) {
        world->active_mod_order = new_mod_order;
        world->save();
    }
}

int worldfactory::show_worldgen_tab_modselection( const catacurses::window &win, WORLDINFO *world,
        const std::function<bool()> &on_quit )
{
    return show_modselection_window( win, world->active_mod_order, on_quit, on_quit, false );
}

int worldfactory::show_modselection_window( const catacurses::window &win,
        std::vector<mod_id> &active_mod_order,
        const std::function<bool()> &on_quit,
        const std::function<bool()> &on_backtab,
        bool standalone )
{
    {
        std::vector<mod_id> tmp_mod_order;
        // clear active_mod_order and re-add all the mods, his ensures
        // that changes (like changing dependencies) get updated
        tmp_mod_order.swap( active_mod_order );
        for( auto &elem : tmp_mod_order ) {
            mman_ui->try_add( elem, active_mod_order );
        }
    }

    input_context ctxt( "MODMANAGER_DIALOG" );
    ctxt.register_updown();
    ctxt.register_action( "LEFT", to_translation( "Switch to other list" ) );
    ctxt.register_action( "RIGHT", to_translation( "Switch to other list" ) );
    ctxt.register_action( "HELP_KEYBINDINGS" );
    ctxt.register_action( "QUIT" );
    ctxt.register_action( "NEXT_CATEGORY_TAB" );
    ctxt.register_action( "PREV_CATEGORY_TAB" );
    if( !standalone ) {
        ctxt.register_action( "NEXT_TAB" );
        ctxt.register_action( "PREV_TAB" );
    }
    ctxt.register_action( "CONFIRM", to_translation( "Activate / deactivate mod" ) );
    ctxt.register_action( "ADD_MOD" );
    ctxt.register_action( "REMOVE_MOD" );
    ctxt.register_action( "SAVE_DEFAULT_MODS" );
    ctxt.register_action( "VIEW_MOD_DESCRIPTION" );
    ctxt.register_action( "FILTER" );
    ctxt.register_action( "TOGGLE_SHOW_OBSOLETE" );

    point filter_pos;
    int filter_view_len = 0;
    bool show_obsolete = false;
    std::string current_filter;
    std::unique_ptr<string_input_popup> fpopup;

    catacurses::window w_header1;
    catacurses::window w_header2;
    catacurses::window w_shift;
    catacurses::window w_list;
    catacurses::window w_active;
    catacurses::window w_description;
    std::vector<catacurses::window> header_windows;

    ui_adaptor ui;

    const auto init_windows = [&]( ui_adaptor & ui ) {
        const int iMinScreenWidth = std::max( FULL_SCREEN_WIDTH, TERMX / 2 );
        const int iOffsetX = TERMX > FULL_SCREEN_WIDTH ? ( TERMX - iMinScreenWidth ) / 2 : 0;

        w_header1     = catacurses::newwin( 1, iMinScreenWidth / 2 - 5,
                                            point( 1 + iOffsetX, 3 ) );
        w_header2     = catacurses::newwin( 1, iMinScreenWidth / 2 - 4,
                                            point( iMinScreenWidth / 2 + 3 + iOffsetX, 3 ) );
        w_shift       = catacurses::newwin( TERMY - 11, 5,
                                            point( iMinScreenWidth / 2 - 3 + iOffsetX, 3 ) );
        w_list        = catacurses::newwin( TERMY - 13, iMinScreenWidth / 2 - 4,
                                            point( iOffsetX, 5 ) );
        w_active      = catacurses::newwin( TERMY - 13, iMinScreenWidth / 2 - 4,
                                            point( iMinScreenWidth / 2 + 2 + iOffsetX, 5 ) );
        w_description = catacurses::newwin( 4, iMinScreenWidth - 4,
                                            point( 1 + iOffsetX, TERMY - 5 ) );

        header_windows.clear();
        header_windows.push_back( w_header1 );
        header_windows.push_back( w_header2 );

        // Specify where the popup's string would be printed
        filter_pos = point( 2, TERMY - 8 );
        filter_view_len = iMinScreenWidth / 2 - 11;
        if( fpopup ) {
            point inner_pos = filter_pos + point( 2, 0 );
            fpopup->window( win, inner_pos, inner_pos.x + filter_view_len );
        }

        ui.position_from_window( win );
    };
    init_windows( ui );
    ui.on_screen_resize( init_windows );

    std::vector<std::string> headers;
    headers.emplace_back( _( "Mod List" ) );
    headers.emplace_back( _( "Mod Load Order" ) );

    size_t active_header = 0;
    int startsel[2] = {0, 0};
    size_t cursel[2] = {0, 0};
    size_t iCurrentTab = 0;

    struct mod_tab {
        std::string id;
        std::vector<mod_id> mods;
        std::vector<mod_id> mods_unfiltered;
    };
    std::vector<mod_tab> all_tabs;

    for( const std::pair<std::string, std::string> &tab : get_mod_list_tabs() ) {
        all_tabs.push_back( {
            tab.first,
            std::vector<mod_id>(),
            std::vector<mod_id>()
        } );
    }

    const std::map<std::string, std::string> &cat_tab_map = get_mod_list_cat_tab();
    for( const mod_id &mod : mman->get_all_sorted() ) {
        int cat_idx = mod->category.first;
        const std::string &cat_id = get_mod_list_categories()[cat_idx].first;

        std::string dest_tab = "tab_default";
        const auto iter = cat_tab_map.find( cat_id );
        if( iter != cat_tab_map.end() ) {
            dest_tab = iter->second;
        }

        for( mod_tab &tab : all_tabs ) {
            if( tab.id == dest_tab ) {
                tab.mods_unfiltered.push_back( mod );
                break;
            }
        }
    }

    // Helper function for determining the currently selected mod
    const auto get_selected_mod = [&]() -> const MOD_INFORMATION* {
        if( active_header == 0 )
        {
            const std::vector<mod_id> &current_tab_mods = all_tabs[iCurrentTab].mods;
            if( current_tab_mods.empty() ) {
                return nullptr;
            } else {
                return &current_tab_mods[cursel[0]].obj();
            }
        } else if( active_header == 1 )
        {
            if( active_mod_order.empty() ) {
                return nullptr;
            } else {
                return &active_mod_order[cursel[1]].obj();
            }
        }
        return nullptr;
    };

    const auto recalc_visible = [&]( const std::string & filter_str, bool show_obsolete ) {
        const MOD_INFORMATION *selected_mod = nullptr;
        if( active_header == 0 && all_tabs[iCurrentTab].mods.size() > cursel[0] ) {
            selected_mod = &*all_tabs[iCurrentTab].mods[cursel[0]];
        }
        for( mod_tab &tab : all_tabs ) {
            tab.mods.reserve( tab.mods_unfiltered.size() );
            tab.mods.clear();
            for( const mod_id &mod : tab.mods_unfiltered ) {
                if( !show_obsolete && mod->obsolete ) {
                    continue;
                }
                auto it = std::ranges::find( active_mod_order, mod );
                if( it != active_mod_order.end() ) {
                    continue;
                }
                if( !filter_str.empty() ) {
                    std::string name = ( *mod ).name();
                    if( !lcmatch( name, filter_str ) ) {
                        continue;
                    }
                }
                tab.mods.push_back( mod );
            }
        }
        startsel[0] = 0;
        cursel[0] = 0;
        // Try to restore cursor position
        const std::vector<mod_id> &curr_tab = all_tabs[iCurrentTab].mods;
        for( size_t i = 0; i < curr_tab.size(); i++ ) {
            if( &*curr_tab[i] == selected_mod ) {
                cursel[0] = i;
                break;
            }
        }
    };
    recalc_visible( current_filter, show_obsolete );

    // Helper function for applying filter to mod tabs
    const auto apply_filter = [&]( const std::string & filter_str ) {
        if( filter_str == current_filter ) {
            return;
        }
        recalc_visible( filter_str, show_obsolete );
        current_filter = filter_str;
    };

    // Helper function for toggling display of obsolete mods
    const auto set_show_obsolete = [&]( bool value ) {
        if( show_obsolete == value ) {
            return;
        }
        recalc_visible( current_filter, value );
        show_obsolete = value;
    };

    // Helper function for recalculating visible entries after adding mods.
    // Also tries to keep cursor position, though it's rather imprecise
    // when multiple mods are added simultaneously.
    const auto recalc_after_add_remove = [&]() {
        size_t sel = cursel[0];
        recalc_visible( current_filter, show_obsolete );
        if( active_header == 0 ) {
            const std::vector<mod_id> &current_tab_mods = all_tabs[iCurrentTab].mods;
            if( active_header == 0 && !current_tab_mods.empty() ) {
                cursel[0] = std::min( current_tab_mods.size() - 1, sel );
            }
        } else {
            cursel[0] = 0;
        }
    };

    ui.on_redraw( [&]( const ui_adaptor & ) {
        if( standalone ) {
            draw_empty_worldgen_tabs( win );
        } else {
            draw_worldgen_tabs( win, 0 );
        }
        draw_modselection_borders( win, ctxt, standalone );

        // Redraw headers
        for( size_t i = 0; i < headers.size(); ++i ) {
            werase( header_windows[i] );
            const int header_x = ( getmaxx( header_windows[i] ) - utf8_width( headers[i] ) ) / 2;
            mvwprintz( header_windows[i], point( header_x, 0 ), c_cyan, headers[i] );

            if( active_header == i ) {
                mvwputch( header_windows[i], point( header_x - 3, 0 ), c_red, '<' );
                mvwputch( header_windows[i], point( header_x + utf8_width( headers[i] ) + 2, 0 ),
                          c_red, '>' );
            }
            wnoutrefresh( header_windows[i] );
        }

        // Redraw description
        werase( w_description );

        if( const MOD_INFORMATION *selmod = get_selected_mod() ) {
            // NOLINTNEXTLINE(cata-use-named-point-constants)
            int num_lines = fold_and_print( w_description, point( 1, 0 ),
                                            getmaxx( w_description ) - 1,
                                            c_white, mman_ui->get_information( selmod ) );
            auto window_height = catacurses::getmaxy( w_description );
            auto window_width = catacurses::getmaxx( w_description );
            if( num_lines > window_height ) {
                // The description didn't fit in the window, so provide a
                // hint for how to see the whole thing
                std::string message = string_format( _( "…%s = View full description " ),
                                                     ctxt.get_desc( "VIEW_MOD_DESCRIPTION" ) );
                nc_color color = c_green;
                print_colored_text( w_description, point( window_width - utf8_width( message ), window_height - 1 ),
                                    color, color, message );
            }
        }

        // Draw tab names
        wmove( win, point( 2, 4 ) );
        for( size_t i = 0; i < get_mod_list_tabs().size(); i++ ) {
            wprintz( win, c_white, "[" );
            wprintz( win, ( iCurrentTab == i ) ? hilite( c_light_green ) : c_light_green,
                     _( get_mod_list_tabs()[i].second ) );
            wprintz( win, c_white, "]" );
            wputch( win, BORDER_COLOR, LINE_OXOX );
        }

        // Draw filter
        if( fpopup ) {
            mvwprintz( win, filter_pos, c_cyan, "< " );
            mvwprintz( win, filter_pos + point( filter_view_len + 2, 0 ), c_cyan, " >" );
            // This call makes popup draw its string at position specified on popup initialization
            fpopup->query_string( /*loop=*/false, /*draw_only=*/true );
        } else {
            mvwprintz( win, filter_pos, c_light_gray, "< " );
            const char *help = current_filter.empty() ? _( "[%s] Filter" ) : _( "[%s] Filter: " );
            wprintz( win, c_light_gray, help, ctxt.get_desc( "FILTER" ) );
            wprintz( win, c_white, current_filter );
            wprintz( win, c_light_gray, " >" );
        }

        wnoutrefresh( w_description );
        wnoutrefresh( win );

        // Draw selected tab
        const mod_tab &current_tab = all_tabs[iCurrentTab];
        const char *msg = current_tab.mods_unfiltered.empty() ?
                          _( "--NO AVAILABLE MODS--" ) : _( "--NO MATCHES--" );
        draw_mod_list( w_list, startsel[0], cursel[0], current_tab.mods, active_header == 0,
                       msg, catacurses::window() );

        // Draw active mods
        draw_mod_list( w_active, startsel[1], cursel[1], active_mod_order, active_header == 1,
                       _( "--NO ACTIVE MODS--" ), w_shift );
    } );

    const auto set_filter = [&]() {
        fpopup = std::make_unique<string_input_popup>();
        fpopup->max_length( 256 );
        // current_filter is modified by apply_filter(), we have to copy the value
        // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
        const std::string old_filter = current_filter;
        fpopup->text( current_filter );

        ime_sentry sentry;

        // On next redraw, call resize callback which will configure how popup is rendered
        ui.mark_resize();

        for( ;; ) {
            ui_manager::redraw();
            fpopup->query_string( /*loop=*/false );

            if( fpopup->canceled() ) {
                apply_filter( old_filter );
                break;
            } else if( fpopup->confirmed() ) {
                break;
            } else {
                apply_filter( fpopup->text() );
            }
        };

        fpopup.reset();
    };

    int tab_output = 0;
    while( tab_output == 0 ) {
        ui_manager::redraw();

        const int next_header = ( active_header == 1 ) ? 0 : 1;
        const int prev_header = ( active_header == 0 ) ? 1 : 0;

        size_t selection = ( active_header == 0 ) ? cursel[0] : cursel[1];
        size_t last_selection = selection;
        size_t next_selection = selection + 1;
        size_t prev_selection = selection - 1;
        if( active_header == 0 ) {
            size_t num_mods = all_tabs[iCurrentTab].mods.size();
            next_selection = ( next_selection >= num_mods ) ? 0 : next_selection;
            prev_selection = ( prev_selection > num_mods ) ? num_mods - 1 : prev_selection;
        } else {
            next_selection = ( next_selection >= active_mod_order.size() ) ? 0 : next_selection;
            prev_selection = ( prev_selection > active_mod_order.size() ) ? active_mod_order.size() - 1 :
                             prev_selection;
        }

        const std::string action = ctxt.handle_input();

        if( action == "DOWN" ) {
            selection = next_selection;
        } else if( action == "UP" ) {
            selection = prev_selection;
        } else if( action == "RIGHT" ) {
            active_header = next_header;
        } else if( action == "LEFT" ) {
            active_header = prev_header;
        } else if( action == "CONFIRM" ) {
            const std::vector<mod_id> &current_tab_mods = all_tabs[iCurrentTab].mods;
            if( active_header == 0 && !current_tab_mods.empty() ) {
                // try-add
                const mod_id &to_add = current_tab_mods[cursel[0]];
                ret_val<bool> ret = mman_ui->try_add( to_add, active_mod_order );
                if( !ret.success() ) {
                    std::string msg = string_format( _( "Cannot add mod %s [%s].\n\n%s" ),
                                                     to_add->name(), to_add, ret.str() );
                    popup( msg );
                }
            } else if( active_header == 1 && !active_mod_order.empty() ) {
                // try-rem
                mman_ui->try_rem( cursel[1], active_mod_order );
                if( active_mod_order.empty() ) {
                    // switch back to other list, we can't change
                    // anything in the empty active mods list.
                    active_header = 0;
                }
            }
            recalc_after_add_remove();
        } else if( action == "ADD_MOD" ) {
            if( active_header == 1 && active_mod_order.size() > 1 ) {
                mman_ui->try_shift( '+', cursel[1], active_mod_order );
            }
        } else if( action == "REMOVE_MOD" ) {
            if( active_header == 1 && active_mod_order.size() > 1 ) {
                mman_ui->try_shift( '-', cursel[1], active_mod_order );
            }
        } else if( action == "TOGGLE_SHOW_OBSOLETE" ) {
            set_show_obsolete( !show_obsolete );
        } else if( action == "NEXT_CATEGORY_TAB" ) {
            if( active_header == 0 ) {
                if( ++iCurrentTab >= get_mod_list_tabs().size() ) {
                    iCurrentTab = 0;
                }

                startsel[0] = 0;
                cursel[0] = 0;
            }

        } else if( action == "PREV_CATEGORY_TAB" ) {
            if( active_header == 0 ) {
                if( --iCurrentTab > get_mod_list_tabs().size() ) {
                    iCurrentTab = get_mod_list_tabs().size() - 1;
                }

                startsel[0] = 0;
                cursel[0] = 0;
            }
        } else if( action == "NEXT_TAB" ) {
            tab_output = 1;
        } else if( action == "PREV_TAB" && ( !on_backtab || on_backtab() ) ) {
            tab_output = -1;
        } else if( action == "SAVE_DEFAULT_MODS" ) {
            if( query_yn( _( "Save list of active mods as default mod list?" ) ) ) {
                if( mman->set_default_mods( active_mod_order ) ) {
                    popup( _( "Saved successfully!" ) );
                } else {
                    popup( _( "Failed to save!  Debug log might contain more details." ) );
                }
            }
        } else if( action == "VIEW_MOD_DESCRIPTION" ) {
            if( const MOD_INFORMATION *selmod = get_selected_mod() ) {
                popup( "%s", mman_ui->get_information( selmod ) );
            }
        } else if( action == "QUIT" && ( !on_quit || on_quit() ) ) {
            tab_output = -999;
        } else if( action == "FILTER" ) {
            set_filter();
        }
        // RESOLVE INPUTS
        if( last_selection != selection ) {
            if( active_header == 0 ) {
                cursel[0] = selection;
            } else {
                cursel[1] = selection;
            }
        }
        if( active_mod_order.empty() ) {
            cursel[1] = 0;
        }

        if( active_header == 1 ) {
            if( active_mod_order.empty() ) {
                cursel[1] = 0;
            } else {
                // If it goes below 0, it'll loop back to max (or at least, greater than AMO size*10.
                if( cursel[1] > active_mod_order.size() * 10 ) {
                    cursel[1] = 0;
                }
                // If it goes above AMO.size(), cap to size.
                else if( cursel[1] >= active_mod_order.size() ) {
                    cursel[1] = active_mod_order.size() - 1;
                }
            }
        }
        // end RESOLVE INPUTS
    }
    return tab_output;
}

int worldfactory::show_worldgen_tab_confirm( const catacurses::window &win, WORLDINFO *world,
        const std::function<bool()> &on_quit )
{
    catacurses::window w_confirmation;

    ui_adaptor ui;

    string_input_popup spopup;
    spopup.max_length( max_worldname_len );

    const point namebar_pos( 3 + utf8_width( _( "World Name:" ) ), 1 );

    input_context ctxt( "WORLDGEN_CONFIRM_DIALOG" );
    // dialog actions
    ctxt.register_action( "QUIT" );
    ctxt.register_action( "NEXT_TAB" );
    ctxt.register_action( "PREV_TAB" );
    ctxt.register_action( "PICK_RANDOM_WORLDNAME" );
    ctxt.register_action( "TOGGLE_V2_SAVE_FORMAT" );
    // string input popup actions
    ctxt.register_action( "TEXT.LEFT" );
    ctxt.register_action( "TEXT.RIGHT" );
    ctxt.register_action( "TEXT.CLEAR" );
    ctxt.register_action( "TEXT.BACKSPACE" );
    ctxt.register_action( "TEXT.HOME" );
    ctxt.register_action( "TEXT.END" );
    ctxt.register_action( "TEXT.DELETE" );
#if defined( TILES )
    ctxt.register_action( "TEXT.PASTE" );
#endif
    ctxt.register_action( "TEXT.INPUT_FROM_FILE" );
    ctxt.register_action( "HELP_KEYBINDINGS" );
    ctxt.register_action( "ANY_INPUT" );

    const auto init_windows = [&]( ui_adaptor & ui ) {
        const int iTooltipHeight = 1;
        const int iContentHeight = TERMY - 3 - iTooltipHeight;
        const int iMinScreenWidth = std::max( FULL_SCREEN_WIDTH, TERMX / 2 );
        const int iOffsetX = TERMX > FULL_SCREEN_WIDTH ? ( TERMX - iMinScreenWidth ) / 2 : 0;

        w_confirmation = catacurses::newwin( iContentHeight, iMinScreenWidth - 2,
                                             point( 1 + iOffsetX, iTooltipHeight + 2 ) );

        // +1 for end-of-text cursor
        spopup.window( w_confirmation, namebar_pos, namebar_pos.x + max_worldname_len + 1 )
        .context( ctxt );

        ui.position_from_window( win );
    };
    init_windows( ui );
    ui.on_screen_resize( init_windows );

    bool noname = false;

    std::string worldname = world->world_name;

    // do not switch IME mode now, but restore previous mode on return
    ime_sentry sentry( ime_sentry::keep );

    ui.on_redraw( [&]( const ui_adaptor & ) {
        draw_worldgen_tabs( win, 2 );
        wnoutrefresh( win );

        werase( w_confirmation );
        mvwprintz( w_confirmation, point( 2, namebar_pos.y ), c_white, _( "World Name:" ) );
        fold_and_print( w_confirmation, point( 2, 3 ), getmaxx( w_confirmation ) - 2, c_light_gray,
                        _( "Press [<color_yellow>%s</color>] to pick a random name for your world." ),
                        ctxt.get_desc( "PICK_RANDOM_WORLDNAME" ) );

        if( world->world_save_format == save_format::V2_COMPRESSED_SQLITE3 ) {
            mvwprintz( w_confirmation, point( 2, 6 ), c_white,
                       _( "Save Format: V2 (Current)" ) );
        } else {
            mvwprintz( w_confirmation, point( 2, 6 ), c_light_gray,
                       _( "Save Format: V1 (Legacy)" ) );
        }

        fold_and_print( w_confirmation, point( 2, 8 ), getmaxx( w_confirmation ) - 2, c_light_gray,
                        _( "Press [<color_yellow>%s</color>] to toggle save format.\n"
                           "<color_light_blue>V2 format shrinks save files and reduces save corruption. "
                           "V1 is the legacy format. You can convert existing V1 worlds to V2 from the main menu. "
                           "V2 worlds cannot currently be converted back to V1.</color>" ),
                        ctxt.get_desc( "TOGGLE_V2_SAVE_FORMAT" ) );

        fold_and_print( w_confirmation, point( 2, TERMY / 2 - 2 ), getmaxx( w_confirmation ) - 2,
                        c_light_gray,
                        _( "Press [<color_yellow>%s</color>] when you are satisfied with the world as it is and are ready "
                           "to continue, or [<color_yellow>%s</color>] to go back and review your world." ),
                        ctxt.get_desc( "NEXT_TAB" ), ctxt.get_desc( "PREV_TAB" ) );
        if( noname ) {
            mvwprintz( w_confirmation, namebar_pos, h_light_gray,
                       _( "________NO NAME ENTERED!________" ) );
            wnoutrefresh( w_confirmation );
        } else {
            // spopup.query_string() will call wnoutrefresh( w_confirmation ), and should
            // be called last to position the cursor at the correct place in the curses build.
            spopup.text( worldname );
            spopup.query_string( false, true );
        }
    } );

    do {
        ui_manager::redraw();

        worldname = spopup.query_string( false, false, true );
        const std::string action = ctxt.input_to_action( ctxt.get_raw_input() );
        if( action == "NEXT_TAB" ) {
            if( worldname.empty() ) {
                noname = true;
                ui_manager::redraw();
                if( !query_yn( _( "Are you SURE you're finished?  World name will be randomly generated." ) ) ) {
                    noname = false;
                    continue;
                } else {
                    noname = false;
                    world->world_name = pick_random_name();
                    if( !valid_worldname( world->world_name ) ) {
                        continue;
                    }
                    return 1;
                }
            } else if( valid_worldname( worldname ) && query_yn( _( "Are you SURE you're finished?" ) ) ) {
                world->world_name = worldname;
                return 1;
            } else {
                continue;
            }
        } else if( action == "PREV_TAB" ) {
            world->world_name = worldname;
            return -1;
        } else if( action == "PICK_RANDOM_WORLDNAME" ) {
            world->world_name = worldname = pick_random_name();
        } else if( action == "TOGGLE_V2_SAVE_FORMAT" ) {
            if( world->world_save_format == save_format::V2_COMPRESSED_SQLITE3 ) {
                world->world_save_format = save_format::V1;
            } else {
                world->world_save_format = save_format::V2_COMPRESSED_SQLITE3;
            }
        } else if( action == "QUIT" && ( !on_quit || on_quit() ) ) {
            world->world_name = worldname;
            return -999;
        }
    } while( true );

    return 0;
}

void worldfactory::draw_modselection_borders( const catacurses::window &win,
        const input_context &ctxtp, bool standalone )
{

    const int iMinScreenWidth = std::max( FULL_SCREEN_WIDTH, TERMX / 2 );

    // make appropriate lines: X & Y coordinate of starting point, length, horizontal/vertical type
    std::array<int, 5> xs = {{1, 1, iMinScreenWidth / 2 + 2, iMinScreenWidth / 2 - 4, iMinScreenWidth / 2 + 2}};
    std::array<int, 5> ys = {{TERMY - 8, 4, 4, 3, 3}};
    std::array<int, 5> ls = {{iMinScreenWidth - 2, iMinScreenWidth / 2 - 4, iMinScreenWidth / 2 - 2, TERMY - 11, 1}};
    std::array<bool, 5> hv = {{true, true, true, false, false}}; // horizontal line = true, vertical line = false

    for( int i = 0; i < 5; ++i ) {
        point p( xs[i], ys[i] );
        int l = ls[i];
        if( hv[i] ) {
            for( int j = 0; j < l; ++j ) {
                mvwputch( win, p + point( j, 0 ), BORDER_COLOR, LINE_OXOX ); // -
            }
        } else {
            for( int j = 0; j < l; ++j ) {
                mvwputch( win, p + point( 0, j ), BORDER_COLOR, LINE_XOXO ); // |
            }
        }
    }

    // Add in connective characters
    mvwputch( win, point( 0, 4 ), BORDER_COLOR, LINE_XXXO ); // |-
    mvwputch( win, point( 0, TERMY - 8 ), BORDER_COLOR, LINE_XXXO ); // |-
    mvwputch( win, point( iMinScreenWidth / 2 + 2, 4 ), BORDER_COLOR, LINE_XXXO ); // |-

    mvwputch( win, point( iMinScreenWidth - 1, 4 ), BORDER_COLOR, LINE_XOXX ); // -|
    mvwputch( win, point( iMinScreenWidth - 1, TERMY - 8 ), BORDER_COLOR, LINE_XOXX ); // -|
    mvwputch( win, point( iMinScreenWidth / 2 - 4, 4 ), BORDER_COLOR, LINE_XOXX ); // -|

    mvwputch( win, point( iMinScreenWidth / 2 - 4, 2 ), BORDER_COLOR, LINE_OXXX ); // -.-
    mvwputch( win, point( iMinScreenWidth / 2 + 2, 2 ), BORDER_COLOR, LINE_OXXX ); // -.-

    mvwputch( win, point( iMinScreenWidth / 2 - 4, TERMY - 8 ), BORDER_COLOR,
              LINE_XXOX ); // _|_
    mvwputch( win, point( iMinScreenWidth / 2 + 2, TERMY - 8 ), BORDER_COLOR,
              LINE_XXOX ); // _|_

    // Add tips & hints
    std::vector<std::string> strings;
    strings.push_back( string_format(
                           _( "[<color_yellow>%s</color>] = save <color_cyan>Mod Load Order</color> as default" ),
                           ctxtp.get_desc( "SAVE_DEFAULT_MODS" ) ) );
    if( !standalone ) {
        strings.push_back( string_format(
                               _( "[<color_yellow>%s</color>/<color_yellow>%s</color>] = switch Main-Tab" ),
                               ctxtp.get_desc( "PREV_TAB" ),
                               ctxtp.get_desc( "NEXT_TAB" ) ) );
    }
    strings.push_back( string_format(
                           _( "[<color_yellow>%s</color>/<color_yellow>%s</color>] = switch "
                              "<color_cyan>Mod List</color> and <color_cyan>Mod Load Order</color>" ),
                           ctxtp.get_desc( "LEFT" ),
                           ctxtp.get_desc( "RIGHT" ) ) );
    strings.push_back( string_format(
                           _( "[<color_yellow>%s</color>/<color_yellow>%s</color>] = switch <color_cyan>Mod List</color> Tab" ),
                           ctxtp.get_desc( "PREV_CATEGORY_TAB" ),
                           ctxtp.get_desc( "NEXT_CATEGORY_TAB" ) ) );
    strings.push_back( string_format(
                           _( "[<color_yellow>%s</color>] = keybindings" ),
                           ctxtp.get_desc( "HELP_KEYBINDINGS" ) ) );
    std::string msg = join( strings, "  " );

    fold_and_print( win, point( 2, TERMY - 7 ), getmaxx( win ) - 4, c_light_gray, msg );
    wnoutrefresh( win );
}

void worldfactory::draw_worldgen_tabs( const catacurses::window &w, size_t current )
{
    werase( w );

    static const std::vector<std::string> tab_strings = { {
            translate_marker( "World Mods" ),
            translate_marker( "World Options" ),
            translate_marker( "Finalize World" )
        }
    };

    std::vector<std::string> tab_strings_translated( tab_strings );
    std::ranges::for_each( tab_strings_translated,
                           []( std::string & str )->void { str = _( str ); } );

    draw_tabs( w, tab_strings_translated, current );
    draw_border_below_tabs( w );
}

void worldfactory::draw_empty_worldgen_tabs( const catacurses::window &w )
{
    draw_tabs( w, std::vector<std::string>(), 0 );
    draw_border_below_tabs( w );
}

bool worldfactory::valid_worldname( const std::string &name, bool automated )
{
    std::string msg;

    if( name.empty() ) {
        msg = _( "World name cannot be empty!" );
    } else if( name == "save" || name == "TUTORIAL" || name == "DEFENSE" ) {
        msg = string_format( _( "%s is a reserved name!" ), name );
    } else if( has_world( name ) ) {
        msg = string_format( _( "A world named %s already exists!" ), name );
    } else if( name.front() == ' ' || name.back() == ' ' ) {
        msg = string_format( _( "A world name cannot start or end with spaces!" ) );
    } else {
        return true;
    }
    if( !automated ) {
        popup( msg, PF_GET_KEY );
    }
    return false;
}


mod_manager &worldfactory::get_mod_manager()
{
    return *mman;
}

WORLDINFO *worldfactory::get_world( const std::string &name )
{
    const auto iter = all_worlds.find( name );
    if( iter == all_worlds.end() ) {
        debugmsg( "Requested non-existing world %s, prepare for crash", name );
        return nullptr;
    }
    return iter->second.get();
}

size_t worldfactory::get_world_index( const std::string &name )
{
    std::vector<std::string> worlds = all_worldnames();
    size_t world_pos = std::ranges::find( worlds,
                                          name ) - worlds.begin();
    if( world_pos >= worlds.size() ) {
        world_pos = 0;
    }
    return world_pos;
}

// Helper predicate to exclude files from deletion when resetting a world directory.
static bool isForbidden( const std::string &candidate )
{
    return candidate.find( PATH_INFO::worldoptions() ) != std::string::npos ||
           candidate.find( "mods.json" ) != std::string::npos;
}

void worldfactory::delete_world( const std::string &worldname, const bool delete_folder )
{
    // Disconnect to the database if we're somehow trying to delete the currently active world
    if( active_world && active_world->info->world_name == worldname ) {
        set_active_world( nullptr );
    }

    std::string worldpath = get_world( worldname )->folder_path();
    std::set<std::string> directory_paths;

    auto file_paths = get_files_from_path( "", worldpath, true, true );
    if( !delete_folder ) {
        std::vector<std::string>::iterator forbidden = std::ranges::find_if( file_paths,
                isForbidden );
        while( forbidden != file_paths.end() ) {
            file_paths.erase( forbidden );
            forbidden = std::ranges::find_if( file_paths, isForbidden );
        }
    }
    for( auto &file_path : file_paths ) {
        // strip to path and remove worldpath from it
        std::string part = file_path.substr( worldpath.size(),
                                             file_path.find_last_of( "/\\" ) - worldpath.size() );
        size_t last_separator = part.find_last_of( "/\\" );
        while( last_separator != std::string::npos && part.size() > 1 ) {
            directory_paths.insert( part );
            part = part.substr( 0, last_separator );
            last_separator = part.find_last_of( "/\\" );
        }
    }

    for( auto &file : file_paths ) {
        remove_file( file );
    }
    // Trying to remove a non-empty parent directory before a child
    // directory will fail.  Removing directories in reverse order
    // will prevent this situation from arising.
    for( auto it = directory_paths.rbegin(); it != directory_paths.rend(); ++it ) {
        remove_directory( worldpath + *it );
    }
    if( delete_folder ) {
        remove_directory( worldpath );
        remove_world( worldname );
    } else {
        get_world( worldname )->world_saves.clear();
    }
}

void worldfactory::convert_to_v2( const std::string &worldname )
{
    // Ensure we're ready to convert
    WORLDINFO *worldinfo = get_world( worldname );
    if( worldinfo == nullptr ) {
        popup( _( "Tried to convert non-existing world %s to v2" ), worldname );
        return;
    }

    if( worldinfo->world_save_format != save_format::V1 ) {
        popup( _( "World %s is already at savefile version 2" ), worldname );
        return;
    }

    // Backup the world by renaming it to a new name
    if( worldname.find( " (V2 Conversion Backup)" ) != std::string::npos ) {
        popup( _( "World '%s' is already a backup. Rename the world before trying again." ), worldname );
        return;
    }

    std::string backup_name = worldname + " (V2 Conversion Backup)";
    if( has_world( backup_name ) ) {
        popup( _( "Backup world '%s' already exists, aborting conversion" ), backup_name );
        return;
    }

    std::unique_ptr<WORLDINFO> old_world = std::make_unique<WORLDINFO>();
    old_world->COPY_WORLD( world_generator->get_world( worldname ) );
    old_world->world_name = backup_name;

    // Deep copy the world saves
    std::vector<save_t> world_saves_copy( worldinfo->world_saves );
    old_world->world_saves = worldinfo->world_saves;

    // Rename the world folder perform the move
    rename_file( worldinfo->folder_path(), old_world->folder_path() );
    worldinfo->world_save_format = save_format::V2_COMPRESSED_SQLITE3;
    world new_world( worldinfo );
    new_world.convert_from_v1( old_world );
    add_world( std::move( old_world ) );

    // Save the world
    worldinfo->save();

    popup( _( "Conversion Complete!" ) );
}
