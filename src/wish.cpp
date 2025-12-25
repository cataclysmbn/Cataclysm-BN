#include "debug_menu.h" // IWYU pragma: associated

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "bionics.h"
#include "calendar.h"
#include "catacharset.h"
#include "color.h"
#include "cursesdef.h"
#include "debug.h"
#include "flag.h"
#include "flat_set.h"
#include "game.h"
#include "input.h"
#include "item.h"
#include "item_factory.h"
#include "itype.h"
#include "map.h"
#include "monster.h"
#include "monstergenerator.h"
#include "mtype.h"
#include "mutation.h"
#include "output.h"
#include "player.h"
#include "point.h"
#include "skill.h"
#include "string_formatter.h"
#include "string_input_popup.h"
#include "translations.h"
#include "type_id.h"
#include "ui.h"
#include "ui_manager.h"
#include "uistate.h"
#include "units.h"
#include "units_energy.h"

class wish_mutate_callback: public uilist_callback
{
    public:
        // Last menu entry
        int lastlen = 0;
        // Feedback message
        std::string msg;
        bool started = false;
        std::vector<trait_id> vTraits;
        std::map<trait_id, bool> pTraits;
        // Traits by mutation category
        std::unordered_map<mutation_category_id, std::set<mutation_branch>> category_mutations;
        Character *p;

        nc_color mcolor( const trait_id &m ) {
            if( pTraits[ m ] ) {
                return c_green;
            }
            return c_light_gray;
        }

        wish_mutate_callback() = default;
        bool key( const input_context &, const input_event &event, int entnum, uilist *menu ) override {
            const int input = event.get_first_input();
            if( input == 't' && p->has_trait( vTraits[ entnum ] ) ) {
                if( p->has_base_trait( vTraits[ entnum ] ) ) {
                    p->toggle_trait( vTraits[ entnum ] );
                    p->unset_mutation( vTraits[ entnum ] );

                } else {
                    p->set_mutation( vTraits[ entnum ] );
                    p->toggle_trait( vTraits[ entnum ] );
                }
                uilist_entry &entry = menu->entries[entnum];
                if( p->has_trait( vTraits[ entnum ] ) ) {
                    entry.text_color = c_green;
                    entry.override_hilite_color = true;
                } else {
                    entry.text_color = menu->text_color;
                    entry.override_hilite_color = false;
                }
                entry.extratxt.txt = p->has_base_trait( vTraits[ entnum ] ) ? "T" : "";
                return true;
            }
            if( input == 'c' || input == 'C' ) {
                // Building menu with mutation category entries (first entry - 'ALL' for cancell purposes)
                uilist category_menu;
                // We'll keep vector of values to map it later from user input
                std::vector<std::pair<const string_id<mutation_category_trait>, std::set<mutation_branch>>*>
                        entries;
                int c = 0;
                auto ch = '0';
                category_menu.addentry( c, true, ch, "ALL" );
                for( auto &it : category_mutations ) {
                    c++;
                    ch++;
                    category_menu.addentry( c, true, ch, it.first.str() );
                    entries.push_back( &it );
                }
                // Waiting for user input
                category_menu.query();
                int ret = category_menu.ret;
                if( ret < 0 ) {
                    return true;
                }
                if( ret == 0 ) {
                    // If 'ALL' chosen - clearing filter
                    menu->clear_filter();
                    return true;
                }
                ret -= 1;
                // Extracting selected option & filtering traits on category presence
                const auto entry = entries[ret];
                auto predicate = [&]( const int idx ) {
                    return entry->second.contains( *vTraits[idx] );
                };
                menu->filterpredicate( predicate );
                return true;
            }
            return false;
        }

