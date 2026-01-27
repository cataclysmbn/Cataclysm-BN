#include "avatar.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cstdlib>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <utility>

#include "action.h"
#include "bodypart.h"
#include "calendar.h"
#include "catalua.h"
#include "catalua_hooks.h"
#include "catalua_sol.h"
#include "cata_utility.h"
#include "catacharset.h"
#include "character.h"
#include "character_effects.h"
#include "character_id.h"
#include "character_functions.h"
#include "character_martial_arts.h"
#include "character_stat.h"
#include "color.h"
#include "debug.h"
#include "diary.h"
#include "effect.h"
#include "enums.h"
#include "event.h"
#include "event_bus.h"
#include "faction.h"
#include "flag.h"
#include "game.h"
#include "game_constants.h"
#include "help.h"
#include "inventory.h"
#include "init.h"
#include "item.h"
#include "item_contents.h"
#include "item_factory.h"
#include "locations.h"
#include "itype.h"
#include "iuse.h"
#include "kill_tracker.h"
#include "make_static.h"
#include "magic_teleporter_list.h"
#include "map.h"
#include "map_memory.h"
#include "martialarts.h"
#include "messages.h"
#include "mission.h"
#include "monster.h"
#include "morale.h"
#include "morale_types.h"
#include "mtype.h"
#include "npc.h"
#include "options.h"
#include "output.h"
#include "overmap.h"
#include "legacy_pathfinding.h"
#include "player.h"
#include "player_activity.h"
#include "recipe.h"
#include "ret_val.h"
#include "rng.h"
#include "skill.h"
#include "stomach.h"
#include "string_formatter.h"
#include "string_id.h"
#include "translations.h"
#include "type_id.h"
#include "ui.h"
#include "vehicle.h"
#include "vpart_position.h"

static const activity_id ACT_READ( "ACT_READ" );

static const bionic_id bio_eye_optic( "bio_eye_optic" );
static const bionic_id bio_memory( "bio_memory" );
static const bionic_id bio_infolink( "bio_infolink" );

static const efftype_id effect_alarm_clock( "alarm_clock" );
static const efftype_id effect_contacts( "contacts" );
static const efftype_id effect_sleep( "sleep" );
static const efftype_id effect_slept_through_alarm( "slept_through_alarm" );
static const efftype_id effect_cold( "cold" );
static const efftype_id effect_hot( "hot" );
static const efftype_id effect_hot_speed( "hot_speed" );

static const itype_id itype_guidebook( "guidebook" );

static const trait_id trait_CENOBITE( "CENOBITE" );
static const trait_id trait_HYPEROPIC( "HYPEROPIC" );
static const trait_id trait_ILLITERATE( "ILLITERATE" );
static const trait_id trait_PROF_DICEMASTER( "PROF_DICEMASTER" );

static const morale_type MORALE_FOOD_COLD( "morale_food_cold" );
static const morale_type MORALE_FOOD_VERY_COLD( "morale_food_very_cold" );

class JsonIn;
class JsonOut;

static void skim_book_msg( const item &book, avatar &u );

avatar &get_avatar()
{
    return g->u;
}

avatar::avatar()
{
    player_map_memory = std::make_unique<map_memory>();
    show_map_memory = true;
    active_mission = nullptr;
    grab_type = OBJECT_NONE;
    a_diary = nullptr;
    start_location = start_location_id( "sloc_shelter" );
    movecounter = 0;
}

avatar::~avatar() = default;
avatar::avatar( avatar && )  noexcept = default;
avatar &avatar::operator=( avatar && ) noexcept = default;

static void swap_npc( npc &one, npc &two, npc &tmp )
{
    tmp = std::move( one );
    one = std::move( two );
    two = std::move( tmp );
}

void avatar::control_npc( npc &np )
{
    if( !np.is_player_ally() ) {
        debugmsg( "control_npc() called on non-allied npc %s", np.name );
        return;
    }
    // Cancel activities before swap to prevent issues with stale references
    // Avatar's activity would transfer to NPC and reference invalid items/state
    cancel_activity();
    np.cancel_activity();
    if( !shadow_npc ) {
        shadow_npc = std::make_unique<npc>();
        shadow_npc->op_of_u.trust = 10;
        shadow_npc->op_of_u.value = 10;
        shadow_npc->set_attitude( NPCATT_FOLLOW );
    }
    npc tmp;
    std::string save_id = get_save_id();
    // move avatar character data into shadow npc
    swap_character( *shadow_npc, tmp );
    // swap target npc with shadow npc
    swap_npc( *shadow_npc, np, tmp );
    // Reset np's dead state cache since swap_npc moved character data
    np.reset_cached_dead_state();
    // move shadow npc character data into avatar
    swap_character( *shadow_npc, tmp );
    set_save_id( save_id );
    // Swappy the thirst and kcal so swapping is not infinite food with no food
    if( get_option<bool>( "NO_NPC_FOOD" ) ) {
        // You're stomachs become one thing :)
        stomach = np.stomach;
        set_thirst( np.get_thirst( ) );
        set_stored_kcal( np.get_stored_kcal() );
        // NPCs can't whine about a lack of food or water after you leave their body
        np.set_stored_kcal( np.max_stored_kcal() - 100 );
        np.set_thirst( 0 );
    }
    for( auto &pr : np.get_body() ) {
        pr.second.set_location( new wield_item_location( &np ) );
    }
    for( auto &pr : get_body() ) {
        pr.second.set_location( new wield_item_location( this ) );
    }

    for( auto &pr : get_body() ) {
        const bodypart_id &bp = pr.first;
        np.remove_effect( effect_cold, bp.id() );
        np.remove_effect( effect_hot, bp.id() );
        np.remove_effect( effect_hot_speed, bp.id() );
    }

    np.onswapsetpos( np.pos() );
    // the avatar character is no longer a follower NPC
    g->remove_npc_follower( getID() );
    // the previous avatar character is now a follower (unless they're dead)
    if( np.is_dead_state() ) {
        // The swapped-out character was dead, so kill the NPC properly
        np.die( nullptr );
    } else {
        g->add_npc_follower( np.getID() );
        np.set_fac( faction_id( "your_followers" ) );
    }
    // perception and mutations may have changed, so reset light level caches
    g->reset_light_level();
    // center the map on the new avatar character
    g->vertical_shift( posz() );
    g->update_map( *this );
}

void avatar::toggle_map_memory()
{
    show_map_memory = !show_map_memory;
}

bool avatar::should_show_map_memory()
{
    return show_map_memory;
}

