#include "diary.h"

#include <string>
#include <list>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <string_view>
#include <utility>

#include "avatar.h"
#include "bionics.h"
#include "calendar.h"
#include "cata_utility.h"
#include "character_martial_arts.h"
#include "filesystem.h"
#include "fstream_utils.h"
#include "game.h"
#include "json.h"
#include "magic.h"
#include "mission.h"
#include "mtype.h"
#include "mutation.h"
#include "output.h"
#include "path_info.h"
#include "skill.h"
#include "string_formatter.h"
#include "type_id.h"
#include "overmap_ui.h"
#include "units.h"
#include "world.h"

diary_page::diary_page() = default;

std::vector<std::string> diary::get_pages_list()
{
    std::vector<std::string> result;
    result.reserve( pages.size() );
    for( std::unique_ptr<diary_page> &n : pages ) {
        result.push_back( to_string( n->turn ) );
    }
    return result;
}

int diary::set_opened_page( int page_number )
{
    if( page_number != opened_page ) {
        change_list.clear();
        desc_map.clear();
    }
    if( pages.empty() ) {
        opened_page = -1;
    } else if( page_number < 0 ) {
        opened_page = pages.size() - 1;
    } else {
        opened_page = page_number % pages.size();
    }
    return opened_page;
}


diary_page *diary::get_page_ptr( int offset )
{
    if( !pages.empty() && opened_page + offset >= 0 ) {

        return pages[opened_page + offset].get();
    }
    return nullptr;
}

void diary::add_to_change_list( const std::string &entry, const std::string &desc )
{
    if( !desc.empty() ) {
        desc_map[change_list.size() + get_head_text().size()] = desc;
    }
    change_list.push_back( entry );
}



void diary::spell_changes()
{
    avatar *u = &get_avatar();
    diary_page *curr_page = get_page_ptr();
    diary_page *prev_page = get_page_ptr( -1 );
    if( curr_page == nullptr ) {
        return;
    }
    if( prev_page == nullptr ) {
        if( !curr_page->known_spells.empty() ) {
            add_to_change_list( _( "Known spells:" ) );
            for( const std::pair<const string_id<spell_type>, int> &elem : curr_page->known_spells ) {
                const spell s = u->magic->get_spell( elem.first );
                add_to_change_list( string_format( _( "%s: %d" ), s.name(), elem.second ), s.description() );
            }
            add_to_change_list( " " );
        }
    } else {
        if( !curr_page->known_spells.empty() ) {
            bool flag = true;
            for( const std::pair<const string_id<spell_type>, int> &elem : curr_page->known_spells ) {
                if( prev_page->known_spells.contains( elem.first ) ) {
                    const int prev_lvl = prev_page->known_spells[elem.first];
                    if( elem.second != prev_lvl ) {
                        if( flag ) {
                            add_to_change_list( _( "Improved/new spells: " ) );
                            flag = false;
                        }
                        const spell s = u->magic->get_spell( elem.first );
                        add_to_change_list( string_format( _( "%s: %d -> %d" ), s.name(), prev_lvl, elem.second ),
                                            s.description() );
                    }
                } else {
                    if( flag ) {
                        add_to_change_list( _( "Improved/new spells: " ) );
                        flag = false;
                    }
                    const spell s = u->magic->get_spell( elem.first );
                    add_to_change_list( string_format( _( "%s: %d" ), s.name(), elem.second ), s.description() );
                }
            }
            if( !flag ) {
                add_to_change_list( " " );
            }
        }
    }
}


