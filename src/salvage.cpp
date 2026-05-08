#include "activity_actor_definitions.h"
#include "salvage.h"

#include <set>
#include <unordered_map>
#include <vector>

#include "activity_speed.h"
#include "character.h"
#include "flag.h"
#include "game_inventory.h"
#include "item.h"
#include "itype.h"
#include "json.h"
#include "map.h"
#include "material.h"
#include "messages.h"
#include "options.h"
#include "output.h"
#include "player.h"
#include "player_activity.h"
#include "popup.h"
#include "recipe_dictionary.h"
#include "type_id.h"
#include "ui_manager.h"

static const skill_id skill_fabrication( "fabrication" );

namespace salvage
{
std::unordered_map<material_id, std::set<quality_id>> salvage_material_quality_dictionary;
std::set<material_id> all_salvagable_materials;

// Helper to find smallest sub-component of an item.
units::mass minimal_weight_to_cut( const item &it )
{
    units::mass min_weight = units::mass_max;

    for( const material_id &material : it.made_of() ) {
        if( const std::optional<itype_id> id = material->salvaged_into() ) {
            min_weight = std::min( min_weight, ( *id )->weight );
        }
    }
    return min_weight;
}

static q_result yn_ignore_query( const std::string &text )
{
    const bool force_uc = get_option<bool>( "FORCE_CAPITAL_YN" );
    const auto &allow_key = force_uc
                            ? input_context::disallow_lower_case
                            : input_context::allow_all_keys;

    const auto &action = query_popup()
                         .context( "YN_IGNORE_QUERY" )
                         .message( force_uc
                                   ? pgettext( "YN_IGNORE_QUERY",
                                           "<color_light_red>%s (Case Sensitive)</color>" )
                                   : pgettext( "YN_IGNORE_QUERY",
                                           "<color_light_red>%s</color>" ),
                                   text )
                         .option( "YES", allow_key )
                         .option( "SKIP", allow_key )
                         .option( "ABORT", allow_key )
                         .option( "IGNORE", allow_key )
                         .query()
                         .action;

    ui_manager::redraw();
    refresh_display();

    if( action == "YES" ) {
        return q_result::yes;
    }
    if( action == "IGNORE" ) {
        return q_result::ignore;
    }
    if( action == "SKIP" ) {
        return q_result::skip;
    }
    return q_result::abort;
}


// It is used to check if an item can be salvaged or not.
ret_val<bool> try_salvage( const item &target, quality_cache &q_cache )
{
    if( target.is_null() ) {
        return ret_val<bool>::make_failure( _( "You do not have that item." ) );
    }
    // There must be some historical significance to these items.
    if( !target.is_salvageable() ) {
        auto ret = string_format( _( "Can't salvage anything from %s.\n" ), target.tname() );
        if( recipe_dictionary::get_uncraft( target.typeId() ) ) {
            ret += string_format( _( "Try disassembling the %s instead." ), target.tname() );
        } else if( !target.only_made_of( all_salvagable_materials ) ) {
            ret += string_format( _( "The %s is made entirely of material that cannot be salvaged up." ),
                                  target.tname() );
        }
        return ret_val<bool>::make_failure( ret );
    }
    if( !target.contents.empty() ) {
        return ret_val<bool>::make_failure(
                   string_format( _( "Please empty the %s before salvaged it up." ), target.tname() ) );
    }
    if( target.weight() < minimal_weight_to_cut( target ) ) {
        return ret_val<bool>::make_failure(
                   string_format( _( "The %s is too small to salvage any material from." ), target.tname() ) );
    }
    if( !has_salvage_tools( q_cache, target ) ) {
        return ret_val<bool>::make_failure(
                   string_format(
                       _( "You lack proper tools to salvage any material from the %s." ),
                       target.tname() ) );
    }
    return ret_val<bool>::make_success();
}

static q_result prompt_warnings( const Character &who, const item &target,
                                 quality_cache &q_cache )
{
    auto &madeof = target.made_of();

    for( auto &mat : madeof ) {
        auto res = mat->salvaged_into();
        // do not promt this if resulting material will provide 0 items
        if( res && res.value()->weight <= target.weight() / static_cast<int16_t>( madeof.size() )
            && !has_salvage_tools( q_cache, mat ) ) {
            auto result = yn_ignore_query(
                              string_format(
                                  _( "You lack proper tools to salvage %s from the %s, meaning no output for this material.  Salvage anyway?" ),
                                  mat->name(), target.tname() ) );
            if( result != q_result::yes ) {
                return result;
            }
        }
    }

    if( who.is_wielding( target ) ) {
        auto result = yn_ignore_query( _( "You are wielding that, salvage anyway?" ) );
        if( result != q_result::yes ) {
            return result;
        }
    } else if( who.is_wearing( target ) ) {
        auto result = yn_ignore_query( _( "You're wearing that, salvage anyway?" ) );
        if( result != q_result::yes ) {
            return result;
        }
    }
    if( target.is_favorite ) {
        auto result = yn_ignore_query( _( "This item is marked as favorite, salvage anyway?" ) );
        if( result != q_result::yes ) {
            return result;
        }
    }
    if( recipe_dictionary::get_uncraft( target.typeId() ) ) {
        auto result = yn_ignore_query( _( "This item could be disassembled instead, salvage anyway?" ) );
        if( result != q_result::yes ) {
            return result;
        }
    }
    return q_result::yes;
}

//Returns vector of pairs <material, fraction>, where fraction = [0.0f, 1.0f]
static std::vector<std::pair< material_id, float>> salvage_result_proportions(
            const item &target )
{
    auto &materials = target.made_of();
    std::vector<std::pair< material_id, float>> salvagable_materials;
    //For now we assume that proportions for all materials are equal
    for( auto &material : materials ) {
        salvagable_materials.emplace_back( material, 1.0f / materials.size() );
    }
    return salvagable_materials;
}

std::unordered_map< itype_id, float> salvage_results( const item &target )
{
    std::unordered_map<itype_id, float> salvagable_materials;
    //For now we assume that proportions for all materials are equal
    for( auto &material : salvage_result_proportions( target ) ) {
        auto res = material.first->salvaged_into();
        if( res && all_salvagable_materials.contains( material.first ) ) {
            //Simplier to debug
            auto t_mass = target.weight().value();
            auto r_mass = ( **res ).weight.value();
            //cuz we need actual float here
            salvagable_materials[*res] += t_mass * material.second / r_mass;
        }
    }
    return salvagable_materials;
}

void complete_salvage( Character &who, item &cut, tripoint_abs_ms pos )
{
    float salvagable_percent = 1.0f;
    // Chance of us losing a material component to entropy.
    /** @EFFECT_FABRICATION reduces chance of losing components when cutting items up */
    int entropy_threshold = std::max( 5, 10 - who.get_skill_level( skill_fabrication ) );
    // Not much practice, and you won't get very far ripping things up.
    who.practice( skill_fabrication, rng( 0, 5 ), 1 );
    // Higher fabrication, less chance of entropy, but still a chance.
    if( rng( 1, 10 ) <= entropy_threshold ) {
        salvagable_percent *= 0.95f;
    }
    // Fail dex roll, potentially lose more parts.
    /** @EFFECT_DEX randomly reduces component loss when cutting items up */
    if( dice( 3, 4 ) > who.dex_cur ) {
        salvagable_percent *= 0.95f;
    }
    // If more than 1 material component can still be salvaged,
    // chance of losing more components if the item is damaged.
    // If the item being cut is not damaged, no additional losses will be incurred.
    if( cut.damage() > 0 ) {
        float component_success_chance = std::min( std::pow( 0.8, cut.damage_level( 4 ) ),
                                         1.0 );
        salvagable_percent *= component_success_chance;
    }

    add_msg( m_info, _( "You try to salvage materials from the %s." ), cut.tname() );

    // Clean up before removing the item.
    remove_ammo( cut, who );
    // Original item has been consumed.
    cut.detach();
    // Force an encumbrance update in case they were wearing that item.
    who.reset_encumbrance();

    map &here = get_map();
    auto pos_here = here.getlocal( pos );

    for( const auto &salvaged : salvage_results( cut ) ) {
        int amount = std::floor( salvagable_percent * salvaged.second );
        if( amount > 0 ) {
            // Time based on number of components.
            add_msg( m_good, vgettext( "Salvaged %1$i %2$s.", "Salvaged %1$i %2$s.", amount ),
                     amount, salvaged.first->nname( amount ) );
            // Done this way so that items with charges > 1 or no support for charges work correctly
            here.spawn_item( pos_here, salvaged.first, amount, 1 );
        } else {
            add_msg( m_bad, _( "Could not salvage a %s." ), salvaged.first->nname( 1 ) );
        }

    }
}

//Returns number of moves needed to salvage item
int moves_to_salvage( const item &target )
{
    int time = 0;
    for( auto &material : salvage_result_proportions( target ) ) {
        if( material.first && all_salvagable_materials.contains( material.first ) ) {
            //based on density, weight and proportion of material
            auto w = units::to_milligram( target.weight() );
            time += material.first->density() * material.second * w / 10000.0f;
        }
    }
    return time;
}

static void remove_qualities( quality_cache &q_cache, const item &item )
{
    for( auto &q : item.get_qualities() ) {
        if( q_cache[q.first][q.second] <= 1 ) {
            q_cache[q.first].erase( q.second );
        } else {
            q_cache[q.first][q.second]--;
        }
    }
}

//Checks if inventory has tools to salvage material
bool has_salvage_tools( quality_cache &q_cache, const material_id &material )
{
    auto it = salvage_material_quality_dictionary.find( material->id );
    if( it != salvage_material_quality_dictionary.end() ) {
        for( auto &quality : it->second ) {
            if( !q_cache[quality].empty() ) {
                return true;
            }
        }
    }
    return false;
}

//Checks if inventory has tools to salvage an item
//strict = false - check if atleast one material is salvagable with current tools
//strict = true - check all materials
bool has_salvage_tools( quality_cache &q_cache, const item &item,
                        bool strict )
{
    //we don't want to try and salvage item with itself
    remove_qualities( q_cache, item );

    for( auto &material : item.made_of() ) {
        if( has_salvage_tools( q_cache, material ) ) {
            return true;
        } else if( strict ) {
            return false;
        }
    }
    return false;
}

bool menu_salvage_single( player &you )
{
    item *target = game_menus::inv::salvage( you );
    if( target ) {
        return prompt_salvage_single( you, *target );
    } else {
        return false;
    }
}

bool prompt_salvage_single( Character &who, item &target )
{
    quality_cache cache = who.crafting_inventory().get_quality_cache();
    if( auto res = try_salvage( target, cache ); !res.success() ) {
        add_msg( m_bad, res.str() );
        return false;
    }

    map &here = get_map();
    std::string msg;
    msg += string_format( _( "Salvaging the %s may yield:\n" ),
                          colorize( target.tname(), target.color_in_inventory() ) );
    const auto components = salvage_results( target );
    for( const auto &component : components ) {
        int c = std::floor( component.second );
        //%1$s: item name, % 2$d :  count
        msg += string_format( " - %1$d %2$s\n", c, component.first->nname( c ) );
    }
    msg += "\n";
    msg += _( "Really salvage?\n" );
    if( !query_yn( msg ) ) {
        add_msg( _( "Never mind." ) );
        return false;
    }

    iuse_location loc( target, 0 );
    who.assign_activity( std::make_unique<player_activity>(
                             std::make_unique<salvage_activity_actor>(
                                 iuse_locations{ loc }, here.getglobal( who.pos() ) ) ) );
    return true;
}

bool salvage_single( Character &who, item &target )
{
    map &here = get_map();
    quality_cache cache = who.crafting_inventory().get_quality_cache();

    if( auto res = try_salvage( target, cache ); !res.success() ) {
        add_msg( m_bad, res.str() );
        return false;
    }

    iuse_location loc( target, 0 );

    who.assign_activity( std::make_unique<player_activity>(
                             std::make_unique<salvage_activity_actor>(
                                 iuse_locations{ loc }, here.getglobal( who.pos() ) ) ) );
    return true;
}

bool salvage_all( Character &who )
{
    map &here = get_map();
    tripoint pos = who.pos();
    std::vector<iuse_location> targets;
    //yes this should NOT be a reference
    quality_cache cache = who.crafting_inventory().get_quality_cache();

    for( auto target : here.i_at( pos ) ) {
        if( target && try_salvage( *target, cache ).success() ) {
            iuse_location loc;
            loc.loc = target;
            targets.push_back( std::move( loc ) );
        }
    }

    if( !targets.empty() ) {
        tripoint_abs_ms pos_abs( here.getabs( pos ) );

        who.assign_activity( std::make_unique<player_activity>
                             ( std::make_unique<salvage_activity_actor>( std::move(
                                         targets ), pos_abs ) ) );
        return true;
    } else {
        return false;
    }
}

void populate_salvage_materials( quality &q )
{
    for( auto &material : q.salvagable_materials ) {
        salvage::salvage_material_quality_dictionary[material].emplace( q.id );
    }
    std::copy( q.salvagable_materials.begin(), q.salvagable_materials.end(),
               std::inserter( all_salvagable_materials, all_salvagable_materials.end() ) );
}

} // namespace salvage