bool avatar::save_map_memory()
{
    return player_map_memory->save( g->m.getabs( pos() ) );
}

void avatar::load_map_memory()
{
    player_map_memory->load( g->m.getabs( pos() ) );
}

void avatar::prepare_map_memory_region( const tripoint &p1, const tripoint &p2 )
{
    player_map_memory->prepare_region( p1, p2 );
}

const memorized_terrain_tile &avatar::get_memorized_tile( const tripoint &pos ) const
{
    return player_map_memory->get_tile( pos );
}

void avatar::memorize_tile( const tripoint &pos, const std::string &ter, const int subtile,
                            const int rotation )
{
    player_map_memory->memorize_tile( pos, ter, subtile, rotation );
}

void avatar::memorize_symbol( const tripoint &pos, const int symbol )
{
    player_map_memory->memorize_symbol( pos, symbol );
}

int avatar::get_memorized_symbol( const tripoint &p ) const
{
    return player_map_memory->get_symbol( p );
}

void avatar::clear_memorized_tile( const tripoint &pos )
{
    player_map_memory->clear_memorized_tile( pos );
}

bool avatar::has_memorized_tile_for_autodrive( const tripoint &p ) const
{
    return player_map_memory->has_memory_for_autodrive( p );
}

std::vector<mission *> avatar::get_active_missions() const
{
    return active_missions;
}

std::vector<mission *> avatar::get_completed_missions() const
{
    return completed_missions;
}

std::vector<mission *> avatar::get_failed_missions() const
{
    return failed_missions;
}

mission *avatar::get_active_mission() const
{
    return active_mission;
}

void avatar::reset_all_missions()
{
    active_mission = nullptr;
    active_missions.clear();
    completed_missions.clear();
    failed_missions.clear();
}

tripoint_abs_omt avatar::get_active_mission_target() const
{
    if( active_mission == nullptr ) {
        return overmap::invalid_tripoint;
    }
    return active_mission->get_target();
}

tripoint_abs_omt avatar::get_custom_mission_target()
{
    if( custom_waypoint == nullptr ) {
        return overmap::invalid_tripoint;
    }
    return *custom_waypoint;
}

void avatar::set_active_mission( mission &cur_mission )
{
    const auto iter = std::ranges::find( active_missions, &cur_mission );
    if( iter == active_missions.end() ) {
        debugmsg( "new active mission %d is not in the active_missions list", cur_mission.get_id() );
    } else {
        active_mission = &cur_mission;
    }
}

void avatar::on_mission_assignment( mission &new_mission )
{
    active_missions.push_back( &new_mission );
    set_active_mission( new_mission );
}

void avatar::on_mission_finished( mission &cur_mission )
{
    if( cur_mission.has_failed() ) {
        failed_missions.push_back( &cur_mission );
        add_msg_if_player( m_bad, _( "Mission \"%s\" is failed." ), cur_mission.name() );
    } else {
        completed_missions.push_back( &cur_mission );
        add_msg_if_player( m_good, _( "Mission \"%s\" is successfully completed." ),
                           cur_mission.name() );
    }
    const auto iter = std::ranges::find( active_missions, &cur_mission );
    if( iter == active_missions.end() ) {
        debugmsg( "completed mission %d was not in the active_missions list", cur_mission.get_id() );
    } else {
        active_missions.erase( iter );
    }
    if( &cur_mission == active_mission ) {
        if( active_missions.empty() ) {
            active_mission = nullptr;
        } else {
            active_mission = active_missions.front();
        }
    }
}

const Character *avatar::get_book_reader( const item &book,
        std::vector<std::string> &reasons ) const
{
    const Character *reader = nullptr;
    if( !book.is_book() ) {
        reasons.push_back( string_format( _( "Your %s is not good reading material." ),
                                          book.tname() ) );
        return nullptr;
    }

    // Check for conditions that immediately disqualify the player from reading:
    const optional_vpart_position vp = get_map().veh_at( pos() );
    if( vp && vp->vehicle().player_in_control( *this ) ) {
        reasons.emplace_back( _( "It's a bad idea to read while driving!" ) );
        return nullptr;
    }
    const auto &type = book.type->book;
    if( !character_funcs::is_fun_to_read( *this, book ) && !has_morale_to_read() ) {
        // Low morale still permits skimming
        reasons.emplace_back( _( "What's the point of studying?  (Your morale is too low!)" ) );
        return nullptr;
    }
    const skill_id &skill = type->skill;
    const int skill_level = get_skill_level( skill );
    if( skill && skill_level < type->req ) {
        reasons.push_back( string_format( _( "%s %d needed to understand.  You have %d" ),
                                          skill.obj().name(), type->req, skill_level ) );
        return nullptr;
    }

    // Check for conditions that disqualify us only if no NPCs can read to us
    if( type->intel > 0 && has_trait( trait_ILLITERATE ) ) {
        reasons.emplace_back( _( "You're illiterate!" ) );
    } else if( has_trait( trait_HYPEROPIC ) &&
               !worn_with_flag( flag_FIX_FARSIGHT ) &&
               !has_effect( effect_contacts ) && !has_bionic( bio_eye_optic ) ) {
        reasons.emplace_back( _( "Your eyes won't focus without reading glasses." ) );
    } else if( !character_funcs::can_see_fine_details( *this ) ) {
        // Too dark to read only applies if the player can read to himself
        reasons.emplace_back( _( "It's too dark to read!" ) );
        return nullptr;
    } else {
        return this;
    }

    //Check for NPCs to read for you, negates Illiterate and Far Sighted
    //The fastest-reading NPC is chosen
    if( is_deaf() ) {
        reasons.emplace_back( _( "Maybe someone could read that to you, but you're deaf!" ) );
        return nullptr;
    }

    int time_taken = INT_MAX;
    auto candidates = character_funcs::get_crafting_helpers( *this );

    for( const npc *elem : candidates ) {
        // Check for disqualifying factors:
        if( type->intel > 0 && elem->has_trait( trait_ILLITERATE ) ) {
            reasons.push_back( string_format( _( "%s is illiterate!" ),
                                              elem->disp_name() ) );
        } else if( skill && elem->get_skill_level( skill ) < type->req ) {
            reasons.push_back( string_format( _( "%s %d needed to understand.  %s has %d" ),
                                              skill.obj().name(), type->req, elem->disp_name(), elem->get_skill_level( skill ) ) );
        } else if( elem->has_trait( trait_HYPEROPIC ) &&
                   !elem->worn_with_flag( STATIC( flag_id( "FIX_FARSIGHT" ) ) ) &&
                   !elem->has_effect( effect_contacts ) ) {
            reasons.push_back( string_format( _( "%s needs reading glasses!" ),
                                              elem->disp_name() ) );
        } else if( !character_funcs::fine_detail_vision_mod( *this ) &&
                   !character_funcs::fine_detail_vision_mod( *elem ) ) {
            reasons.push_back( string_format(
                                   _( "It's too dark for %s to read!" ),
                                   elem->disp_name() ) );
        } else if( !elem->sees( *this ) ) {
            reasons.push_back( string_format( _( "%s could read that to you, but they can't see you." ),
                                              elem->disp_name() ) );
        } else if( !character_funcs::is_fun_to_read( *elem, book ) && !elem->has_morale_to_read() ) {
            // Low morale still permits skimming
            reasons.push_back( string_format( _( "%s morale is too low!" ), elem->disp_name( true ) ) );
        } else if( elem->is_blind() ) {
            reasons.push_back( string_format( _( "%s is blind." ), elem->disp_name() ) );
        } else {
            int proj_time = time_to_read( book, *elem );
            if( proj_time < time_taken ) {
                reader = elem;
                time_taken = proj_time;
            }
        }
    }
    //end for all candidates
    return reader;
}

