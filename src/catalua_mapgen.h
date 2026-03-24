#pragma once

#include "map.h"
#include "mapgen.h"
#include "catalua_sol.h"
#include "catalua_sol_fwd.h"
#include "ret_val.h"
#include "sol/sol.hpp"
#include <lua.h>

/** Dynamic mapgeniuse_actor provided by Lua. */
class mapgen_function_lua : public virtual mapgen_function
{
        friend class DynamicDataLoader;
    private:
        sol::protected_function generate_func;

    public:
        mapgen_function_lua( const std::string &func, int weight = 1000 );

        void generate( mapgendata &dat ) override;

};