void diary::martial_art_changes()
{
    diary_page *curr_page = get_page_ptr();
    diary_page *prev_page = get_page_ptr( -1 );
    if( curr_page == nullptr ) {
        return;
    }
    if( prev_page == nullptr ) {
        if( !curr_page->known_martial_arts.empty() ) {
            add_to_change_list( _( "Known martial arts:" ) );
            for( const matype_id &elem : curr_page->known_martial_arts ) {
                const auto &ma = elem.obj();
                add_to_change_list( ma.name.translated(), ma.description.translated() );
            }
            add_to_change_list( "" );
        }
    } else {
        if( prev_page->known_martial_arts.empty() && !curr_page->known_martial_arts.empty() ) {
            add_to_change_list( _( "New martial arts:" ) );
            for( const matype_id &elem : curr_page->known_martial_arts ) {
                const auto &ma = elem.obj();
                add_to_change_list( ma.name.translated(), ma.description.translated() );
            }
            add_to_change_list( "" );
        } else {

            bool flag = true;
            for( const matype_id &elem : curr_page->known_martial_arts ) {
                if( std::find( prev_page->known_martial_arts.begin(), prev_page->known_martial_arts.end(),
                               elem ) == prev_page->known_martial_arts.end() ) {
                    if( flag ) {
                        add_to_change_list( _( "New martial arts:" ) );
                        flag = false;
                    }
                    const auto &ma = elem.obj();
                    add_to_change_list( ma.name.translated(), ma.description.translated() );
                }

            }
            if( !flag ) {
                add_to_change_list( " " );
            }
        }
    }
}

void diary::mission_changes()
{
    diary_page *curr_page = get_page_ptr();
    diary_page *prev_page = get_page_ptr( -1 );
    if( curr_page == nullptr ) {
        return;
    }
    if( prev_page == nullptr ) {
        auto add_missions = [&]( const std::string & name, const std::vector<int> *missions ) {
            if( !missions->empty() ) {
                bool flag = true;

                for( const int uid : *missions ) {
                    mission *miss = mission::find( uid );
                    if( miss != nullptr ) {
                        if( flag ) {
                            add_to_change_list( name );
                            flag = false;
                        }
                        add_to_change_list( miss->name(), miss->get_description() );
                    }
                }
                if( !flag ) {
                    add_to_change_list( " " );
                }
            }
        };
        add_missions( _( "Active missions:" ), &curr_page->mission_active );
        add_missions( _( "Completed missions:" ), &curr_page->mission_completed );
        add_missions( _( "Failed missions:" ), &curr_page->mission_failed );

    } else {
        auto add_missions = [&]( const std::string & name, const std::vector<int> *missions,
        const std::vector<int> *prev_missions ) {
            bool flag = true;
            for( const int uid : *missions ) {
                if( std::find( prev_missions->begin(), prev_missions->end(), uid ) == prev_missions->end() ) {
                    mission *miss = mission::find( uid );
                    if( miss != nullptr ) {
                        if( flag ) {
                            add_to_change_list( name );
                            flag = false;
                        }
                        add_to_change_list( miss->name(), miss->get_description() );
                    }

                }
                if( !flag ) {
                    add_to_change_list( " " );
                }
            }
        };
        add_missions( _( "New missions:" ), &curr_page->mission_active, &prev_page->mission_active );
        add_missions( _( "New completed missions:" ), &curr_page->mission_completed,
                      &prev_page->mission_completed );
        add_missions( _( "New failed missions:" ), &curr_page->mission_failed, &prev_page->mission_failed );

    }
}

void diary::bionic_changes()
{
    diary_page *curr_page = get_page_ptr();
    diary_page *prev_page = get_page_ptr( -1 );
    if( curr_page == nullptr ) {
        return;
    }
    if( prev_page == nullptr ) {
        if( !curr_page->bionics.empty() ) {
            add_to_change_list( _( "Bionics:" ) );
            for( const bionic_id &elem : curr_page->bionics ) {
                const bionic_data &b = elem.obj();
                add_to_change_list( b.name.translated(), b.description.translated() );
            }
            add_to_change_list( " " );
        }
    } else {

        bool flag = true;
        if( !curr_page->bionics.empty() ) {
            for( const bionic_id &elem : curr_page->bionics ) {

                if( std::find( prev_page->bionics.begin(), prev_page->bionics.end(),
                               elem ) == prev_page->bionics.end() ) {
                    if( flag ) {
                        add_to_change_list( _( "New bionics:" ) );
                        flag = false;
                    }
                    const bionic_data &b = elem.obj();
                    add_to_change_list( b.name.translated(), b.description.translated() );
                }
            }
            if( !flag ) {
                add_to_change_list( " " );
            }
        }

        flag = true;
        if( !prev_page->bionics.empty() ) {
            for( const bionic_id &elem : prev_page->bionics ) {
                if( std::find( curr_page->bionics.begin(), curr_page->bionics.end(),
                               elem ) == curr_page->bionics.end() ) {
                    if( flag ) {
                        add_to_change_list( _( "Lost bionics:" ) );
                        flag = false;
                    }
                    const bionic_data &b = elem.obj();
                    add_to_change_list( b.name.translated(), b.description.translated() );
                }
            }
            if( !flag ) {
                add_to_change_list( " " );
            }
        }
    }
}