void salvage_activity_actor::calc_all_moves( player_activity &act, Character &who )
{
    const auto &target = targets.front();
    auto reqs = activity_reqs_adapter( get_type()->skills, std::make_pair(
                                           target.loc->weight(), target.loc->volume() ) );

    act.speed.calc_all_moves( who, reqs );
}

void salvage_activity_actor::start( player_activity &act, Character &who )
{
    //yes this should NOT be a reference
    auto cache = who.crafting_inventory().get_quality_cache();
    for( auto &target : targets ) {
        if( !target.loc ) {
            debugmsg( "Lost target of ", get_type() );
        } else {
            if( progress.empty() && !mute_prompts ) {
                switch( salvage::prompt_warnings( who, *target.loc, cache ) ) {
                    case salvage::q_result::ignore:
                        mute_prompts = true;
                        [[fallthrough]];
                    case salvage::q_result::yes:
                        progress.emplace( target.loc->tname(), salvage::moves_to_salvage( *target.loc ) );
                        break;
                    case salvage::q_result::fail:
                    case salvage::q_result::skip:
                        targets.erase( targets.begin() );
                        // If we skipped everything, cancel or we'll crash.
                        if( targets.empty() ) {
                            act.set_to_null();
                            add_msg( _( "Never mind." ) );
                        }
                        break;
                    case salvage::q_result::abort:
                        act.set_to_null();
                        add_msg( _( "Never mind." ) );
                        break;
                    default:
                        break;
                }
            } else {
                progress.emplace( target.loc->tname(), salvage::moves_to_salvage( *target.loc ) );
            }
        }
    }
}