        void refresh( uilist *menu ) override {
            // If it is our first time in menu - collect some data to map with uilist entry later
            if( !started ) {
                started = true;
                for( auto &traits_iter : mutation_branch::get_all() ) {
                    vTraits.push_back( traits_iter.id );
                    pTraits[traits_iter.id] = p->has_trait( traits_iter.id );
                    for( auto &category : traits_iter.category ) {
                        if( !category_mutations.contains( category ) ) {
                            category_mutations[category] = std::set<mutation_branch>();
                        }
                        category_mutations[category].insert( traits_iter );
                    }
                }
            }

            const std::string padding = std::string( menu->pad_right - 1, ' ' );

            const int startx = menu->w_width - menu->pad_right;
            for( int i = 2; i < lastlen; i++ ) {
                mvwprintw( menu->window, point( startx, i ), padding );
            }

            int line2 = 4;

            if( menu->selected >= 0 && static_cast<size_t>( menu->selected ) < vTraits.size() ) {
                const mutation_branch &mdata = vTraits[menu->selected].obj();

                mvwprintw( menu->window, point( startx, 3 ),
                           mdata.valid ? _( "Valid" ) : _( "Nonvalid" ) );

                line2++;
                mvwprintz(
                    menu->window,
                    point( startx, line2 ),
                    c_light_gray,
                    _( "Id:" )
                );
                mvwprintw(
                    menu->window,
                    point( startx + 11, line2 ),
                    mdata.id.str()
                );

                if( !mdata.prereqs.empty() ) {
                    line2++;
                    mvwprintz( menu->window, point( startx, line2 ), c_light_gray, _( "Prereqs:" ) );
                    for( const trait_id &j : mdata.prereqs ) {
                        mvwprintz( menu->window, point( startx + 11, line2 ), mcolor( j ),
                                   mutation_branch::get_name( j ) );
                        line2++;
                    }
                }

                if( !mdata.prereqs2.empty() ) {
                    line2++;
                    mvwprintz( menu->window, point( startx, line2 ), c_light_gray, _( "Prereqs, 2d:" ) );
                    for( const trait_id &j : mdata.prereqs2 ) {
                        mvwprintz( menu->window, point( startx + 15, line2 ), mcolor( j ),
                                   mutation_branch::get_name( j ) );
                        line2++;
                    }
                }

                if( mdata.threshold_tier != 0 ) {
                    line2++;
                    mvwprintz( menu->window, point( startx, line2 ), c_light_gray, _( "Threshold tier: %d" ),
                               mdata.threshold_tier );
                }

                if( !mdata.cancels.empty() ) {
                    line2++;
                    mvwprintz( menu->window, point( startx, line2 ), c_light_gray, _( "Cancels:" ) );
                    for( const trait_id &j : mdata.cancels ) {
                        mvwprintz( menu->window, point( startx + 11, line2 ), mcolor( j ),
                                   mutation_branch::get_name( j ) );
                        line2++;
                    }
                }

                if( !mdata.replacements.empty() ) {
                    line2++;
                    mvwprintz( menu->window, point( startx, line2 ), c_light_gray, _( "Becomes:" ) );
                    for( const trait_id &j : mdata.replacements ) {
                        mvwprintz( menu->window, point( startx + 11, line2 ), mcolor( j ),
                                   mutation_branch::get_name( j ) );
                        line2++;
                    }
                }

                if( !mdata.additions.empty() ) {
                    line2++;
                    mvwprintz( menu->window, point( startx, line2 ), c_light_gray, _( "Add-ons:" ) );
                    for( auto &j : mdata.additions ) {
                        mvwprintz( menu->window, point( startx + 11, line2 ), mcolor( j ),
                                   mutation_branch::get_name( j ) );
                        line2++;
                    }
                }

                if( !mdata.types.empty() ) {
                    line2++;
                    mvwprintz( menu->window, point( startx, line2 ), c_light_gray,  _( "Type:" ) );
                    for( auto &j : mdata.types ) {
                        mvwprintw( menu->window, point( startx + 11, line2 ), j );
                        line2++;
                    }
                }

                if( !mdata.category.empty() ) {
                    line2++;
                    mvwprintz( menu->window, point( startx, line2 ), c_light_gray,  _( "Category:" ) );
                    for( const mutation_category_id &j : mdata.category ) {
                        mvwprintw( menu->window, point( startx + 11, line2 ), j.str() );
                        line2++;
                    }
                }
                line2 += 2;

                //~ pts: points, vis: visibility, ugly: ugliness
                mvwprintz( menu->window, point( startx, line2 ), c_light_gray, _( "pts: %d vis: %d ugly: %d" ),
                           mdata.points,
                           mdata.visibility,
                           mdata.ugliness
                         );
                line2 += 2;

                std::vector<std::string> desc = foldstring( mdata.desc(),
                                                menu->pad_right - 1 );
                for( auto &elem : desc ) {
                    mvwprintz( menu->window, point( startx, line2 ), c_light_gray, elem );
                    line2++;
                }
            }

            lastlen = line2 + 1;

            mvwprintz( menu->window, point( startx, menu->w_height - 3 ), c_green, msg );
            msg.clear();
            input_context ctxt( menu->input_category );
            mvwprintw( menu->window, point( startx, menu->w_height - 2 ),
                       _( "[%s] find, [%s] quit, [t] toggle base trait, [c] mutation categories menu" ),
                       ctxt.get_desc( "FILTER" ), ctxt.get_desc( "QUIT" ) );

            wnoutrefresh( menu->window );
        }