void diary::kill_changes()
{
    diary_page *curr_page = get_page_ptr();
    diary_page *prev_page = get_page_ptr( -1 );
    if( curr_page == nullptr ) {
        return;
    }
    if( prev_page == nullptr ) {
        if( !curr_page->kills.empty() ) {
            add_to_change_list( _( "Kills:" ) );
            for( const std::pair<const string_id<mtype>, int> &elem : curr_page->kills ) {
                const mtype &m = elem.first.obj();
                nc_color color = m.color;
                std::string symbol = m.sym;
                std::string nname = m.nname( elem.second );
                add_to_change_list( string_format( "%4d ", elem.second ) + colorize( symbol,
                                    color ) + " " + colorize( nname, c_light_gray ), m.get_description() );
            }
            add_to_change_list( " " );
        }
        if( !curr_page->npc_kills.empty() ) {
            add_to_change_list( _( "NPC killed:" ) );
            for( const std::string &npc_name : curr_page->npc_kills ) {
                add_to_change_list( string_format( "%4d ", 1 ) + colorize( "@ " + npc_name, c_magenta ) );
            }
            add_to_change_list( " " );
        }

    } else {

        if( !curr_page->kills.empty() ) {

            bool flag = true;
            for( const std::pair<const string_id<mtype>, int> &elem : curr_page->kills ) {
                const mtype &m = elem.first.obj();
                nc_color color = m.color;
                std::string symbol = m.sym;
                std::string nname = m.nname( elem.second );
                int kills = elem.second;
                if( prev_page->kills.contains( elem.first ) ) {
                    const int prev_kills = prev_page->kills[elem.first];
                    if( kills > prev_kills ) {
                        if( flag ) {
                            add_to_change_list( _( "Kills:" ) );
                            flag = false;
                        }
                        kills = kills - prev_kills;
                        add_to_change_list( string_format( "%4d ", kills ) + colorize( symbol,
                                            color ) + " " + colorize( nname, c_light_gray ), m.get_description() );
                    }
                } else {
                    if( flag ) {
                        add_to_change_list( _( "Kills:" ) );
                        flag = false;
                    }
                    add_to_change_list( string_format( "%4d ", kills ) + colorize( symbol,
                                        color ) + " " + colorize( nname, c_light_gray ), m.get_description() );
                }


            }
            if( !flag ) {
                add_to_change_list( " " );
            }

        }
        if( !curr_page->npc_kills.empty() ) {

            const std::vector<std::string> &prev_npc_kills = prev_page->npc_kills;

            bool flag = true;
            for( const std::string &npc_name : curr_page->npc_kills ) {

                if( ( std::find( prev_npc_kills.begin(), prev_npc_kills.end(),
                                 npc_name ) == prev_npc_kills.end() ) ) {
                    if( flag ) {
                        add_to_change_list( _( "NPC killed:" ) );
                        flag = false;
                    }
                    add_to_change_list( string_format( "%4d ", 1 ) + colorize( "@ " + npc_name, c_magenta ) );
                }

            }
            if( !flag ) {
                add_to_change_list( " " );
            }
        }
    }
}


