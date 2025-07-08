#include "activity_speed.h"
#include "activity_speed_adapters.h"

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include "activity_type.h"
#include "character.h"
#include "character_functions.h"
#include "character_stat.h"
#include "game.h"
#include "map.h"
#include "type_id.h"
#include "veh_type.h"
#include "vehicle_part.h"
#include "vpart_position.h"

static const skill_id stat_speech( "speech" );

static const quality_id qual_BUTCHER( "BUTCHER" );
static const quality_id qual_CUT_FINE( "CUT_FINE" );

inline static float limit_factor( float factor, float min = 0.25f, float max = 2.0f )
{
    //constrain speed between min and max
    return clamp<float>( factor, min, max );
}

inline static float refine_factor( float speed, int denom = 1, float min = -75.0f,
                                   float max = 100.0f )
{
    speed = limit_factor( speed, min, max );
    denom = denom < 1.0f
            ? 1.0f
            : denom;
    speed /= denom;

    //speed to factor
    return speed / 100.0f;
}

void activity_speed::calc_moves( const Character &who )
{
    if( type->light_affected() ) {
        calc_light_factor( who );
    }
    if( type->speed_affected() ) {
        player_speed = who.get_speed() / 100.0f;
    }
    if( type->stats_affected() ) {
        calc_stats_factors( who );
    }
    if( type->morale_affected() ) {
        calc_morale_factor( who );
    }
}

void activity_speed::calc_all_moves( Character &who )
{
    if( type->bench_affected() ) {
        calc_bench_factor( who );
    }
    if( type->tools_affected() ) {
        calc_tools_factor( who, type->qualities );
    }
    if( type->skill_affected() ) {
        calc_skill_factor( who, type->skills );
    }
    if( type->assistable() ) {
        calc_assistants_factor( who );
    }
    calc_moves( who );
}

void activity_speed::calc_all_moves( Character &who, activity_reqs_adapter &reqs )
{
    if( type->bench_affected() ) {
        find_best_bench( who.pos(), reqs.metrics );
        calc_bench_factor( who );
    }
    if( type->tools_affected() ) {
        calc_tools_factor( who, reqs.qualities );
    }
    if( type->skill_affected() ) {
        calc_skill_factor( who, reqs.skills );
    }
    if( type->assistable() ) {
        calc_assistants_factor( who );
    }
    calc_moves( who );
}



void activity_speed::calc_light_factor( const Character &who )
{
    if( character_funcs::can_see_fine_details( who ) ) {
        light = 1.0f;
        return;
    }

    // This value whould be within [0,1]
    const float darkness =
        (
            character_funcs::fine_detail_vision_mod( who ) -
            character_funcs::FINE_VISION_THRESHOLD
        ) / 7.0f;
    light = limit_factor( 1.0f - darkness, 0.0f );
}

void activity_speed::calc_skill_factor( const Character &who, const skill_reqs &skill_req )
{
    float ac_f = skills_factor_custom_formula( who, skill_req );
    //Any factor above 0 is valid, else - use default calc
    if( ac_f > 0 ) {
        skills = ac_f;
        return;
    }

    float f = 1.0f;
    std::vector<float> factors;
    for( const auto &skill : skill_req ) {
        int who_eff_skill = who.get_skill_level( skill.req ) - skill.threshold;
        float bonus = 0;
        if( who_eff_skill != 0 ) {
            bonus = 0.02f * std::pow( who_eff_skill, 3 )
                    - 0.5f * std::pow( who_eff_skill, 2 )
                    + 6.0f * who_eff_skill + skill.mod;
        }

        factors.push_back( bonus );
    }
    std::ranges::sort( factors, std::greater<>() );

    int denom = 0;
    for( const auto &factor : factors ) {
        f += refine_factor( factor, ++denom * 0.8f );
    }

    skills = limit_factor( f );
}

std::pair<character_stat, float> activity_speed::calc_single_stat( const Character &who,
        const activity_req<character_stat> &stat )
{
    int who_stat = 0;
    switch( stat.req ) {
        case character_stat::STRENGTH:
            who_stat = who.get_str();
            break;
        case character_stat::DEXTERITY:
            who_stat = who.get_dex();
            break;
        case character_stat::INTELLIGENCE:
            who_stat = who.get_int();
            break;
        case character_stat::PERCEPTION:
            who_stat = who.get_per();
            break;
        default:
            return std::pair<character_stat, float>( character_stat::DUMMY_STAT, 1.0f );
    }
    float f = 1.0f + refine_factor( stat.mod * ( who_stat - stat.threshold ) );
    return std::pair<character_stat, float>( stat.req, f );
}


void activity_speed::calc_assistants_factor( const Character &who )
{
    if( assistant_count == 0 ) {
        assist = 1.0f;
    }

    float f = 0.5f * std::pow( assistant_count, 3 )
              - 7 * std::pow( assistant_count, 2 )
              + 45 * assistant_count;

    // range [0.8:1.2] based on speech
    f *= 0.8f + 0.04f * who.get_skill_level( stat_speech );

    assist = 1.0f + refine_factor( f, 1, 0.0f, 200.0f );
}