int avatar::time_to_read( const item &book, const Character &reader,
                          const Character *learner ) const
{
    const auto &type = book.type->book;
    const skill_id &skill = type->skill;
    // The reader's reading speed has an effect only if they're trying to understand the book as they read it
    // Reading speed is assumed to be how well you learn from books (as opposed to hands-on experience)
    const bool try_understand = character_funcs::is_fun_to_read( reader, book ) ||
                                reader.get_skill_level( skill ) < type->level;
    int reading_speed = try_understand ? std::max( reader.read_speed(), read_speed() ) : read_speed();
    if( learner ) {
        reading_speed = std::max( reading_speed, learner->read_speed() );
    }

    int retval = type->time * reading_speed;
    retval *= std::min( character_funcs::fine_detail_vision_mod( *this ),
                        character_funcs::fine_detail_vision_mod( reader ) );

    const int effective_int = std::min( { get_int(), reader.get_int(), learner ? learner->get_int() : INT_MAX } );
    if( type->intel > effective_int && !reader.has_trait( trait_PROF_DICEMASTER ) ) {
        retval += type->time * ( type->intel - effective_int ) * 100;
    }
    return retval;
}

diary *avatar::get_avatar_diary()
{
    if( a_diary == nullptr ) {
        a_diary = std::make_unique<diary>();
    }
    return a_diary.get();
}

/**
 * Explanation of ACT_READ activity values:
 *
 * position: ID of the reader
 * targets: 1-element vector with the item_location (always in inventory/wielded) of the book being read
 * index: We are studying until the player with this ID gains a level; 0 indicates reading once
 * values: IDs of the NPCs who will learn something
 * str_values: Parallel to values, these contain the learning penalties (as doubles in string form) as follows:
 *             Experience gained = Experience normally gained * penalty
 */