void diary::skill_changes()
{
    diary_page *curr_page = get_page_ptr();
    diary_page *prev_page = get_page_ptr( -1 );
    if( curr_page == nullptr ) {
        return;
    }
    if( prev_page == nullptr ) {
        if( curr_page->skill_levels.empty() ) {
            return;
        } else {

            add_to_change_list( _( "Skills:" ) );
            for( const std::pair<const string_id<Skill>, int> &elem : curr_page->skill_levels ) {

                if( elem.second > 0 ) {
                    Skill s = elem.first.obj();
                    add_to_change_list( string_format( "<color_light_blue>%s: %d</color>", s.name(), elem.second ),
                                        s.description() );
                }
            }
            add_to_change_list( "" );
        }
    } else {

        bool flag = true;
        for( const std::pair<const string_id<Skill>, int> &elem : curr_page->skill_levels ) {
            if( prev_page->skill_levels.contains( elem.first ) ) {
                if( prev_page->skill_levels[elem.first] != elem.second ) {
                    if( flag ) {
                        add_to_change_list( _( "Skills:" ) );
                        flag = false;
                    }
                    Skill s = elem.first.obj();
                    add_to_change_list( string_format( _( "<color_light_blue>%s: %d -> %d</color>" ), s.name(),
                                                       prev_page->skill_levels[elem.first], elem.second ), s.description() );
                }

            }

        }
        if( !flag ) {
            add_to_change_list( " " );
        }

    }
}

void diary::trait_changes()
{
    diary_page *curr_page = get_page_ptr();
    diary_page *prev_page = get_page_ptr( -1 );
    if( curr_page == nullptr ) {
        return;
    }
    if( prev_page == nullptr ) {
        if( !curr_page->traits.empty() ) {
            add_to_change_list( _( "Mutations:" ) );
            for( const trait_id &elem : curr_page->traits ) {
                const mutation_branch &trait = elem.obj();
                add_to_change_list( colorize( trait.name(), trait.get_display_color() ), trait.desc() );
            }
            add_to_change_list( "" );
        }
    } else {
        if( prev_page->traits.empty() && !curr_page->traits.empty() ) {
            add_to_change_list( _( "Mutations:" ) );
            for( const trait_id &elem : curr_page->traits ) {
                const mutation_branch &trait = elem.obj();
                add_to_change_list( colorize( trait.name(), trait.get_display_color() ), trait.desc() );

            }
            add_to_change_list( "" );
        } else {

            bool flag = true;
            for( const trait_id &elem : curr_page->traits ) {

                if( std::find( prev_page->traits.begin(), prev_page->traits.end(),
                               elem ) == prev_page->traits.end() ) {
                    if( flag ) {
                        add_to_change_list( _( "Gained mutations:" ) );
                        flag = false;
                    }
                    const mutation_branch &trait = elem.obj();
                    add_to_change_list( colorize( trait.name(), trait.get_display_color() ), trait.desc() );
                }

            }
            if( !flag ) {
                add_to_change_list( " " );
            }

            flag = true;
            for( const trait_id &elem : prev_page->traits ) {

                if( std::find( curr_page->traits.begin(), curr_page->traits.end(),
                               elem ) == curr_page->traits.end() ) {
                    if( flag ) {
                        add_to_change_list( _( "Lost mutations:" ) );
                        flag = false;
                    }
                    const mutation_branch &trait = elem.obj();
                    add_to_change_list( colorize( trait.name(), trait.get_display_color() ), trait.desc() );
                }
            }
            if( !flag ) {
                add_to_change_list( " " );
            }

        }
    }
}

void diary::stat_changes()
{
    diary_page *curr_page = get_page_ptr();
    diary_page *prev_page = get_page_ptr( -1 );
    if( curr_page == nullptr ) {
        return;
    }
    if( prev_page == nullptr ) {
        add_to_change_list( _( "Stats:" ) );
        add_to_change_list( string_format( _( "Strength: %d" ), curr_page->strength ) );
        add_to_change_list( string_format( _( "Dexterity: %d" ), curr_page->dexterity ) );
        add_to_change_list( string_format( _( "Intelligence: %d" ), curr_page->intelligence ) );
        add_to_change_list( string_format( _( "Perception: %d" ), curr_page->perception ) );
        add_to_change_list( " " );
    } else {

        bool flag = true;
        if( curr_page->strength != prev_page->strength ) {
            if( flag ) {
                add_to_change_list( _( "Stats:" ) );
                flag = false;
            }
            add_to_change_list( string_format( _( "Strength: %d -> %d" ), prev_page->strength,
                                               curr_page->strength ) );
        }
        if( curr_page->dexterity != prev_page->dexterity ) {
            if( flag ) {
                add_to_change_list( _( "Stats:" ) );
                flag = false;
            }
            add_to_change_list( string_format( _( "Dexterity: %d -> %d" ), prev_page->dexterity,
                                               curr_page->dexterity ) );
        }
        if( curr_page->intelligence != prev_page->intelligence ) {
            if( flag ) {
                add_to_change_list( _( "Stats: " ) );
                flag = false;
            }
            add_to_change_list( string_format( _( "Intelligence: %d -> %d" ), prev_page->intelligence,
                                               curr_page->intelligence ) );
        }

        if( curr_page->perception != prev_page->perception ) {
            if( flag ) {
                add_to_change_list( _( "Stats:" ) );
                flag = false;
            }
            add_to_change_list( string_format( _( "Perception: %d -> %d" ), prev_page->perception,
                                               curr_page->perception ) );
        }
        if( !flag ) {
            add_to_change_list( " " );
        }

    }
}


