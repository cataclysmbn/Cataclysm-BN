#pragma once

#include "iuse.h"
#include "catalua_sol.h"
#include "ret_val.h"

/** Dynamic iuse_actor provided by Lua. */
class lua_iuse_actor : public iuse_actor
{
    private:
        sol::protected_function luafunc;

    public:
        lua_iuse_actor( const std::string &type, sol::protected_function &&luafunc );
        ~lua_iuse_actor() override;
        void load( const JsonObject &obj ) override;
        int use( player &who, item &itm, bool tick, const tripoint &pos ) const override;
        ret_val<bool> can_use( const Character &, const item &, bool, const tripoint & ) const override;
        std::unique_ptr<iuse_actor> clone() const override;
};


