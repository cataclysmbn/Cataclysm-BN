#include "activity_speed_adapters.h"

#include <utility>

#include "construction.h"
#include "mapdata.h"
#include "recipe.h"
#include "requirements.h"
#include "units_mass.h"
#include "units_volume.h"
#include "veh_type.h"

workbench_info_wrapper::workbench_info_wrapper( furn_workbench_info f_info )
    : multiplier( f_info.multiplier ), allowed_mass( f_info.allowed_mass ),
      allowed_volume( f_info.allowed_volume ), type( bench_type::furniture ) {}

workbench_info_wrapper::workbench_info_wrapper( vpslot_workbench v_info )
    : multiplier( v_info.multiplier ), allowed_mass( v_info.allowed_mass ),
      allowed_volume( v_info.allowed_volume ), type( bench_type::vehicle ) {}

workbench_info_wrapper::workbench_info_wrapper( float multiplier,
        const units::mass &allowed_mass, const units::volume &allowed_volume, const bench_type &type )
    : multiplier( multiplier ), allowed_mass( allowed_mass ), allowed_volume( allowed_volume ),
      type( type ) {}

template<typename T>
static float lerped_multiplier( const T &value, const T &low, const T &high )
{
    // No effect if less than allowed value
    if( value < low ) {
        return 1.0f;
    }
    // Bottom out at 25% speed
    if( value > high ) {
        return 0.25f;
    }
    // Linear interpolation between high and low
    // y = y0 + ( x - x0 ) * ( y1 - y0 ) / ( x1 - x0 )
    return 1.0f + ( value - low ) * ( 0.25f - 1.0f ) / ( high - low );
}

void workbench_info_wrapper::adjust_multiplier( const metric &metrics )
{
    multiplier_adjusted *= lerped_multiplier( metrics.first, allowed_mass, 1000_kilogram );
    multiplier_adjusted *= lerped_multiplier( metrics.second, allowed_volume, 1000_liter );
}

activity_reqs_adapter::activity_reqs_adapter( const recipe &rec,
        const metric &metrics ) : metrics( metrics )
{
    for( auto &qual : rec.simple_requirements().get_qualities() ) {
        qualities.emplace_back( qual.front().type, qual.front().level );
    }

    skills.emplace_back( rec.skill_used, rec.difficulty );
    for( auto &skill : rec.required_skills ) {
        skills.emplace_back( skill.first, skill.second );
    }
}

activity_reqs_adapter::activity_reqs_adapter( const construction &con )
{
    for( auto &qual : con.requirements->get_qualities() ) {
        qualities.emplace_back( qual.front().type, qual.front().level );
    }

    for( auto &skill : con.required_skills ) {
        skills.emplace_back( skill.first, skill.second );
    }
}