bool avatar::read( item *loc, const bool continuous )
{
    if( !loc ) {
        add_msg( m_info, _( "Never mind." ) );
        return false;
    }
    item &it = *loc;
    if( !has_identified( it.typeId() ) ) {
        // We insta-identify the book, then try to read it
        items_identified.insert( it.typeId() );
        skim_book_msg( it, *this );
    }
    std::vector<std::string> fail_messages;
    const auto *reader = get_book_reader( it, fail_messages );
    if( reader == nullptr ) {
        // We can't read, and neither can our followers
        for( const std::string &reason : fail_messages ) {
            add_msg( m_bad, reason );
        }
        return false;
    }
    const int time_taken = time_to_read( it, *reader );

    add_msg( m_debug, "avatar::read: time_taken = %d", time_taken );
    player_activity act( ACT_READ, time_taken, continuous ? activity->index : 0,
                         reader->getID().get_value() );
    act.targets.emplace_back( loc );

    if( it.typeId() == itype_guidebook ) {
        // special guidebook effect: print a misc. hint when read
        if( reader != this ) {
            add_msg( m_info, fail_messages[0] );
            dynamic_cast<const npc *>( reader )->say( get_hint() );
        } else {
            add_msg( m_info, get_hint() );
        }
        mod_moves( -100 );
        return false;
    }

    const auto &type = it.type->book;
    const skill_id &skill = type->skill;
    const std::string skill_name = skill ? skill.obj().name() : "";

    // Find NPCs to join the study session:
    std::map<npc *, std::string> learners;
    //reading only for fun
    std::map<npc *, std::string> fun_learners;
    std::map<npc *, std::string> nonlearners;
    auto candidates = character_funcs::get_crafting_helpers( *this );
    for( npc *elem : candidates ) {
        const int lvl = elem->get_skill_level( skill );
        const bool is_fun_to_read = character_funcs::is_fun_to_read( *elem, it );
        const bool skill_req = ( is_fun_to_read && ( !skill || lvl >= type->req ) ) ||
                               ( skill && lvl < type->level && lvl >= type->req );
        const bool morale_req = is_fun_to_read || elem->has_morale_to_read();

        if( !skill_req && elem != reader ) {
            if( skill && lvl < type->req ) {
                nonlearners.insert( { elem, string_format( _( " (needs %d %s)" ), type->req, skill_name ) } );
            } else if( skill ) {
                nonlearners.insert( { elem, string_format( _( " (already has %d %s)" ), type->level, skill_name ) } );
            } else {
                nonlearners.insert( { elem, _( " (uninterested)" ) } );
            }
        } else if( elem->is_deaf() && reader != elem ) {
            nonlearners.insert( { elem, _( " (deaf)" ) } );
        } else if( !morale_req ) {
            nonlearners.insert( { elem, _( " (too sad)" ) } );
        } else if( skill && lvl < type->level ) {
            const double penalty = static_cast<double>( time_taken ) / time_to_read( it, *reader, elem );
            learners.insert( {elem, elem == reader ? _( " (reading aloud to you)" ) : ""} );
            act.values.push_back( elem->getID().get_value() );
            act.str_values.push_back( std::to_string( penalty ) );
        } else {
            fun_learners.insert( {elem, elem == reader ? _( " (reading aloud to you)" ) : "" } );
            act.values.push_back( elem->getID().get_value() );
            act.str_values.emplace_back( "1" );
        }
    }

    if( !continuous ) {
        //only show the menu if there's useful information or multiple options
        if( skill || !nonlearners.empty() || !fun_learners.empty() ) {
            uilist menu;

            // Some helpers to reduce repetition:
            auto length = []( const std::pair<npc *, std::string> &elem ) {
                return utf8_width( elem.first->disp_name() ) + utf8_width( elem.second );
            };

            auto max_length = [&length]( const std::map<npc *, std::string> &m ) {
                auto max_ele = std::ranges::max_element( m,
                               [&length]( const std::pair<npc *, std::string> &left,
                const std::pair<npc *, std::string> &right ) {
                    return length( left ) < length( right );
                } );
                return max_ele == m.end() ? 0 : length( *max_ele );
            };

            auto get_text =
            [&]( const std::map<npc *, std::string> &m, const std::pair<npc *, std::string> &elem ) {
                const int lvl = elem.first->get_skill_level( skill );
                const std::string lvl_text = skill ? string_format( _( " | current level: %d" ), lvl ) : "";
                const std::string name_text = elem.first->disp_name() + elem.second;
                return string_format( "%s%s", left_justify( name_text, max_length( m ) ), lvl_text );
            };

            auto add_header = [&menu]( const std::string & str ) {
                menu.addentry( -1, false, -1, "" );
                uilist_entry header( -1, false, -1, str, c_yellow, c_yellow );
                header.force_color = true;
                menu.entries.push_back( header );
            };

            menu.title = !skill ? string_format( _( "Reading %s" ), it.type_name() ) :
                         //~ %1$s: book name, %2$s: skill name, %3$d and %4$d: skill levels
                         string_format( _( "Reading %1$s (can train %2$s from %3$d to %4$d)" ), it.type_name(),
                                        skill_name, type->req, type->level );

            if( skill ) {
                const int lvl = get_skill_level( skill );
                menu.addentry( getID().get_value(), lvl < type->level, '0',
                               string_format( _( "Read until you gain a level | current level: %d" ), lvl ) );
            } else {
                menu.addentry( -1, false, '0', _( "Read until you gain a level" ) );
            }
            menu.addentry( 0, true, '1', _( "Read once" ) );

            if( skill && !learners.empty() ) {
                add_header( _( "Read until this NPC gains a level:" ) );
                for( const auto &elem : learners ) {
                    menu.addentry( elem.first->getID().get_value(), true, -1,
                                   get_text( learners, elem ) );
                }
            }
            if( !fun_learners.empty() ) {
                add_header( _( "Reading for fun:" ) );
                for( const auto &elem : fun_learners ) {
                    menu.addentry( -1, false, -1, get_text( fun_learners, elem ) );
                }
            }
            if( !nonlearners.empty() ) {
                add_header( _( "Not participating:" ) );
                for( const auto &elem : nonlearners ) {
                    menu.addentry( -1, false, -1, get_text( nonlearners, elem ) );
                }
            }

            menu.query( true );
            if( menu.ret == UILIST_CANCEL ) {
                add_msg( m_info, _( "Never mind." ) );
                return false;
            }
            act.index = menu.ret;
        }
        if( it.type->use_methods.contains( "MA_MANUAL" ) ) {

            if( martial_arts_data->has_martialart( martial_art_learned_from( *it.type ) ) ) {
                add_msg_if_player( m_info, _( "You already know all this book has to teach." ) );
                activity->set_to_null();
                return false;
            }

            uilist menu;
            menu.title = string_format( _( "Train %s from manual:" ),
                                        martial_art_learned_from( *it.type )->name );
            menu.addentry( -1, true, '1', _( "Train once" ) );
            menu.addentry( getID().get_value(), true, '0', _( "Train until tired or success" ) );
            menu.query( true );
            if( menu.ret == UILIST_CANCEL ) {
                add_msg( m_info, _( "Never mind." ) );
                return false;
            }
            act.index = menu.ret;
        }
        add_msg( m_info, _( "Now reading %s, %s to stop early." ),
                 it.type_name(), press_x( ACTION_PAUSE ) );
    }

    // Print some informational messages, but only the first time or if the information changes

    if( !continuous || activity->position != act.position ) {
        if( reader != this ) {
            add_msg( m_info, fail_messages[0] );
            add_msg( m_info, _( "%s reads aloud…" ), reader->disp_name() );
        } else if( !learners.empty() || !fun_learners.empty() ) {
            add_msg( m_info, _( "You read aloud…" ) );
        }
    }

    if( !continuous ||
    !std::ranges::all_of( learners, [&]( const std::pair<npc *, std::string> &elem ) {
    return std::count( activity->values.begin(), activity->values.end(),
                       elem.first->getID().get_value() ) != 0;
    } ) ||
    !std::all_of( activity->values.begin(), activity->values.end(), [&]( int elem ) {
        return learners.contains( g->find_npc( character_id( elem ) ) );
    } ) ) {

        if( learners.size() == 1 ) {
            add_msg( m_info, _( "%s studies with you." ), learners.begin()->first->disp_name() );
        } else if( !learners.empty() ) {
            const std::string them = enumerate_as_string( learners.begin(),
            learners.end(), [&]( const std::pair<npc *, std::string> &elem ) {
                return elem.first->disp_name();
            } );
            add_msg( m_info, _( "%s study with you." ), them );
        }

        // Don't include the reader as it would be too redundant.
        std::set<std::string> readers;
        for( const auto &elem : fun_learners ) {
            if( elem.first != reader ) {
                readers.insert( elem.first->disp_name() );
            }
        }
        if( readers.size() == 1 ) {
            add_msg( m_info, _( "%s reads with you for fun." ), readers.begin()->c_str() );
        } else if( !readers.empty() ) {
            const std::string them = enumerate_as_string( readers );
            add_msg( m_info, _( "%s read with you for fun." ), them );
        }
    }

    const float vision_mod = std::min(
                                 character_funcs::fine_detail_vision_mod( *this ),
                                 character_funcs::fine_detail_vision_mod( *reader )
                             );
    if( vision_mod > character_funcs::FINE_VISION_PERFECT ) {
        add_msg( m_warning,
                 _( "It's difficult for %s to see fine details right now.  Reading will take longer than usual." ),
                 reader->disp_name() );
    }

    const int intelligence = get_int();
    const bool complex_penalty = type->intel > std::min( intelligence, reader->get_int() ) &&
                                 !reader->has_trait( trait_PROF_DICEMASTER );
    const auto *complex_player = reader->get_int() < intelligence ? reader : this;
    if( complex_penalty && !continuous ) {
        add_msg( m_warning,
                 _( "This book is too complex for %s to easily understand.  It will take longer to read." ),
                 complex_player->disp_name() );
    }

    // push an indentifier of martial art book to the action handling
    if( it.type->use_methods.contains( "MA_MANUAL" ) ) {

        if( get_stamina() < get_stamina_max() / 10 ) {
            add_msg( m_info, _( "You are too exhausted to train martial arts." ) );
            return false;
        }
        act.str_values.clear();
        act.str_values.emplace_back( "martial_art" );
    }

    assign_activity( std::make_unique<player_activity>( std::move( act ) ) ) ;

    // Reinforce any existing morale bonus/penalty, so it doesn't decay
    // away while you read more.
    const time_duration decay_start = 1_turns * time_taken / 1000;
    std::set<Character *> apply_morale = { this };
    for( const auto &elem : learners ) {
        apply_morale.insert( elem.first );
    }
    for( const auto &elem : fun_learners ) {
        apply_morale.insert( elem.first );
    }
    for( Character *ch : apply_morale ) {
        int fun = character_funcs::get_book_fun_for( *ch, it );
        ch->add_morale( MORALE_BOOK, 0, fun * 15, decay_start + 30_minutes,
                        decay_start, false, it.type );
    }

    return true;
}

