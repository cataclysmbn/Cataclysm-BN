---
description: Add Lua API bindings for global game functions (game state, RNG, sounds, etc.)
argument-hint: api-domain
---

# Add Lua API Bindings

This command helps you expose global game functions and utilities to Lua through the game API.

## API Organization

Lua API functions are organized into libraries (namespaces):

- `game.*` - Game state, player access, time
- `map.*` - Map operations, terrain, furniture
- `rng.*` - Random number generation
- `gdebug.*` - Debug functions
- `const.*` - Game constants
- Custom libraries for specific domains

## Step-by-Step Process

### 1. Create or Extend Library Registration Function

In appropriate `src/catalua_bindings_*.cpp` file:

```cpp
auto cata::detail::reg_my_api( sol::state &lua ) -> void
{
    DOC( "My API - Description of what this API provides" );
    auto lib = luna::begin_lib( lua, "myapi" );

    // Add functions to the library
    luna::set_fx( lib, "function_name", &my_function );

    // Add constants
    luna::set( lib, "CONSTANT_NAME", CONSTANT_VALUE );

    luna::finalize_lib( lib );
}
```

### 2. Examples by Domain

#### Game State API

```cpp
auto cata::detail::reg_game_api( sol::state &lua ) -> void
{
    DOC( "Game state and control functions" );
    auto lib = luna::begin_lib( lua, "game" );

    // Get player
    luna::set_fx( lib, "get_player",
        []() -> Character& { return get_player_character(); } // *NOPAD*
    );

    // Get avatar (more specific)
    luna::set_fx( lib, "get_avatar",
        []() -> avatar& { return get_avatar(); } // *NOPAD*
    );

    // Get current time
    luna::set_fx( lib, "get_time",
        []() -> time_point { return calendar::turn; }
    );

    // Advance time
    luna::set_fx( lib, "advance_time",
        []( const time_duration &dur ) { calendar::turn += dur; }
    );

    luna::finalize_lib( lib );
}
```

#### Map API

```cpp
auto cata::detail::reg_map_api( sol::state &lua ) -> void
{
    DOC( "Map manipulation functions" );
    auto lib = luna::begin_lib( lua, "map" );

    // Get map reference
    luna::set_fx( lib, "get_map",
        []() -> map& { return get_map(); } // *NOPAD*
    );

    // Spawn item at location
    luna::set_fx( lib, "spawn_item",
        []( const tripoint &pos, const itype_id &id, int count ) {
            get_map().add_item_or_charges( pos, item( id, calendar::turn, count ) );
        }
    );

    luna::finalize_lib( lib );
}
```

#### RNG API

```cpp
auto cata::detail::reg_rng_api( sol::state &lua ) -> void
{
    DOC( "Random number generation" );
    auto lib = luna::begin_lib( lua, "rng" );

    // Random integer in range
    DOC( "Returns random integer between min and max (inclusive)" );
    luna::set_fx( lib, "rng",
        []( int min, int max ) -> int { return rng( min, max ); }
    );

    // Random from list
    DOC( "Returns random element from table" );
    luna::set_fx( lib, "random_entry",
        []( sol::table tbl ) -> sol::object {
            if( tbl.empty() ) { return sol::nil; }
            auto idx = rng( 1, tbl.size() );
            return tbl[idx];
        }
    );

    // One in X chance
    luna::set_fx( lib, "one_in",
        []( int chance ) -> bool { return one_in( chance ); }
    );

    // X in Y chance
    luna::set_fx( lib, "x_in_y",
        []( int x, int y ) -> bool { return x_in_y( x, y ); }
    );

    luna::finalize_lib( lib );
}
```

#### UI/Output API

```cpp
auto cata::detail::reg_ui_api( sol::state &lua ) -> void
{
    DOC( "User interface and message functions" );
    auto lib = luna::begin_lib( lua, "ui" );

    // Display message
    luna::set_fx( lib, "message",
        []( const std::string &msg ) { add_msg( msg ); }
    );

    // Display colored message
    luna::set_fx( lib, "message_colored",
        []( const std::string &color, const std::string &msg ) {
            auto col = color == "red" ? c_red :
                       color == "green" ? c_green :
                       color == "blue" ? c_blue : c_white;
            add_msg( col, msg );
        }
    );

    // Query yes/no
    luna::set_fx( lib, "query_yn",
        []( const std::string &msg ) -> bool { return query_yn( msg ); }
    );

    luna::finalize_lib( lib );
}
```

#### Constants API

```cpp
auto cata::detail::reg_constants( sol::state &lua ) -> void
{
    DOC( "Various game constants" );
    auto lib = luna::begin_lib( lua, "const" );

    // Map size constants
    luna::set( lib, "OM_OMT_SIZE", OMAPX );
    luna::set( lib, "OM_SM_SIZE", OMAPX * 2 );
    luna::set( lib, "OM_MS_SIZE", OMAPX * 2 * SEEX );
    luna::set( lib, "OMT_SM_SIZE", 2 );
    luna::set( lib, "OMT_MS_SIZE", SEEX * 2 );
    luna::set( lib, "SM_MS_SIZE", SEEX );

    // Movement costs
    luna::set( lib, "MOVE_COST", 100 );
    luna::set( lib, "ATTACK_COST", 100 );

    // Turn duration
    luna::set( lib, "TURN_DURATION", 1_seconds );

    luna::finalize_lib( lib );
}
```

### 3. Register in Main Binding

In `src/catalua_bindings.h`:

```cpp
namespace cata::detail {
    void reg_my_api( sol::state &lua );
}
```

In `src/catalua_bindings.cpp` in `reg_all_bindings()`:

```cpp
reg_my_api( lua );
```

## Advanced Patterns