        ~wish_mutate_callback() override = default;
};

void debug_menu::wishmutate( Character *who )
{
    uilist wmenu;
    int c = 0;

    for( auto &traits_iter : mutation_branch::get_all() ) {
        wmenu.addentry( -1, true, -2, traits_iter.name() );
        wmenu.entries[ c ].extratxt.left = 1;
        wmenu.entries[ c ].extratxt.txt.clear();
        wmenu.entries[ c ].extratxt.color = c_light_green;
        wmenu.entries[ c ].hilite_color = h_light_green;
        if( who->has_trait( traits_iter.id ) ) {
            wmenu.entries[ c ].text_color = c_green;
            wmenu.entries[ c ].override_hilite_color = true;
            if( who->has_base_trait( traits_iter.id ) ) {
                wmenu.entries[ c ].extratxt.txt = "T";
            }
        }
        c++;
    }
    wmenu.w_x_setup = 0;
    wmenu.w_width_setup = []() -> int {
        return TERMX;
    };
    wmenu.pad_right_setup = []() -> int {
        return TERMX - 40;
    };
    wmenu.selected = uistate.wishmutate_selected;
    wish_mutate_callback cb;
    cb.p = who;
    wmenu.callback = &cb;
    do {
        wmenu.query();
        if( wmenu.ret >= 0 ) {
            int rc = 0;
            const trait_id mstr = cb.vTraits[ wmenu.ret ];
            const auto &mdata = mstr.obj();
            const bool threshold = mdata.threshold;
            const bool profession = mdata.profession;
            // Manual override for the threshold-gaining
            if( threshold || profession ) {
                if( who->has_trait( mstr ) ) {
                    do {
                        who->remove_mutation( mstr );
                        rc++;
                    } while( who->has_trait( mstr ) && rc < 10 );
                } else {
                    do {
                        who->set_mutation( mstr );
                        rc++;
                    } while( !who->has_trait( mstr ) && rc < 10 );
                }
            } else if( who->has_trait( mstr ) ) {
                do {
                    who->remove_mutation( mstr );
                    rc++;
                } while( who->has_trait( mstr ) && rc < 10 );
            } else {
                do {
                    who->mutate_towards( mstr );
                    rc++;
                } while( !who->has_trait( mstr ) && rc < 10 );
            }
            cb.msg = string_format( _( "%s Mutation changes: %d" ), mstr.c_str(), rc );
            uistate.wishmutate_selected = wmenu.selected;
            if( rc != 0 ) {
                for( size_t i = 0; i < cb.vTraits.size(); i++ ) {
                    uilist_entry &entry = wmenu.entries[ i ];
                    entry.extratxt.txt.clear();
                    if( who->has_trait( cb.vTraits[ i ] ) ) {
                        entry.text_color = c_green;
                        entry.override_hilite_color = true;
                        cb.pTraits[ cb.vTraits[ i ] ] = true;
                        if( who->has_base_trait( cb.vTraits[ i ] ) ) {
                            entry.extratxt.txt = "T";
                        }
                    } else {
                        entry.text_color = wmenu.text_color;
                        entry.override_hilite_color = false;
                        cb.pTraits[ cb.vTraits[ i ] ] = false;
                    }
                }
            }
        }
    } while( wmenu.ret >= 0 );
}