void avatar::grab( object_type grab_type, const tripoint &grab_point )
{
    this->grab_type = grab_type;
    this->grab_point = grab_point;

    path_settings->avoid_rough_terrain = grab_type != OBJECT_NONE;
}

object_type avatar::get_grab_type() const
{
    return grab_type;
}

static void skim_book_msg( const item &book, avatar &u )
{
    if( !book.type->book ) {
        return;
    }
    const auto &reading = book.type->book;
    const skill_id &skill = reading->skill;

    if( skill && u.get_skill_level_object( skill ).can_train() ) {
        add_msg( m_info, _( "Can bring your %s skill to %d." ),
                 skill.obj().name(), reading->level );
        if( reading->req != 0 ) {
            add_msg( m_info, _( "Requires %s level %d to understand." ),
                     skill.obj().name(), reading->req );
        }
    }

    if( reading->intel != 0 ) {
        add_msg( m_info, _( "Requires intelligence of %d to easily read." ), reading->intel );
    }
    //It feels wrong to use a pointer to *this, but I can't find any other player pointers in this method.
    if( character_funcs::get_book_fun_for( u, book ) != 0 ) {
        add_msg( m_info, _( "Reading this book affects your morale by %d" ),
                 character_funcs::get_book_fun_for( u, book ) );
    }

    if( book.type->use_methods.contains( "MA_MANUAL" ) ) {
        const matype_id style_to_learn = martial_art_learned_from( *book.type );
        add_msg( m_info, _( "You can learn %s style from it." ), style_to_learn->name );
        add_msg( m_info, _( "This fighting style is %s to learn." ),
                 martialart_difficulty( style_to_learn ) );
        add_msg( m_info, _( "It would be easier to master if you'd have skill expertise in %s." ),
                 style_to_learn->primary_skill->name() );
        add_msg( m_info, _( "A training session with this book takes %s" ),
                 to_string( time_duration::from_minutes( reading->time ) ) );
    } else {
        add_msg( m_info, vgettext( "A chapter of this book takes %d minute to read.",
                                   "A chapter of this book takes %d minutes to read.", reading->time ),
                 reading->time );
    }

    std::vector<std::string> recipe_list;
    for( const auto &elem : reading->recipes ) {
        // If the player knows it, they recognize it even if it's not clearly stated.
        if( elem.is_hidden() && !u.knows_recipe( elem.recipe ) ) {
            continue;
        }
        recipe_list.push_back( elem.name.translated() );
    }
    if( !recipe_list.empty() ) {
        std::string recipe_line =
            string_format( vgettext( "This book contains %1$zu crafting recipe: %2$s",
                                     "This book contains %1$zu crafting recipes: %2$s",
                                     recipe_list.size() ),
                           recipe_list.size(),
                           enumerate_as_string( recipe_list ) );
        add_msg( m_info, recipe_line );
    }
    if( recipe_list.size() != reading->recipes.size() ) {
        add_msg( m_info, _( "It might help you figuring out some more recipes." ) );
    }

    add_msg( _( "You note that you have a copy of %s in your possession." ), book.type_name() );
}

