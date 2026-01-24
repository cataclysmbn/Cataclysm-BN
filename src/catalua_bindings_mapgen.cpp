#include "catalua_bindings.h"
#include "catalua_bindings_utils.h"
#include "catalua_luna.h"
#include "catalua_luna_doc.h"

#include "mapgendata.h"
#include "ui.h"
#include "popup.h"
#include "string_input_popup.h"

void cata::detail::reg_mapgendata( sol::state &lua )
{
    sol::usertype<mapgendata> ut = luna::new_usertype<mapgendata>( lua, luna::no_bases,
                                   luna::no_constructor );

    DOC( "Gets it's oter id." );
    luna::set_fx( ut, "id", []( mapgendata & dat ) { return dat.terrain_type(); } );
    DOC( "Gets the oter id to the north." );
    luna::set_fx( ut, "north", []( mapgendata & dat ) { return dat.north(); } );
    DOC( "Gets the oter id to the east." );
    luna::set_fx( ut, "east", []( mapgendata & dat ) { return dat.east(); } );
    DOC( "Gets the oter id to the south." );
    luna::set_fx( ut, "south", []( mapgendata & dat ) { return dat.south(); } );
    DOC( "Gets the oter id to the west." );
    luna::set_fx( ut, "west", []( mapgendata & dat ) { return dat.west(); } );
    DOC( "Gets the oter id to the neast." );
    luna::set_fx( ut, "neast", []( mapgendata & dat ) { return dat.neast(); } );
    DOC( "Gets the oter id to the seast." );
    luna::set_fx( ut, "seast", []( mapgendata & dat ) { return dat.seast(); } );
    DOC( "Gets the oter id to the swest." );
    luna::set_fx( ut, "swest", []( mapgendata & dat ) { return dat.swest(); } );
    DOC( "Gets the oter id to the nwest." );
    luna::set_fx( ut, "nwest", []( mapgendata & dat ) { return dat.nwest(); } );
    DOC( "Gets the oter id to the above." );
    luna::set_fx( ut, "above", []( mapgendata & dat ) { return dat.above(); } );
    DOC( "Gets the oter id to the below." );
    luna::set_fx( ut, "below", []( mapgendata & dat ) { return dat.below(); } );
    DOC( "Gets the z-level that it is generated at." );
    luna::set_fx( ut, "zlevel", []( mapgendata & dat ) { return dat.zlevel(); } );

}
