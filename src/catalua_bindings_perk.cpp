#include "catalua_bindings.h"
#include "catalua_bindings_utils.h"
#include "catalua_coord.h"
#include "catalua_luna.h"
#include "catalua_luna_doc.h"
#include "perk.h"
#include "popup.h"
#include "string_input_popup.h"
#include "ui.h"

void cata::detail::reg_perk( sol::state &lua )
{
#define UT_CLASS perk
    sol::usertype<perk> ut = luna::new_usertype<perk>( lua, luna::no_bases,
                             luna::no_constructor );
    SET_MEMB( id );
    SET_FX( get_name );
    SET_FX( get_description );
    SET_FX( get_category );
}