void salvage_activity_actor::do_turn( player_activity &act, Character &who )
{
    if( progress.front().complete() ) {
        auto &target = targets.front();
        if( !target.loc ) {
            debugmsg( "Lost target of ", get_type() );
        } else {
            salvage::complete_salvage( who, *target.loc, pos );
        }
        targets.erase( targets.begin() );
        progress.pop();
    }
    if( !progress.empty() && progress.front().not_started() ) {
        auto &target = targets.front();
        if( !target.loc ) {
            debugmsg( "Lost target of ", get_type() );
            act.set_to_null();
        } else {
            if( !mute_prompts ) {
                //yes this should NOT be a reference
                auto cache = who.crafting_inventory().get_quality_cache();
                switch( salvage::prompt_warnings( who, *target.loc, cache ) ) {
                    case salvage::q_result::ignore:
                        mute_prompts = true;
                        [[fallthrough]];
                    case salvage::q_result::yes:
                        calc_all_moves( act, who );
                        break;
                    case salvage::q_result::fail:
                    case salvage::q_result::skip:
                        targets.erase( targets.begin() );
                        progress.pop();
                        break;
                    case salvage::q_result::abort:
                        act.set_to_null();
                        break;
                    default:
                        break;
                }
            } else {
                calc_all_moves( act, who );
            }
        }
    }
}

void salvage_activity_actor::finish( player_activity &act, Character & )
{
    if( !progress.complete() ) {
        debugmsg( "salvage_activity_actor call finish function while able to start new salvage" );
    }
    add_msg( _( "You finish salvaging." ) );
    act.set_to_null();
}

void salvage_activity_actor::serialize( JsonOut &jsout ) const
{
    jsout.start_object();

    jsout.member( "progress", progress );
    jsout.member( "targets", targets );
    jsout.member( "pos", pos );
    jsout.member( "mute_prompts", mute_prompts );

    jsout.end_object();
}

//If strict == false we check if atleast one material is salvagable
//Else we check all materials
bool item::is_salvageable( bool strict ) const
{
    if( is_null() ) {
        return false;
    }
    for( auto &mat : made_of() ) {
        if( salvage::all_salvagable_materials.contains( mat ) ) {
            if( !strict ) {
                return !has_flag( flag_NO_SALVAGE );
            }
        } else {
            if( strict ) {
                return false;
            }
        }
    }
    return strict;
}