void avatar::do_read( item *loc )
{
    if( !loc ) {
        activity->set_to_null();
        return;
    }

    item &book = *loc;
    const auto &reading = book.type->book;
    if( !reading ) {
        activity->set_to_null();
        return;
    }
    const skill_id &skill = reading->skill;

    if( !has_identified( book.typeId() ) ) {
        // Note that we've read the book.
        items_identified.insert( book.typeId() );
        skim_book_msg( book, *this );
        activity->set_to_null();
        return;
    }

    const bool allow_recipes = get_option<bool>( "ALLOW_LEARNING_BOOK_RECIPES" );

    //learners and their penalties
    std::vector<std::pair<player *, double>> learners;
    for( size_t i = 0; i < activity->values.size(); i++ ) {
        player *n = g->find_npc( character_id( activity->values[i] ) );
        if( n != nullptr ) {
            const std::string &s = activity->get_str_value( i, "1" );
            learners.emplace_back( n, strtod( s.c_str(), nullptr ) );
        }
        // Otherwise they must have died/teleported or something
    }
    learners.emplace_back( this, 1.0 );
    //whether to continue reading or not
    bool continuous = false;
    // NPCs who learned a little about the skill
    std::set<std::string> little_learned;
    std::set<std::string> cant_learn;
    std::list<std::string> out_of_chapters;

    for( auto &elem : learners ) {
        player *learner = elem.first;

        int book_fun_for = character_funcs::get_book_fun_for( *learner, book );
        if( book_fun_for != 0 ) {
            learner->add_morale( MORALE_BOOK, book_fun_for * 5, book_fun_for * 15, 1_hours, 30_minutes, true,
                                 book.type );
        }

        book.mark_chapter_as_read( *learner );

        const auto available_recipes = book.get_available_recipes( *learner );
        std::vector<const recipe *> learnable_recipes;
        for( const std::pair<const recipe *, int> &p : available_recipes ) {
            if( allow_recipes && !learner->knows_recipe( p.first ) ) {
                learnable_recipes.push_back( p.first );
                learner->learn_recipe( p.first );
                if( learner->is_player() ) {
                    add_msg( m_info, _( "You memorize a recipe for %s." ), p.first->result_name() );
                }
            }
        }

        if( skill && learner->get_skill_level( skill ) < reading->level &&
            learner->get_skill_level_object( skill ).can_train() ) {
            SkillLevel &skill_level = learner->get_skill_level_object( skill );
            const int originalSkillLevel = skill_level.level();

            // Calculate experience gained
            /** @EFFECT_INT increases reading comprehension */
            // Enhanced Memory Banks modestly boosts experience
            int min_ex = std::max( 1, reading->time / 10 + learner->get_int() / 4 );
            int max_ex = reading->time /  5 + learner->get_int() / 2 - originalSkillLevel;
            if( has_active_bionic( bio_memory ) ) {
                min_ex += 2;
            }

            min_ex = adjust_for_focus( min_ex );
            max_ex = adjust_for_focus( max_ex );

            if( max_ex < 2 ) {
                max_ex = 2;
            }
            if( max_ex > 10 ) {
                max_ex = 10;
            }
            if( max_ex < min_ex ) {
                max_ex = min_ex;
            }

            min_ex *= ( originalSkillLevel + 1 ) * elem.second;
            min_ex = std::max( min_ex, 1 );
            max_ex *= ( originalSkillLevel + 1 ) * elem.second;
            max_ex = std::max( min_ex, max_ex );

            skill_level.readBook( min_ex, max_ex, reading->level );

            std::string skill_name = skill.obj().name();

            if( skill_level != originalSkillLevel ) {
                g->events().send<event_type::gains_skill_level>(
                    learner->getID(), skill, skill_level.level() );
                if( learner->is_player() ) {
                    add_msg( m_good, _( "You increase %s to level %d." ), skill.obj().name(),
                             originalSkillLevel + 1 );
                } else {
                    add_msg( m_good, _( "%s increases their %s level." ), learner->disp_name(), skill_name );
                }
            } else {
                //skill_level == originalSkillLevel
                if( activity->index == learner->getID().get_value() ) {
                    continuous = true;
                }
                if( learner->is_player() ) {
                    add_msg( m_info, _( "You learn a little about %s!  (%d%%)" ), skill_name, skill_level.exercise() );
                } else {
                    little_learned.insert( learner->disp_name() );
                }
            }

            if( learnable_recipes.empty() && ( skill_level == reading->level || !skill_level.can_train() ) ) {
                if( learner->is_player() ) {
                    add_msg( m_info, _( "You can no longer learn from %s." ), book.type_name() );
                } else {
                    cant_learn.insert( learner->disp_name() );
                }
            }
        } else if( learnable_recipes.empty() && skill ) {
            if( learner->is_player() ) {
                add_msg( m_info, _( "You can no longer learn from %s." ), book.type_name() );
            } else {
                cant_learn.insert( learner->disp_name() );
            }
        }
    }
    //end for all learners

    if( little_learned.size() == 1 ) {
        add_msg( m_info, _( "%s learns a little about %s!" ), little_learned.begin()->c_str(),
                 skill.obj().name() );
    } else if( !little_learned.empty() ) {
        const std::string little_learned_msg = enumerate_as_string( little_learned );
        add_msg( m_info, _( "%s learn a little about %s!" ), little_learned_msg, skill.obj().name() );
    }

    if( !cant_learn.empty() ) {
        const std::string names = enumerate_as_string( cant_learn );
        add_msg( m_info, _( "%s can no longer learn from %s." ), names, book.type_name() );
    }
    if( !out_of_chapters.empty() ) {
        const std::string names = enumerate_as_string( out_of_chapters );
        add_msg( m_info, _( "Rereading the %s isn't as much fun for %s." ),
                 book.type_name(), names );
        if( out_of_chapters.front() == disp_name() && one_in( 6 ) ) {
            add_msg( m_info, _( "Maybe you should find something new to read…" ) );
        }
    }

    // NPCs can't learn martial arts from manuals (yet).
    auto m = book.type->use_methods.find( "MA_MANUAL" );
    if( m != book.type->use_methods.end() ) {
        const matype_id style_to_learn = martial_art_learned_from( *book.type );
        skill_id skill_used = style_to_learn->primary_skill;
        int difficulty = std::max( 1, style_to_learn->learn_difficulty );
        difficulty = std::max( 1, 20 + difficulty * 2 - get_skill_level( skill_used ) * 2 );
        add_msg( m_debug, _( "Chance to learn one in: %d" ), difficulty );

        if( one_in( difficulty ) ) {
            m->second.call( *this, book, false, pos() );
            continuous = false;
        } else {
            if( activity->index == getID().get_value() ) {
                continuous = true;
                switch( rng( 1, 5 ) ) {
                    case 1:
                        add_msg( m_info,
                                 _( "You train the moves according to the book, but can't get a grasp of the style, so you start from the beginning." ) );
                        break;
                    case 2:
                        add_msg( m_info,
                                 _( "This martial art is not easy to grasp.  You start training the moves from the beginning." ) );
                        break;
                    case 3:
                        add_msg( m_info,
                                 _( "You decide to read the manual and train even more.  In martial arts, patience leads to mastery." ) );
                        break;
                    case 4:
                    case 5:
                        add_msg( m_info, _( "You try again.  This training will finally pay off." ) );
                        break;
                }
            } else {
                add_msg( m_info, _( "You train for a while." ) );
            }
        }
    }

    if( continuous ) {
        activity->set_to_null();
        read( loc, true );
        if( activity ) {
            return;
        }
    }

    activity->set_to_null();
}

bool avatar::has_identified( const itype_id &item_id ) const
{
    return items_identified.contains( item_id );
}

void avatar::wake_up()
{
    if( has_effect( effect_sleep ) ) {
        if( calendar::turn - get_effect( effect_sleep ).get_start_time() > 2_hours ) {
            print_health();
        }
        // alarm was set and player hasn't slept through the alarm.
        if( has_effect( effect_alarm_clock ) && !has_effect( effect_slept_through_alarm ) ) {
            add_msg( _( "It looks like you woke up before your alarm." ) );
            remove_effect( effect_alarm_clock );
        } else if( has_effect( effect_slept_through_alarm ) ) {
            if( has_bionic( bio_infolink ) ) {
                add_msg( m_warning, _( "It looks like you've slept through your internal alarm…" ) );
            } else {
                add_msg( m_warning, _( "It looks like you've slept through the alarm…" ) );
            }
        }
    }
    Character::wake_up();
}