void debug_menu::wishbionics( Character &c )
{
    std::vector<const itype *> cbm_items = item_controller->find( []( const itype & itm ) -> bool {
        return itm.can_use( "install_bionic" );
    } );
    std::ranges::sort( cbm_items, []( const itype * a, const itype * b ) {
        return localized_compare( a->nname( 1 ), b->nname( 1 ) );
    } );

    while( true ) {
        units::energy power_level = c.get_power_level();
        units::energy power_max = c.get_max_power_level();
        size_t num_installed = c.get_bionic_collection().size();

        bool can_uninstall = num_installed > 0;
        bool can_uninstall_all = can_uninstall || power_max > 0_J;

        uilist smenu;
        smenu.text += string_format(
                          _( "Current power level: %s\nMax power: %s\nBionics installed: %d" ),
                          units::display( power_level ),
                          units::display( power_max ),
                          num_installed
                      );
        smenu.addentry( 0, true, 'i', _( "Install from CBM…" ) );
        smenu.addentry( 1, can_uninstall, 'u', _( "Uninstall…" ) );
        smenu.addentry( 2, can_uninstall_all, 'U', _( "Uninstall all" ) );
        smenu.addentry( 3, true, 'c', _( "Edit power capacity (kJ)" ) );
        smenu.addentry( 4, true, 'C', _( "Edit power capacity (J)" ) );
        smenu.addentry( 5, true, 'p', _( "Edit power level (kJ)" ) );
        smenu.addentry( 6, true, 'P', _( "Edit power level (J)" ) );
        smenu.query();
        switch( smenu.ret ) {
            case 0: {
                uilist scbms;
                for( size_t i = 0; i < cbm_items.size(); i++ ) {
                    bool enabled = !c.has_bionic( cbm_items[i]->bionic->id );
                    scbms.addentry( i, enabled, MENU_AUTOASSIGN, "%s", cbm_items[i]->nname( 1 ) );
                }
                scbms.query();
                if( scbms.ret >= 0 ) {
                    const itype &cbm = *cbm_items[scbms.ret];
                    const bionic_id &bio = cbm.bionic->id;
                    constexpr int difficulty = 0;
                    constexpr int success = 1;
                    constexpr int level = 99;
                    c.perform_install( bio, bio->upgraded_bionic, difficulty, success, level, "DEBUG",
                                       bio->canceled_mutations );
                }
                break;
            }
            case 1: {
                std::vector<bionic_id> bionics = c.get_bionics();
                uilist sbionics;
                for( size_t i = 0; i < bionics.size(); i++ ) {
                    sbionics.addentry( i, true, MENU_AUTOASSIGN, "%s", bionics[i]->name.translated() );
                }
                sbionics.query();
                if( sbionics.ret >= 0 ) {
                    c.remove_bionic( bionics[sbionics.ret] );
                }
                break;
            }
            case 2: {
                c.clear_bionics();
                c.set_power_level( units::from_kilojoule( 0 ) );
                c.set_max_power_level( units::from_kilojoule( 0 ) );
                break;
            }
            case 3: {
                int new_value = 0;
                if( query_int( new_value, _( "Set the value to (in kJ)?  Currently: %s" ),
                               units::display( power_max ) ) ) {
                    c.set_max_power_level( units::from_kilojoule( new_value ) );
                    c.set_power_level( c.get_power_level() );
                }
                break;
            }
            case 4: {
                int new_value = 0;
                if( query_int( new_value, _( "Set the value to (in J)?  Currently: %s" ),
                               units::display( power_max ) ) ) {
                    c.set_max_power_level( units::from_joule( new_value ) );
                    c.set_power_level( c.get_power_level() );
                }
                break;
            }
            case 5: {
                int new_value = 0;
                if( query_int( new_value, _( "Set the value to (in kJ)?  Currently: %s" ),
                               units::display( power_level ) ) ) {
                    c.set_power_level( units::from_kilojoule( new_value ) );
                }
                break;
            }
            case 6: {
                int new_value = 0;
                if( query_int( new_value, _( "Set the value to (in J)?  Currently: %s" ),
                               units::display( power_level ) ) ) {
                    c.set_power_level( units::from_joule( new_value ) );
                }
                break;
            }
            default: {
                return;
            }
        }
    }
}

class wish_monster_callback: public uilist_callback
{
    public:
        // last menu entry
        int lastent;
        // feedback message
        std::string msg;
        // spawn friendly critter?
        bool friendly;
        bool hallucination;
        // Number of monsters to spawn.
        int group;
        // scrap critter for monster::print_info
        std::unique_ptr<monster> tmp;
        const std::vector<const mtype *> &mtypes;

        wish_monster_callback( const std::vector<const mtype *> &mtypes )
            : mtypes( mtypes ) {
            friendly = false;
            hallucination = false;
            group = 0;
            lastent = -2;
        }

