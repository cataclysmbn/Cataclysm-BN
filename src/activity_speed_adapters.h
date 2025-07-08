#pragma once
#include <utility>

#include "activity_type.h"
#include "point.h"
#include "units_mass.h"
#include "units_volume.h"

using metric = std::pair<units::mass, units::volume>;

using q_reqs = std::vector<activity_req<quality_id>>;
using skill_reqs = std::vector<activity_req<skill_id>>;

struct furn_workbench_info;
struct vpslot_workbench;
struct construction;

class recipe;

enum class bench_type : int {
    ground = 0,
    hands,
    furniture,
    vehicle
};

struct workbench_info_wrapper {
    // Base multiplier applied for crafting here
    float multiplier = 1.0f;
    float multiplier_adjusted = multiplier;
    // Mass/volume allowed before a crafting speed penalty is applied
    units::mass allowed_mass = 0_gram;
    units::volume allowed_volume = 0_ml;
    bench_type type = bench_type::ground;

    workbench_info_wrapper( furn_workbench_info f_info );
    workbench_info_wrapper( vpslot_workbench v_info );
    workbench_info_wrapper( float multiplier, const units::mass &allowed_mass,
                            const units::volume &allowed_volume, const bench_type &type );

    void adjust_multiplier( const metric &metrics );
};

struct bench_loc {
    workbench_info_wrapper wb_info;
    tripoint position;

    explicit bench_loc( workbench_info_wrapper info, tripoint position )
        : wb_info( info ), position( position ) {
    }
};

struct activity_reqs_adapter {
    q_reqs qualities;
    skill_reqs skills;
    metric metrics = std::make_pair( 0_milligram, 0_ml );

    activity_reqs_adapter( const construction &con );
    activity_reqs_adapter( const recipe &rec, const metric &metrics );
    activity_reqs_adapter( const skill_reqs &skills, const metric &metrics )
        : skills( skills ), metrics( metrics ) {
    }
};