void avatar::add_snippet( snippet_id snippet )
{
    // Optional: caller can check !has_seen_snippet(snippet) before calling this
    // to avoid doing unnecessary work. This function is safe to call multiple times:
    // set_value() and emplace() won't change anything if the snippet was already added.
    std::string combined_name = "has_seen_snippet_" + snippet.str();
    get_avatar().set_value( combined_name, "true" );
    snippets_read.emplace( snippet );
}
bool avatar::has_seen_snippet( const snippet_id &snippet ) const
{
    return snippets_read.contains( snippet );
}

const std::set<snippet_id> &avatar::get_snippets()
{
    return snippets_read;
}

void avatar::vomit()
{
    if( stomach.get_calories() > 0 ) {
        // Remove all joy from previously eaten food and apply the penalty
        rem_morale( MORALE_FOOD_GOOD );
        rem_morale( MORALE_FOOD_HOT );
        rem_morale( MORALE_FOOD_COLD );
        rem_morale( MORALE_FOOD_VERY_COLD );
        // bears must suffer too
        rem_morale( MORALE_HONEY );
        // 1.5 times longer
        add_morale( MORALE_VOMITED, -20, -40, 90_minutes, 45_minutes, false );

    } else {
        add_msg( m_warning, _( "You retched, but your stomach is empty." ) );
    }
    Character::vomit();
}

bool avatar::is_hallucination() const
{
    return false;
}

bool avatar::is_dead_state() const
{
    if( cached_dead_state.has_value() ) {
        return cached_dead_state.value();
    }

    if( Character::is_dead_state() ) {
        cata::run_hooks( "on_character_death", [ &, this]( auto & params ) {
            params["char"] = this;
        } );
        cached_dead_state.reset();
    }

    return Character::is_dead_state();
}

void avatar::disp_morale()
{
    int equilibrium = character_effects::calc_focus_equilibrium( *this );

    int fatigue_cap = character_effects::calc_morale_fatigue_cap( this->get_fatigue() );

    int pain_penalty = has_trait( trait_CENOBITE ) ? 0 : get_perceived_pain();

    morale->display( equilibrium, pain_penalty, fatigue_cap );
}

int avatar::get_str_base() const
{
    return Character::get_str_base() + std::max( 0, str_upgrade );
}

int avatar::get_dex_base() const
{
    return Character::get_dex_base() + std::max( 0, dex_upgrade );
}

int avatar::get_int_base() const
{
    return Character::get_int_base() + std::max( 0, int_upgrade );
}

int avatar::get_per_base() const
{
    return Character::get_per_base() + std::max( 0, per_upgrade );
}

int avatar::kill_xp() const
{
    return g->get_kill_tracker().kill_xp();
}

static int xp_cutoffs( unsigned int level )
{
    int coeff = get_option<int>( "AVATAR_LEVEL_XP_COEFF" );
    // In nicer terms, this is a quadratic
    return ( pow( level, 2 ) * coeff );
}

int avatar::free_upgrade_points() const
{
    const int xp = kill_xp();
    int lvl = 0;
    bool found_level = false;
    while( !found_level ) {
        if( xp >= xp_cutoffs( lvl + 1 ) ) {
            lvl++;
        } else {
            found_level = true;
        }
    }
    return lvl - str_upgrade - dex_upgrade - int_upgrade - per_upgrade;
}

int avatar::kill_xp_for_next_point() const
{
    const int xp_gained = kill_xp();
    int level = 0;
    bool found_higher = false;
    while( !found_higher ) {
        level++;
        if( xp_gained < xp_cutoffs( level ) ) {
            found_higher = true;
        }
    }
    return xp_cutoffs( level ) - xp_gained;
}

void avatar::upgrade_stat( character_stat stat )
{
    switch( stat ) {
        case character_stat::STRENGTH:
            str_upgrade++;
            break;
        case character_stat::DEXTERITY:
            dex_upgrade++;
            break;
        case character_stat::INTELLIGENCE:
            int_upgrade++;
            break;
        case character_stat::PERCEPTION:
            per_upgrade++;
            break;
        case character_stat::DUMMY_STAT:
            debugmsg( "Tried to use invalid stat" );
            break;
    }
    recalc_hp();
}

faction *avatar::get_faction() const
{
    return g->faction_manager_ptr->get( faction_id( "your_followers" ) );
}

void avatar::set_movement_mode( character_movemode new_mode )
{
    switch( new_mode ) {
        case CMM_WALK: {
            if( is_mounted() ) {
                if( mounted_creature->has_flag( MF_RIDEABLE_MECH ) ) {
                    add_msg( _( "You set your mech's leg power to a loping fast walk." ) );
                } else {
                    add_msg( _( "You nudge your steed into a steady trot." ) );
                }
            } else {
                // Spend moves to stand up if crouched, otherwise just stop running.
                if( move_mode == CMM_CROUCH ) {
                    mod_moves( -100 );
                    recoil = MAX_RECOIL;
                    add_msg( _( "You stand up." ) );
                } else {
                    add_msg( _( "You start walking." ) );
                }
            }
            break;
        }
        case CMM_RUN: {
            if( can_run() ) {
                if( is_hauling() ) {
                    stop_hauling();
                }
                if( is_mounted() ) {
                    if( mounted_creature->has_flag( MF_RIDEABLE_MECH ) ) {
                        add_msg( _( "You set the power of your mech's leg servos to maximum." ) );
                    } else {
                        add_msg( _( "You spur your steed into a gallop." ) );
                    }
                } else {
                    // Spend moves to stand up if crouched, otherwise just stop running.
                    if( move_mode == CMM_CROUCH ) {
                        mod_moves( -100 );
                        recoil = MAX_RECOIL;
                        add_msg( _( "You stand up and start running." ) );
                    } else {
                        add_msg( _( "You start running." ) );
                    }
                }
            } else {
                if( is_mounted() ) {
                    // mounts don't currently have stamina, but may do in future.
                    add_msg( m_bad, _( "Your steed is too tired to go faster." ) );
                } else if( get_working_leg_count() < 2 ) {
                    add_msg( m_bad, _( "You need two functional legs to run." ) );
                } else {
                    add_msg( m_bad, _( "You're too tired to run." ) );
                }
                return;
            }
            break;
        }
        case CMM_CROUCH: {
            if( is_mounted() ) {
                if( mounted_creature->has_flag( MF_RIDEABLE_MECH ) ) {
                    add_msg( _( "You reduce the power of your mech's leg servos to minimum." ) );
                } else {
                    add_msg( _( "You slow your steed to a walk." ) );
                }
            } else {
                // Don't spend moves if we were already crouching.
                if( move_mode != CMM_CROUCH ) {
                    recoil = MAX_RECOIL;
                    mod_moves( -100 );
                }
                add_msg( _( "You start crouching." ) );
            }
            break;
        }
        default: {
            return;
        }
    }
    if( move_mode == CMM_CROUCH || new_mode == CMM_CROUCH ) {
        // crouching affects visibility
        get_map().set_seen_cache_dirty( pos().z );
    }
    move_mode = new_mode;
}