void diary::max_power_level_changes()
{
    diary_page *curr_page = get_page_ptr();
    diary_page *prev_page = get_page_ptr( -1 );
    if( curr_page == nullptr ) {
        return;
    }
    if( prev_page == nullptr ) {
        add_to_change_list( "Max power:" );
        add_to_change_list( string_format( _( "<color_yellow>%ikJ</color>" ),
                                           units::to_kilojoule( curr_page->max_power_level ) ) );
        add_to_change_list( " " );
    } else {

        if( curr_page->max_power_level != prev_page->max_power_level ) {
            add_to_change_list( "Max power:" );
            add_to_change_list( string_format(
                                    _( "<color_yellow>%i</color> -> <color_yellow>%ikJ</color>" ),
                                    units::to_kilojoule( prev_page->max_power_level ),
                                    units::to_kilojoule( curr_page->max_power_level ) ) );
            add_to_change_list( " " );
        }
    }
}

std::vector<std::string> diary::get_change_list()
{
    if( !change_list.empty() ) {
        return change_list;
    }
    if( !pages.empty() ) {
        stat_changes();
        skill_changes();
        trait_changes();
        bionic_changes();
        max_power_level_changes();
        spell_changes();
        martial_art_changes();
        mission_changes();
        kill_changes();
    }
    return change_list;
}

std::map<int, std::string> diary::get_desc_map()
{
    if( !desc_map.empty() ) {
        return desc_map;
    } else {
        get_change_list();
        return desc_map;
    }
}


std::string diary::get_page_text()
{

    if( !pages.empty() ) {
        return get_page_ptr()->m_text;
    }
    return "";
}

std::vector<std::string> diary::get_head_text()
{

    if( !pages.empty() ) {
        const diary_page *prev_page_ptr = get_page_ptr( -1 );
        const time_point prev_turn = ( prev_page_ptr != nullptr ) ? prev_page_ptr->turn :
                                     calendar::turn_zero;
        const time_duration turn_diff = get_page_ptr()->turn - prev_turn;
        const int days = to_days<int>( turn_diff );
        const int hours = to_hours<int>( turn_diff ) % 24;
        const int minutes = to_minutes<int>( turn_diff ) % 60;
        const int seconds = to_seconds<int>( turn_diff ) % 60;

        std::string time_diff_text;
        if( opened_page != 0 ) {
            // will display the time since last entry (excluding days) as "hours:min:sec"
            std::string days_text;
            std::string time_text;
            if( days > 0 ) {
                days_text = string_format( vgettext( "%d day, ", "%d days, ", days ), days );
            }

            time_text += ( hours > 9 ) ? std::to_string( hours ) : "0" + std::to_string( hours );
            time_text += ":";
            time_text += ( minutes > 9 ) ? std::to_string( minutes ) : "0" + std::to_string( minutes );
            time_text += ":";
            time_text += ( seconds > 9 ) ? std::to_string( seconds ) : "0" + std::to_string( seconds );

            //~ %1$s is xx days, %2$d is the time left in universal format
            time_diff_text = string_format( _( "%1$s%2$s since last entry" ),
                                            days_text, time_text );

        }
        //~ Head text of a diary page
        //~ %1$d is the current page number, %2$d is the number of pages in total
        std::vector<std::string> head_text = { string_format( _( "Entry: %1$d/%2$d" ), opened_page + 1, pages.size() ) };

        std::string complete_time_text = to_string( get_page_ptr()->turn ); // get complete time
        std::string day_and_time_text = complete_time_text.substr( complete_time_text.find_last_of( ',' ) +
                                        2 );
        day_and_time_text[0] = std::toupper( day_and_time_text[0] );
        std::string year_and_season_text = complete_time_text.substr( 0,
                                           complete_time_text.find_last_of( ',' ) + 1 );

        std::string overmap_position_text = ( !get_page_ptr()->overmap_position_str.empty() ) ?
                                            string_format(
                                                //~ %1$s is the player's position on the overmap when writing the page
                                                _( "Location: %1$s" ),
                                                get_page_ptr()->overmap_position_str ) : "";

        head_text.insert( head_text.end(), year_and_season_text );
        head_text.insert( head_text.end(), day_and_time_text );
        head_text.insert( head_text.end(), time_diff_text );
        head_text.insert( head_text.end(), overmap_position_text );

        if( !time_diff_text.empty() ) {
            head_text.insert( head_text.end(), { "" } );
        }
        if( !overmap_position_text.empty() ) {
            head_text.insert( head_text.end(), { "" } );
        }

        return head_text;
    }
    return { "" };
}