### Variadic Message Functions

```cpp
// Accept variable arguments like print()
luna::set_fx( lib, "print",
    []( sol::variadic_args va ) {
        auto msg = cata::detail::fmt_lua_va( va );
        add_msg( msg );
    }
);
```

### Tables as Parameters

```cpp
// Accept Lua table, process as config
luna::set_fx( lib, "spawn_configured",
    []( const tripoint &pos, sol::table config ) {
        auto id = config.get_or<std::string>( "id", "" );
        auto count = config.get_or<int>( "count", 1 );
        auto active = config.get_or<bool>( "active", false );

        auto it = item( itype_id( id ), calendar::turn, count );
        if( active ) { it.activate(); }
        get_map().add_item( pos, it );
    }
);

-- Usage in Lua:
-- game.spawn_configured(pos, { id = "flashlight", count = 1, active = true })
```

### Callback Registration

```cpp
// Allow Lua to register callbacks
static sol::protected_function on_turn_callback;

luna::set_fx( lib, "register_on_turn",
    []( sol::protected_function func ) {
        on_turn_callback = func;
    }
);

// Later, in C++ game loop:
if( on_turn_callback ) {
    auto result = on_turn_callback();
    if( !result.valid() ) {
        sol::error err = result;
        debugmsg( "Lua callback error: %s", err.what() );
    }
}
```

### Safe Function Calls with Error Handling

```cpp
luna::set_fx( lib, "safe_execute",
    []( sol::state_view lua, const std::string &code ) -> bool {
        auto result = lua.safe_script( code );
        if( !result.valid() ) {
            auto err = sol::error( result );
            add_msg( c_red, "Lua error: %s", err.what() );
            return false;
        }
        return true;
    }
);
```

## Logging API Pattern

```cpp
auto cata::detail::reg_logging( sol::state &lua ) -> void
{
    auto lib = luna::begin_lib( lua, "log" );

    luna::set_fx( lib, "info", []( sol::variadic_args va ) {
        auto msg = cata::detail::fmt_lua_va( va );
        DebugLog( DL::Info, DC::Lua ) << msg;
        cata::get_lua_log_instance().add( cata::LuaLogLevel::Info, msg );
    });

    luna::set_fx( lib, "warn", []( sol::variadic_args va ) {
        auto msg = cata::detail::fmt_lua_va( va );
        DebugLog( DL::Warn, DC::Lua ) << msg;
        cata::get_lua_log_instance().add( cata::LuaLogLevel::Warn, msg );
    });

    luna::set_fx( lib, "error", []( sol::variadic_args va ) {
        auto msg = cata::detail::fmt_lua_va( va );
        DebugLog( DL::Error, DC::Lua ) << msg;
        cata::get_lua_log_instance().add( cata::LuaLogLevel::Error, msg );
    });

    luna::finalize_lib( lib );
}
```

## Best Practices

### 1. Safety First

```cpp
// Always validate parameters
luna::set_fx( lib, "spawn_monster",
    []( const tripoint &pos, const mtype_id &id ) {
        if( !id.is_valid() ) {
            debugmsg( "Invalid monster type: %s", id.c_str() );
            return;
        }
        g->place_critter_at( id, pos );
    }
);

// or as reference return with NOPAD
luna::set_fx( lib, "get_creature",
    []( const tripoint &pos ) -> creature& { // *NOPAD*
        return g->critter_at( pos );
    }
);
```

### 2. Use References for Performance

```cpp
// Return reference, not copy (for large objects)
luna::set_fx( lib, "get_map",
    []() -> map& { return get_map(); } // *NOPAD*
);
```

### 3. Document Everything

```cpp
DOC( "Spawns a monster at the given position" );
DOC_PARAMS(
    "pos: Tripoint - The position to spawn at",
    "id: MtypeId - The monster type to spawn",
    "returns: Monster - The spawned monster"
);
luna::set_fx( lib, "spawn_monster", &spawn_monster );
```

### 4. Consistent Naming

- Use snake_case for function names (matching C++ convention)
- Use UPPER_CASE for constants
- Group related functions in libraries

### 5. Error Handling

```cpp
luna::set_fx( lib, "risky_operation",
    []( int param ) {
        try {
            // risky code
        } catch( const std::exception &e ) {
            add_msg( c_red, "Error: %s", e.what() );
        }
    }
);
```

**Note**: Follow C++23 conventions from AGENTS.md: use `auto`, trailing return types, designated initializers, and `std::ranges` over for loops.

## Testing Your API

```lua
-- In Lua console

-- Test function
local result = myapi.function_name(arg1, arg2)
print(result)

-- Test constant
print(const.CONSTANT_NAME)

-- Test with callbacks
myapi.register_callback(function()
    print("Callback triggered!")
end)
```

## Common API Patterns

### Getter/Setter Pattern

```cpp
// Get value
luna::set_fx( lib, "get_difficulty",
    []() -> int { return get_option<int>( "DIFFICULTY" ); }
);

// Set value
luna::set_fx( lib, "set_difficulty",
    []( int value ) { get_options().get_option( "DIFFICULTY" ).set_value( value ); }
);
```

### Builder Pattern

```cpp
// Fluent API for complex object creation
luna::set_fx( lib, "create_item_builder",
    []( const itype_id &id ) -> item { return item( id, calendar::turn ); }
);
```

## File Organization

- `catalua_bindings_game.cpp` - Game state, player, time
- `catalua_bindings_map.cpp` - Map operations
- `catalua_bindings_ui.cpp` - UI and messages
- Create new file for new API domain

## References

- Existing APIs: `src/catalua_bindings_game.cpp`
- Utility functions: `src/catalua_bindings.cpp`
- Luna library API: `src/catalua_luna.h`
