#pragma once

#include <optional>
#include <utility>
#include <vector>

#include "activity_speed_adapters.h"
#include "activity_type.h"
#include "character_stat.h"
#include "type_id.h"
#include "units.h"
#include "units_mass.h"
#include "units_volume.h"

class Character;
class inventory;
struct tripoint;

using stat_reqs = std::vector<activity_req<character_stat>>;
using stat_factors = std::vector<std::pair<character_stat, float>>;

using bench_factor_fn = std::function<void( bench_loc &, const metric & )>;
using morale_factor_fn = std::function<float( const Character & )>;
using tools_factor_fn = std::function<float( const q_reqs &, const inventory & )>;
using skills_factor_fn = std::function<float( const Character &, const skill_reqs & )>;
using stats_factor_fn = std::function<stat_factors( const Character &, const stat_reqs & )>;

static bench_factor_fn default_bench_factor = []( bench_loc &bench, const metric & )
{
    bench.wb_info.multiplier_adjusted = bench.wb_info.multiplier;
};
static morale_factor_fn default_morale_factor = []( const Character & )
{
    return -1.f;
};
static tools_factor_fn default_tools_factor = []( const q_reqs &, const inventory & )
{
    return -1.f;
};
static skills_factor_fn default_skills_factor = []( const Character &, const skill_reqs & )
{
    return -1.f;
};
static stats_factor_fn default_stats_factor = []( const Character &, const stat_reqs & )
{
    return stat_factors{};
};


/*
 * Struct to track activity speed by factors
*/
class activity_speed
{
    public:
        activity_id type = activity_id::NULL_ID();
        std::optional<bench_loc> bench;
        int assistant_count = 0;
        morale_factor_fn morale_factor_custom_formula = default_morale_factor;
        tools_factor_fn tools_factor_custom_formula = default_tools_factor;
        skills_factor_fn skills_factor_custom_formula = default_skills_factor;
        stats_factor_fn stats_factor_custom_formula = default_stats_factor;

        float assist = 1.0f;
        float bench_factor = 1.0f;
        float player_speed = 1.0f;
        float skills = 1.0f;
        float tools = 1.0f;
        float morale = 1.0f;
        float light = 1.0f;
        stat_factors stats;

        //Returns total product of all stats
        inline float stats_total() const {
            float acc = 1.0f;
            for( auto &stat : stats ) {
                acc *= stat.second;
            }
            return acc;
        }

        //Returns total product of all factors
        inline float total() const {
            return 1.0f * assist * bench_factor * player_speed * stats_total() * skills * tools * morale *
                   light;
        }

        //Returns total amonut of moves based on factors
        inline int total_moves() const {
            return std::roundf( total() * 100.0f );
        }

        //Calculates all factors
        void calc_all_moves( Character &who );
        void calc_all_moves( Character &who, activity_reqs_adapter &reqs );


        void calc_moves( const Character &who );

        void calc_assistants_factor( const Character &who );
        void calc_bench_factor( const Character &who );
        void find_best_bench( const tripoint &pos, metric metrics = std::make_pair( 0_milligram, 0_ml ) );
        void calc_light_factor( const Character &who );
        void calc_morale_factor( const Character &who );
        void calc_skill_factor( const Character &who, const skill_reqs &skill_req );

        void calc_stats_factors( const Character &who );
        static std::pair<character_stat, float> calc_single_stat( const Character &who,
                const activity_req<character_stat> &stat );

        void calc_tools_factor( Character &who, const q_reqs &quality_reqs );
        static float get_best_qual_mod( const activity_req<quality_id> &q,
                                        const inventory &inv );
};