void avatar::toggle_run_mode()
{
    if( move_mode == CMM_RUN ) {
        set_movement_mode( CMM_WALK );
    } else {
        set_movement_mode( CMM_RUN );
    }
}

void avatar::toggle_crouch_mode()
{
    if( move_mode == CMM_CROUCH ) {
        set_movement_mode( CMM_WALK );
    } else {
        set_movement_mode( CMM_CROUCH );
    }
}

void avatar::reset_move_mode()
{
    if( move_mode != CMM_WALK ) {
        set_movement_mode( CMM_WALK );
    }
}

void avatar::cycle_move_mode()
{
    unsigned char as_uchar = static_cast<unsigned char>( move_mode );
    as_uchar = ( as_uchar + 1 + CMM_COUNT ) % CMM_COUNT;
    set_movement_mode( static_cast<character_movemode>( as_uchar ) );
    // if a movemode is disabled then just cycle to the next one
    if( !movement_mode_is( static_cast<character_movemode>( as_uchar ) ) ) {
        as_uchar = ( as_uchar + 1 + CMM_COUNT ) % CMM_COUNT;
        set_movement_mode( static_cast<character_movemode>( as_uchar ) );
    }
}

bool avatar::wield( item &target )
{
    if( is_wielding( target ) ) {
        return true;
    }

    if( !can_wield( target ).success() ) {
        return false;
    }

    if( !unwield() ) {
        return false;
    }
    clear_npc_ai_info_cache( npc_ai_info::ideal_weapon_value );
    if( target.is_null() ) {
        return true;
    }

    // Query whether to draw an item from a holster when attempting to wield the holster
    if( target.get_use( "holster" ) && !target.contents.empty() ) {
        //~ %1$s: weapon name, %2$s: holster name
        if( query_yn( pgettext( "holster", "Draw %1$s from %2$s?" ), target.get_contained().tname(),
                      target.tname() ) ) {
            invoke_item( &target );
            return false;
        }
    }

    // Wielding from inventory is relatively slow and does not improve with increasing weapon skill.
    // Worn items (including guns with shoulder straps) are faster but still slower
    // than a skilled player with a holster.
    // There is an additional penalty when wielding items from the inventory whilst currently grabbed.

    bool worn = is_worn( target );
    int mv = item_handling_cost( target, true,
                                 worn ? INVENTORY_HANDLING_PENALTY / 2 : INVENTORY_HANDLING_PENALTY );

    add_msg( m_debug, "wielding took %d moves", mv );
    moves -= mv;

    set_primary_weapon( target.detach() );

    last_item = target.typeId();
    recoil = MAX_RECOIL;

    target.on_wield( *this, mv );

    inv.update_invlet( target );
    inv.update_invlet_cache_with_item( target );

    return true;
}


detached_ptr<item> avatar::wield( detached_ptr<item> &&target )
{
    if( !can_wield( *target ).success() ) {
        return std::move( target );
    }

    if( !unwield() ) {
        return std::move( target );
    }
    clear_npc_ai_info_cache( npc_ai_info::ideal_weapon_value );
    if( !target || target->is_null() ) {
        return std::move( target );
    }
    item &obj = *target;
    set_primary_weapon( std::move( target ) );

    last_item = obj.typeId();
    recoil = MAX_RECOIL;
    int mv = item_handling_cost( obj, true, INVENTORY_HANDLING_PENALTY );
    obj.on_wield( *this, mv );


    inv.update_invlet( obj );
    inv.update_invlet_cache_with_item( obj );
    return detached_ptr<item>();
}

bool avatar::invoke_item( item *used, const tripoint &pt )
{
    std::map<std::string, use_function> use_methods;
    use_methods.insert( used->type->use_methods.begin(), used->type->use_methods.end() );
    // Make sure the flag is that of the mod, not of the item being used :P
    if( used->has_flag( flag_ADD_UPS_TOGGLE ) && !used->type->has_flag( flag_ADD_UPS_TOGGLE ) ) {
        use_methods["TOGGLE_UPS_CHARGING"] = item_controller->usage_from_string( "TOGGLE_UPS_CHARGING" );
    }
    if( use_methods.empty() ) {
        return false;
    } else if( use_methods.size() == 1 ) {
        return invoke_item( used, use_methods.begin()->first, pt );
    }

    uilist umenu;

    umenu.text = string_format( _( "What to do with your %s?" ), used->tname() );
    umenu.hilight_disabled = true;

    for( const auto &e : use_methods ) {
        const auto res = e.second.can_call( *this, *used, false, pt );
        umenu.addentry_desc( MENU_AUTOASSIGN, res.success(), MENU_AUTOASSIGN, e.second.get_name(),
                             res.str() );
    }

    umenu.desc_enabled = std::ranges::any_of( umenu.entries,
    []( const uilist_entry & elem ) {
        return !elem.desc.empty();
    } );

    umenu.query();

    int choice = umenu.ret;
    if( choice < 0 || choice >= static_cast<int>( use_methods.size() ) ) {
        return false;
    }

    const std::string &method = std::next( use_methods.begin(), choice )->first;

    return invoke_item( used, method, pt );
}

bool avatar::invoke_item( item *used )
{
    return Character::invoke_item( used );
}

bool avatar::invoke_item( item *used, const std::string &method, const tripoint &pt )
{
    return Character::invoke_item( used, method, pt );
}

bool avatar::invoke_item( item *used, const std::string &method )
{
    return Character::invoke_item( used, method );
}

bool avatar::add_faction_warning( const faction_id &id )
{
    const auto it = warning_record.find( id );
    if( it != warning_record.end() ) {
        it->second.first += 1;
        if( it->second.second - calendar::turn > 5_minutes ) {
            it->second.first -= 1;
        }
        it->second.second = calendar::turn;
        if( it->second.first > 3 ) {
            return true;
        }
    } else {
        warning_record[id] = std::make_pair( 1, calendar::turn );
    }
    faction *fac = g->faction_manager_ptr->get( id );
    if( fac != nullptr && is_player() && fac->id != faction_id( "no_faction" ) ) {
        fac->likes_u -= 1;
        fac->respects_u -= 1;
    }
    return false;
}