        bool key( const input_context &, const input_event &event, int /*entnum*/,
                  uilist * /*menu*/ ) override {
            if( event.get_first_input() == 'f' ) {
                friendly = !friendly;
                // Force tmp monster regen
                lastent = -2;
                // Tell menu we handled keypress
                return true;
            } else if( event.get_first_input() == 'i' ) {
                group++;
                return true;
            } else if( event.get_first_input() == 'h' ) {
                hallucination = !hallucination;
                return true;
            } else if( event.get_first_input() == 'd' && group != 0 ) {
                group--;
                return true;
            }
            return false;
        }

        void refresh( uilist *menu ) override {
            catacurses::window w_info = catacurses::newwin( menu->w_height - 2, menu->pad_right,
                                        point( menu->w_x + menu->w_width - 1 - menu->pad_right, 1 ) );

            const int entnum = menu->selected;
            const bool valid_entnum = entnum >= 0 && static_cast<size_t>( entnum ) < mtypes.size();
            if( entnum != lastent ) {
                lastent = entnum;
                if( valid_entnum ) {
                    tmp = std::make_unique<monster>( mtypes[ entnum ]->id );
                    if( friendly ) {
                        tmp->friendly = -1;
                    }
                } else {
                    tmp = std::make_unique<monster>();
                }
            }

            werase( w_info );
            if( valid_entnum ) {
                tmp->print_info( w_info, 2, 5, 1 );

                std::string header = string_format( "#%d: %s (%d)%s%s", entnum, tmp->type->nname(),
                                                    group, friendly ? _( " (friendly)" ) : "",
                                                    hallucination ? _( " (hallucination)" ) : "" );
                mvwprintz( w_info, point( ( getmaxx( w_info ) - utf8_width( header ) ) / 2, 0 ), c_cyan, header );
            }

            mvwprintz( w_info, point( 0, getmaxy( w_info ) - 3 ), c_green, msg );
            msg.clear();
            input_context ctxt( menu->input_category );
            mvwprintw( w_info, point( 0, getmaxy( w_info ) - 2 ),
                       _( "[%s] find, [f]riendly, [h]allucination, [i]ncrease group, [d]ecrease group, [%s] quit" ),
                       ctxt.get_desc( "FILTER" ), ctxt.get_desc( "QUIT" ) );

            wnoutrefresh( w_info );
        }

        ~wish_monster_callback() override = default;
};

void debug_menu::wishmonster( const std::optional<tripoint> &p )
{
    std::vector<const mtype *> mtypes;

    uilist wmenu;
    wmenu.w_x_setup = 0;
    wmenu.w_width_setup = []() -> int {
        return TERMX;
    };
    wmenu.pad_right_setup = []() -> int {
        return TERMX - 30;
    };
    wmenu.selected = uistate.wishmonster_selected;
    wish_monster_callback cb( mtypes );
    wmenu.callback = &cb;

    int i = 0;
    for( const mtype &montype : MonsterGenerator::generator().get_all_mtypes() ) {
        wmenu.addentry( i, true, 0, montype.nname() );
        wmenu.entries[i].extratxt.txt = montype.sym;
        wmenu.entries[i].extratxt.color = montype.color;
        wmenu.entries[i].extratxt.left = 1;
        ++i;
        mtypes.push_back( &montype );
    }

    do {
        wmenu.query();
        if( wmenu.ret >= 0 ) {
            const mtype_id &mon_type = mtypes[ wmenu.ret ]->id;
            if( std::optional<tripoint> spawn = p ? p : g->look_around( true ) ) {
                int num_spawned = 0;
                for( const tripoint &destination : closest_points_first( *spawn, cb.group ) ) {
                    monster *const mon = g->place_critter_at( mon_type, destination );
                    if( !mon ) {
                        continue;
                    }
                    if( cb.friendly ) {
                        mon->friendly = -1;
                    }
                    if( cb.hallucination ) {
                        mon->hallucination = true;
                    }
                    ++num_spawned;
                }
                input_context ctxt( wmenu.input_category );
                cb.msg = string_format( _( "Spawned %d monsters, choose another or [%s] to quit." ),
                                        num_spawned, ctxt.get_desc( "QUIT" ) );
                if( num_spawned == 0 ) {
                    cb.msg += _( "\nTarget location is not suitable for placing this kind of monster.  Choose a different target or [i]ncrease the groups size." );
                }
                uistate.wishmonster_selected = wmenu.selected;
            }
        }
    } while( wmenu.ret >= 0 );
}

