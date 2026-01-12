#include "catalua_bindings.h"

#include "catalua_bindings_utils.h"
#include "catalua.h"
#include "catalua_luna.h"
#include "catalua_luna_doc.h"

#include "effect.h"

void cata::detail::reg_effect( sol::state &lua )
{
#define UT_CLASS effect
    {
        sol::usertype<UT_CLASS> ut =
        luna::new_usertype<UT_CLASS>(
            lua,
            luna::no_bases,
            luna::no_constructor
        );
        SET_FX( get_id );
        SET_FX( disp_name );
        SET_FX( disp_desc );
        SET_FX( disp_short_desc );
        SET_FX( use_part_descs );
        SET_FX_N( get_effect_type, "get_type" );
        SET_FX( decay );
        SET_FX( get_duration );
        SET_FX( get_max_duration );
        SET_FX( set_duration );
        SET_FX( mod_duration );
        SET_FX( mult_duration );
        SET_FX( get_start_time );
        SET_FX( get_bp );
        SET_FX( get_intensity );
        SET_FX( get_max_intensity );
        SET_FX( set_intensity );
        SET_FX( mod_intensity );
        SET_FX( get_resist_traits );
        SET_FX( get_resist_effects );
        SET_FX( get_removes_effects );
        SET_FX( get_blocks_effects );
        SET_FX( get_mod );
        SET_FX( get_avg_mod );
        SET_FX( get_amount );
        SET_FX( get_min_val );
        SET_FX( get_max_val );
        SET_FX( get_sizing );
        SET_FX( get_percentage );
        SET_FX( activated );
        SET_FX( has_flag );
        SET_FX( get_addict_mod );
        SET_FX( get_harmful_cough );
        SET_FX( get_dur_add_perc );
        SET_FX( get_int_dur_factor );
        SET_FX( get_int_add_val );
        SET_FX( is_permanent );
        SET_FX( set_permanent );

        reg_serde_functions( ut );
    }
#undef UT_CLASS
}