void activity_speed::calc_bench_factor( const Character &/*who*/ )
{
    bench_factor = bench
                   ? bench->wb_info.multiplier_adjusted
                   : 1.0f;
}

void activity_speed::calc_stats_factors( const Character &who )
{
    stats = stats_factor_custom_formula( who, type->stats );

    if( stats.empty() ) {
        for( auto &stat : type->stats ) {
            stats.emplace_back( calc_single_stat( who, stat ) );
        }
    }
}

float activity_speed::get_best_qual_mod( const activity_req<quality_id> &q,
        const inventory &inv )
{
    int q_level = 0;
    inv.visit_items( [&q, &q_level]( const item * itm ) {
        int new_q = itm->get_quality( q.req );
        if( new_q > q_level ) {
            q_level = new_q;
        }
        return VisitResponse::NEXT;
    } );
    q_level = q_level - q.threshold;

    if( q.req == qual_CUT_FINE ) {
        float cut_fine_f = 2.0f * std::pow( q_level, 3 )
                           - 10.0f * std::pow( q_level, 2 )
                           + 32.0f * q_level + q.mod;
        return cut_fine_f;
    }

    if( q_level == 0 ) {
        return 0.0f;
    }

    if( q.req == qual_BUTCHER ) {
        return q_level * q.mod;
    }

    return  q.mod * q_level / ( q_level + 1.75f );
}

void activity_speed::calc_tools_factor( Character &who, const q_reqs &quality_reqs )
{
    auto &inv = who.crafting_inventory();
    float ac_f = tools_factor_custom_formula( quality_reqs, inv );
    //Any factor above 0 is valid, else - use default calc
    if( ac_f > 0 ) {
        tools = ac_f;
        return;
    }

    float f = 1;
    std::vector<float> factors;
    for( const auto &q : quality_reqs ) {
        factors.push_back( get_best_qual_mod( q, inv ) );
    }
    std::ranges::sort( factors, std::greater<>() );

    int denom = 0;
    for( const auto &factor : factors ) {
        f += refine_factor( factor, ++denom * 0.8f );
    }

    tools = limit_factor( f );
}

void activity_speed::calc_morale_factor( const Character &who )
{
    const int p_morale = who.get_morale_level();
    float ac_morale = morale_factor_custom_formula( who );
    //Any morale mod above 0 is valid, else - use default morale calc
    if( ac_morale > 0 ) {
        morale = ac_morale;
        return;
    }

    //1% per 4 extra morale
    if( morale > 20 ) {
        morale = 0.95f + p_morale / 400.0f;
    }
    // 1% per 1 insuff morale
    else if( morale < -20 ) {
        morale = 1.20f + p_morale / 100.0f;
    }
    morale = 1.0f;
}

void activity_speed::find_best_bench( const tripoint &pos, const metric metrics )
{
    static const std::string feature_wb = "WORKBENCH";
    static const workbench_info_wrapper ground_bench(
        *string_id<furn_t>( "f_ground_crafting_spot" )->workbench );
    static const workbench_info_wrapper hands_bench(
        *string_id<furn_t>( "f_fake_bench_hands" )->workbench );

    map &here = get_map();
    bench = bench_loc( ground_bench, pos );
    auto bench_tmp = bench_loc( hands_bench, pos );

    bench->wb_info.adjust_multiplier( metrics );
    bench_tmp.wb_info.adjust_multiplier( metrics );

    if( bench_tmp.wb_info.multiplier_adjusted > bench->wb_info.multiplier_adjusted ) {
        bench = bench_tmp;
    }

    std::vector<tripoint> reachable( PICKUP_RANGE * PICKUP_RANGE );
    here.reachable_flood_steps( reachable, pos, PICKUP_RANGE, 1, 100 );
    for( const tripoint &adj : reachable ) {
        if( const auto &wb = here.furn( adj )->workbench ) {
            bench_tmp = bench_loc( workbench_info_wrapper( *wb ), adj );
            bench_tmp.wb_info.adjust_multiplier( metrics );
            if( bench_tmp.wb_info.multiplier_adjusted > bench->wb_info.multiplier_adjusted ) {
                bench = bench_tmp;
            }
        } else if( const auto &vp = here.veh_at( adj ).part_with_feature( feature_wb, true ) ) {
            if( const auto &wb_info = vp->part().info().get_workbench_info() ) {
                bench_tmp = bench_loc( workbench_info_wrapper( *wb_info ), adj );
                bench_tmp.wb_info.adjust_multiplier( metrics );
                if( bench_tmp.wb_info.multiplier_adjusted > bench->wb_info.multiplier_adjusted ) {
                    bench = bench_tmp;
                }
            } else {
                debugmsg( "part '%' with WORKBENCH flag has no workbench info", vp->part().name() );
            }
        }
    }
}