class wish_item_callback: public uilist_callback
{
    public:
        bool incontainer;
        bool has_flag;
        bool spawn_everything;
        std::string msg;
        std::string flag;
        const std::vector<const itype *> &standard_itype_ids;
        wish_item_callback( const std::vector<const itype *> &ids ) :
            incontainer( false ), has_flag( false ), spawn_everything( false ), standard_itype_ids( ids ) {
        }

        void select( uilist *menu ) override {
            if( menu->selected < 0 ) {
                return;
            }
            incontainer = standard_itype_ids[menu->selected]->phase == phase_id::LIQUID;
        }

        bool key( const input_context &, const input_event &event, int /*entnum*/,
                  uilist * /*menu*/ ) override {
            if( event.get_first_input() == 'f' ) {
                incontainer = !incontainer;
                return true;
            }
            if( event.get_first_input() == 'F' ) {
                flag = string_input_popup()
                       .title( _( "Add which flag?  Use UPPERCASE letters without quotes" ) )
                       .query_string();
                if( !flag.empty() ) {
                    has_flag = true;
                }
                return true;
            }
            if( event.get_first_input() == 'E' ) {
                spawn_everything = !spawn_everything;
                return true;
            }
            return false;
        }

        void refresh( uilist *menu ) override {
            const int starty = 3;
            const int startx = menu->w_width - menu->pad_right;
            const std::string padding( menu->pad_right, ' ' );
            for( int y = 2; y < menu->w_height - 1; y++ ) {
                mvwprintw( menu->window, point( startx - 1, y ), padding );
            }
            mvwhline( menu->window, point( startx, 1 ), ' ', menu->pad_right - 1 );
            const int entnum = menu->selected;
            if( entnum >= 0 && static_cast<size_t>( entnum ) < standard_itype_ids.size() ) {
                item &tmp = *item::spawn_temporary( standard_itype_ids[entnum], calendar::turn );
                const std::string header = string_format( "#%d: %s%s%s", entnum,
                                           standard_itype_ids[entnum]->get_id().c_str(),
                                           incontainer ? _( " (contained)" ) : "",
                                           has_flag ? _( " (flagged)" ) : "" );
                mvwprintz( menu->window, point( startx + ( menu->pad_right - 1 - utf8_width( header ) ) / 2, 1 ),
                           c_cyan, header );

                std::vector<iteminfo> info = tmp.info();
                std::string info_string = format_item_info( info, {} );
                fold_and_print( menu->window, point( startx, starty ), menu->pad_right - 1, c_light_gray,
                                info_string );
            }

            if( spawn_everything ) {
                mvwprintz( menu->window, point( startx, menu->w_height - 4 ), c_green,
                           _( "Select any item to spawn everything (ignores filters)." ) );
            }
            mvwprintz( menu->window, point( startx, menu->w_height - 3 ), c_green, msg );
            msg.erase();
            input_context ctxt( menu->input_category );
            mvwprintw( menu->window, point( startx, menu->w_height - 2 ),
                       _( "[%s] find, [f] container, [F] flag, [E] everything, [%s] quit" ),
                       ctxt.get_desc( "FILTER" ), ctxt.get_desc( "QUIT" ) );
            wnoutrefresh( menu->window );
        }
};

void debug_menu::wishitem( Character *who )
{
    wishitem( who, tripoint( -1, -1, -1 ) );
}