void diary::death_entry()
{
    bool last_time = query_yn( _( "Open your diary for the last time?" ) );
    if( last_time ) {
        show_diary_ui( this );
    }
    export_to_md( true );
}

diary::diary()
{
    owner = get_avatar().name;
}
void diary::set_page_text( std::string text )
{
    get_page_ptr()->m_text = std::move( text );
}

void diary::new_page()
{
    std::unique_ptr<diary_page> page( new diary_page() );
    page->m_text = std::string();
    page->turn = calendar::turn;
    page->kills = g->get_kill_tracker().kills;
    page->npc_kills = g->get_kill_tracker().npc_kills;
    avatar *u = &get_avatar();

    page->overmap_position_str = overmap_ui::fmt_omt_coords( u->global_omt_location() );
    page->mission_completed = mission::to_uid_vector( u->get_completed_missions() );
    page->mission_active = mission::to_uid_vector( u->get_active_missions() );
    page->mission_failed = mission::to_uid_vector( u->get_failed_missions() );
    page->male = u->male;
    page->strength = u->get_str_base();
    page->dexterity = u->get_dex_base();
    page->intelligence = u->get_int_base();
    page->perception = u->get_per_base();
    page->traits = u->get_mutations( false );
    const auto spells = u->magic->get_spells();
    for( const spell *spell : spells ) {
        const spell_id &id = spell->id();
        const int lvl = spell->get_level();
        page->known_spells[id] = lvl;
    }
    page->known_martial_arts = u->martial_arts_data->get_known_styles();
    page->bionics = u->get_bionics();
    for( Skill &elem : Skill::skills ) {
        int level = u->get_skill_level_object( elem.ident() ).level();
        page->skill_levels.insert( { elem.ident(), level } );
    }
    page->max_power_level = u->get_max_power_level();
    diary::pages.push_back( std::move( page ) );
}

void diary::delete_page()
{
    if( opened_page < static_cast<int>( pages.size() ) ) {
        pages.erase( pages.begin() + opened_page );
        set_opened_page( opened_page - 1 );
    }
}

void diary::export_to_md( bool last_export )
{
    std::ofstream myfile;
    std::string path = last_export
                       ? PATH_INFO::memorialdir()
                       : ( g->get_active_world() ? g->get_active_world()->info->folder_path() : PATH_INFO::savedir() );
    path += "/" + owner + "s_diary.md";
    myfile.open( path );

    for( int i = 0; i < static_cast<int>( pages.size() ); i++ ) {
        set_opened_page( i );
        const diary_page page = *get_page_ptr();

        std::vector<std::string> left_diary_text = this->get_head_text();
        std::vector<std::string> change_list = this->get_change_list();
        left_diary_text.insert( left_diary_text.end(), change_list.begin(), change_list.end() );
        // add "## " before each "Entry" line to visually differentiate each page
        left_diary_text[0] = "## " + left_diary_text[0];

        for( const std::string &str : left_diary_text ) {
            myfile << remove_color_tags( str ) + "\n";
        }
        const std::vector<std::string> folded_texts = foldstring( page.m_text, 50 );
        for( const std::string_view folded_text : folded_texts ) {
            myfile << folded_text << "\n";
        }
        myfile << "\n\n\n";
    }
    myfile.close();
}