void debug_menu::wishitem( Character *who, const tripoint &pos )
{
    if( who == nullptr && pos.x <= 0 ) {
        debugmsg( "game::wishitem(): invalid parameters" );
        return;
    }
    std::vector<std::pair<std::string, const itype *>> opts;
    for( const itype *i : item_controller->all() ) {
        //TODO!: push up
        auto it = item::spawn_temporary( i, calendar::start_of_cataclysm );
        if( it->has_flag( flag_VARSIZE ) ) {
            it->set_flag( flag_FIT );
        }
        opts.emplace_back( it->tname( 1, false ), i );
    }
    std::ranges::sort( opts, localized_compare );
    std::vector<const itype *> itypes;
    std::ranges::transform( opts, std::back_inserter( itypes ),
    []( const auto & pair ) {
        return pair.second;
    } );

    int prev_amount = 1;
    int amount = 1;
    uilist wmenu;
    wmenu.w_x_setup = 0;
    wmenu.w_width_setup = []() -> int {
        return TERMX;
    };
    wmenu.pad_right_setup = []() -> int {
        return std::max( TERMX / 2, TERMX - 50 );
    };
    wmenu.selected = uistate.wishitem_selected;
    wish_item_callback cb( itypes );
    wmenu.callback = &cb;

    for( size_t i = 0; i < opts.size(); i++ ) {
        //TODO!: push up
        item &ity = *item::spawn_temporary( opts[i].second, calendar::start_of_cataclysm );
        wmenu.addentry( i, true, 0, opts[i].first );
        mvwzstr &entry_extra_text = wmenu.entries[i].extratxt;
        entry_extra_text.txt = ity.symbol();
        entry_extra_text.color = ity.color();
        entry_extra_text.left = 1;
    }
    do {
        wmenu.query();
        if( cb.spawn_everything && wmenu.ret != UILIST_CANCEL ) {
            wmenu.ret = opts.size() - 1;
        }
        bool did_amount_prompt = false;
        while( wmenu.ret >= 0 ) {
            detached_ptr<item> granted = item::spawn( opts[wmenu.ret].second );
            if( granted->has_flag( flag_VARSIZE ) ) {
                granted->set_flag( flag_FIT );
            }
            if( cb.incontainer ) {
                granted = item::in_its_container( std::move( granted ) );
            }
            if( cb.has_flag ) {
                granted->item_tags.insert( flag_id( cb.flag ) );
            }
            // If the item has an ammunition, this loads it to capacity, including magazines.
            if( !granted->ammo_default().is_null() ) {
                granted->ammo_set( granted->ammo_default(), -1 );
            }

            granted->set_birthday( calendar::turn );
            prev_amount = amount;
            bool canceled = false;
            if( who != nullptr && !did_amount_prompt ) {
                string_input_popup popup;
                if( cb.spawn_everything ) {
                    popup.title( _( "How many of each?" ) );
                    if( cb.incontainer ) {
                        popup.description( "Spawning items in containers." );
                    } else {
                        popup.description( "Spawning items without containers." );
                    }
                } else {
                    popup
                    .title( _( "How many?" ) )
                    .description( granted->tname() );
                }
                popup.width( 20 )
                .edit( amount );
                canceled = popup.canceled();
            }
            if( !canceled ) {
                did_amount_prompt = true;
                if( who != nullptr ) {
                    if( granted->count_by_charges() ) {
                        if( amount > 0 ) {
                            granted->charges = amount;
                            who->i_add_or_drop( item::spawn( *granted ) );
                        }
                    } else {
                        for( int i = 0; i < amount; i++ ) {
                            who->i_add_or_drop( item::spawn( *granted ) );
                        }
                    }
                    who->invalidate_crafting_inventory();
                } else if( pos.x >= 0 && pos.y >= 0 ) {
                    g->m.add_item_or_charges( pos, item::spawn( *granted ) );
                    wmenu.ret = -1;
                }
                if( amount > 0 ) {
                    input_context ctxt( wmenu.input_category );
                    cb.msg = string_format( _( "Wish granted.  Wish for more or hit [%s] to quit." ),
                                            ctxt.get_desc( "QUIT" ) );
                }
            }
            uistate.wishitem_selected = wmenu.selected;
            if( canceled || amount <= 0 ) {
                amount = prev_amount;
            }
            if( cb.spawn_everything ) {
                wmenu.ret--;
            } else {
                break;
            }
        }
    } while( wmenu.ret >= 0 );
}

/*
 * Set skill on any player object; player character or NPC
 */