bool diary::store()
{
    if( !g->get_active_world() ) {
        return false;
    }

    std::string name = base64_encode( get_avatar().get_save_id() + "_diary" );
    const bool is_writen = g->get_active_world()->write_to_file( name + ".json", [&](
    std::ostream & fout ) {
        serialize( fout );
    }, _( "diary data" ) );
    return is_writen;
}

void diary::serialize( std::ostream &fout )
{
    JsonOut jsout( fout, true );
    jsout.start_object();
    serialize( jsout );
    jsout.end_object();
}

void diary::serialize( JsonOut &jsout )
{
    jsout.member( "owner", owner );
    jsout.member( "pages" );
    jsout.start_array();
    for( std::unique_ptr<diary_page> &n : pages ) {
        jsout.start_object();
        jsout.member( "text", n->m_text );
        jsout.member( "turn", n->turn );
        jsout.member( "overmap_position_str", n->overmap_position_str );
        jsout.member( "completed", n->mission_completed );
        jsout.member( "active", n->mission_active );
        jsout.member( "failed", n->mission_failed );
        jsout.member( "kills", n->kills );
        jsout.member( "npc_kills", n->npc_kills );
        jsout.member( "male", n->male );
        jsout.member( "str", n->strength );
        jsout.member( "dex", n->dexterity );
        jsout.member( "int", n->intelligence );
        jsout.member( "per", n->perception );
        jsout.member( "traits", n->traits );
        jsout.member( "bionics", n->bionics );
        jsout.member( "spells", n->known_spells );
        jsout.member( "skill_levels", n->skill_levels );
        jsout.member( "martial_arts", n->known_martial_arts );
        jsout.member( "max_power_level", n->max_power_level );
        jsout.end_object();
    }
    jsout.end_array();
}



void diary::load()
{
    if( !g->get_active_world() ) {
        return;
    }

    std::string name = base64_encode( get_avatar().get_save_id() + "_diary" );
    if( g->get_active_world()->file_exist( name + ".json" ) ) {
        g->get_active_world()->read_from_file( name + ".json", [&]( std::istream & fin ) {
            deserialize( fin );
        } );
    }
}

void diary::deserialize( std::istream &fin )
{
    JsonIn jsin( fin );
    deserialize( jsin );
}

void diary::deserialize( JsonIn &jsin )
{
    try {
        JsonObject data = jsin.get_object();

        data.read( "owner", owner );
        pages.clear();
        for( JsonObject elem : data.get_array( "pages" ) ) {
            std::unique_ptr<diary_page> page( new diary_page() );
            page->m_text = elem.get_string( "text" );
            elem.read( "turn", page->turn );
            elem.read( "overmap_position_str", page->overmap_position_str );
            elem.read( "active", page->mission_active );
            elem.read( "completed", page->mission_completed );
            elem.read( "failed", page->mission_failed );
            elem.read( "kills", page->kills );
            elem.read( "npc_kills", page->npc_kills );
            elem.read( "male", page->male );
            elem.read( "str", page->strength );
            elem.read( "dex", page->dexterity );
            elem.read( "int", page->intelligence );
            elem.read( "per", page->perception );
            elem.read( "traits", page->traits );
            elem.read( "bionics", page->bionics );
            elem.read( "spells", page->known_spells );
            elem.read( "skill_levels", page->skill_levels );
            elem.read( "martial_arts", page->known_martial_arts );
            elem.read( "max_power_level", page->max_power_level );
            diary::pages.push_back( std::move( page ) );
        }
    } catch( const JsonError &e ) {

    }
}