void debug_menu::wishskill( Character *who )
{
    const int skoffset = 1;
    uilist skmenu;
    skmenu.text = _( "Select a skill to modify" );
    skmenu.allow_anykey = true;
    skmenu.addentry( 0, true, '1', _( "Modify all skills…" ) );

    auto sorted_skills = Skill::get_skills_sorted_by( []( const Skill & a, const Skill & b ) {
        return localized_compare( a.name(), b.name() );
    } );

    std::vector<int> origskills;
    origskills.reserve( sorted_skills.size() );

    for( const auto &s : sorted_skills ) {
        const int level = who->get_skill_level( s->ident() );
        skmenu.addentry( origskills.size() + skoffset, true, -2, _( "@ %d: %s  " ), level,
                         s->name() );
        origskills.push_back( level );
    }

    shared_ptr_fast<ui_adaptor> skmenu_ui = skmenu.create_or_get_ui_adaptor();

    do {
        skmenu.query();
        int skill_id = -1;
        int skset = -1;
        const int sksel = skmenu.selected - skoffset;
        if( skmenu.ret == UILIST_UNBOUND && ( skmenu.keypress == KEY_LEFT ||
                                              skmenu.keypress == KEY_RIGHT ) ) {
            if( sksel >= 0 && sksel < static_cast<int>( sorted_skills.size() ) ) {
                skill_id = sksel;
                skset = who->get_skill_level( sorted_skills[skill_id]->ident() ) +
                        ( skmenu.keypress == KEY_LEFT ? -1 : 1 );
            }
        } else if( skmenu.ret >= 0 && sksel >= 0 &&
                   sksel < static_cast<int>( sorted_skills.size() ) ) {
            skill_id = sksel;
            const Skill &skill = *sorted_skills[skill_id];
            const int NUM_SKILL_LVL = 21;
            uilist sksetmenu;
            sksetmenu.w_height_setup = NUM_SKILL_LVL + 4;
            sksetmenu.w_x_setup = [&]( int ) -> int {
                return skmenu.w_x + skmenu.w_width + 1;
            };
            sksetmenu.w_y_setup = [&]( const int height ) {
                return std::max( 0, skmenu.w_y + ( skmenu.w_height - height ) / 2 );
            };
            sksetmenu.settext( string_format( _( "Set '%s' to…" ), skill.name() ) );
            const int skcur = who->get_skill_level( skill.ident() );
            sksetmenu.selected = skcur;
            for( int i = 0; i < NUM_SKILL_LVL; i++ ) {
                sksetmenu.addentry( i, true, i + 48, "%d%s", i, skcur == i ? _( " (current)" ) : "" );
            }
            sksetmenu.query();
            skset = sksetmenu.ret;
        }

        if( skill_id >= 0 && skset >= 0 ) {
            const Skill &skill = *sorted_skills[skill_id];
            who->set_skill_level( skill.ident(), skset );
            skmenu.textformatted[0] = string_format( _( "%s set to %d             " ),
                                      skill.name(),
                                      who->get_skill_level( skill.ident() ) ).substr( 0, skmenu.w_width - 4 );
            skmenu.entries[skill_id + skoffset].txt = string_format( _( "@ %d: %s  " ),
                    who->get_skill_level( skill.ident() ),
                    skill.name() );
            skmenu.entries[skill_id + skoffset].text_color =
                who->get_skill_level( skill.ident() ) == origskills[skill_id] ?
                skmenu.text_color : c_yellow;
        } else if( skmenu.ret == 0 && sksel == -1 ) {
            const int ret = uilist( _( "Alter all skill values" ), {
                _( "Add 3" ), _( "Add 1" ),
                _( "Subtract 1" ), _( "Subtract 3" ), _( "Set to 0" ),
                _( "Set to 5" ), _( "Set to 10" ), _( "(Reset changes)" )
            } );
            if( ret >= 0 ) {
                int skmod = 0;
                int skset = -1;
                if( ret < 4 ) {
                    skmod = 3 - ret * 2;
                } else if( ret < 7 ) {
                    skset = ( ret - 4 ) * 5;
                }
                for( size_t skill_id = 0; skill_id < sorted_skills.size(); skill_id++ ) {
                    const Skill &skill = *sorted_skills[skill_id];
                    int changeto = skmod != 0 ? who->get_skill_level( skill.ident() ) + skmod :
                                   skset != -1 ? skset : origskills[skill_id];
                    who->set_skill_level( skill.ident(), std::max( 0, changeto ) );
                    skmenu.entries[skill_id + skoffset].txt = string_format( _( "@ %d: %s  " ),
                            who->get_skill_level( skill.ident() ),
                            skill.name() );
                    who->get_skill_level_object( skill.ident() ).practice();
                    skmenu.entries[skill_id + skoffset].text_color =
                        who->get_skill_level( skill.ident() ) == origskills[skill_id] ? skmenu.text_color : c_yellow;
                }
            }
        }
    } while( skmenu.ret != UILIST_CANCEL );
}
