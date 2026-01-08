---
description: Add comprehensive Lua bindings for complex C++ classes with methods, inheritance, and constructors
argument-hint: ClassName [binding-file]
---

# Add Complex Lua Binding

This command helps you create comprehensive Lua bindings for complex C++ classes including:

- Classes with methods and properties
- Inheritance hierarchies
- Constructors and factory methods
- Custom operators

## Planning Your Binding

Before you start, determine:

1. **Class name** and **Lua name**
2. **Base classes** (if any)
3. **Constructors** to expose
4. **Methods** to expose (public API)
5. **Properties** to expose
6. **Target file** (`catalua_bindings_*.cpp` - group by domain)

## Step-by-Step Process

### 1. Add Luna Documentation Macro

In `src/catalua_luna_doc.h`:

```cpp
// For regular class
LUNA_DOC( my_class, "MyClass" )

// For class that can be detached (owned by Lua)
LUNA_PTR_VAL( my_class, "MyClass" )
```

### 2. Choose/Create Binding File

Files are organized by domain:

- `catalua_bindings_creature.cpp` - Creature, Character, Player, Avatar, NPC
- `catalua_bindings_item.cpp` - Item and related
- `catalua_bindings_map.cpp` - Map, terrain, furniture
- `catalua_bindings_*.cpp` - Create new if needed

### 3. Include Required Headers

At top of binding file:

```cpp
#include "catalua_bindings.h"
#include "catalua_bindings_utils.h"
#include "catalua_luna.h"
#include "catalua_luna_doc.h"

#include "my_class.h"  // Your class header
// ... other needed headers
```

### 4. Create Registration Function

```cpp
auto cata::detail::reg_my_domain( sol::state &lua ) -> void
{
#define UT_CLASS my_class
    {
        DOC( "Description of MyClass" );
        DOC( "Additional documentation lines" );

        auto ut = luna::new_usertype<UT_CLASS>(
            lua,
            // Specify base classes
            luna::bases<base_class>(),  // or luna::no_bases
            // Specify constructors
            luna::constructors<
                UT_CLASS(),                    // Default
                UT_CLASS( const UT_CLASS & ),  // Copy
                UT_CLASS( int, std::string )   // Custom
            >()
            // or luna::no_constructor
        );

        // === MEMBER VARIABLES ===
        SET_MEMB( my_field );           // Read/write
        SET_MEMB_RO( readonly_field );  // Read-only

        // === MEMBER FUNCTIONS ===
        SET_FX( simple_method );

        // Method with overloads (specify signature)
        SET_FX_T( overloaded_method, int( std::string ) const );

        // Method with lambda wrapper
        luna::set_fx( ut, "custom_method",
            []( UT_CLASS &self, int arg ) -> std::string { return self.internal_method( arg ); }
        );

        // === STATIC FUNCTIONS ===
        luna::set_fx( ut, "static_factory", &UT_CLASS::create );

        // === OPERATORS ===
        luna::set_fx( ut, sol::meta_function::equal_to, &UT_CLASS::operator== );
        luna::set_fx( ut, sol::meta_function::less_than, &UT_CLASS::operator< );
        luna::set_fx( ut, sol::meta_function::to_string,
            []( const UT_CLASS &obj ) -> std::string { return obj.to_string(); }
        );
    }
#undef UT_CLASS
}
```

### 5. Register in Main Binding Function

In `src/catalua_bindings.h`:

```cpp
namespace cata::detail {
    void reg_my_domain( sol::state &lua );
}
```

In `src/catalua_bindings.cpp`, in `reg_all_bindings()`:

```cpp
void cata::detail::reg_all_bindings( sol::state &lua ) {
    // ... existing registrations
    reg_my_domain( lua );
}
```

## Advanced Patterns

### Handling Inheritance

```cpp
// Base class
luna::new_usertype<base_class>(
    lua,
    luna::no_bases,
    luna::no_constructor
);

// Derived class
luna::new_usertype<derived_class>(
    lua,
    luna::bases<base_class>(),  // Specify base
    luna::no_constructor
);
```

### Detached Pointers (Lua ownership)

For objects that Lua can own:

```cpp
// Return detached pointer from C++
luna::set_fx( ut, "remove_item",
    []( character &ch, item &it ) -> detached_ptr<item> {
        return detached_ptr<item>( ch.remove_item( it ) );
    }
);

// Accept detached pointer as parameter
luna::set_fx( ut, "add_item",
    []( character &ch, detached_ptr<item> it ) {
        ch.add_item( std::move( it ) );
    }
);
```

### Optional Parameters and Return Values

```cpp
// Optional parameters (sol::optional)
luna::set_fx( ut, "method_with_optional",
    []( UT_CLASS &self, int required, sol::optional<std::string> opt ) {
        if( opt ) {
            self.do_something( required, *opt );
        } else {
            self.do_something( required );
        }
    }
);

// Optional return (std::optional)
luna::set_fx( ut, "maybe_get_value",
    &UT_CLASS::try_get_value  // Returns std::optional<T>
);
```

### Variadic Arguments

```cpp
luna::set_fx( ut, "variadic_method",
    []( UT_CLASS &self, sol::variadic_args va ) {
        for( auto arg : va ) {
            // Process each argument
        }
    }
);
```

### Exposing Collections

```cpp
// Return vector/array as Lua table
luna::set_fx( ut, "get_items",
    &UT_CLASS::get_items  // Returns std::vector<item>
);

// Return map as Lua table
luna::set_fx( ut, "get_stats",
    &UT_CLASS::get_stats  // Returns std::map<std::string, int>
);
```

### Documentation with Parameters

```cpp
DOC( "Applies damage to a body part" );
DOC_PARAMS(
    "attacker: Creature - The attacking creature",
    "bp: BodyPartTypeIntId - Target body part", 
    "damage: DamageInstance - Damage to apply"
);
luna::set_fx( ut, "apply_damage", &UT_CLASS::apply_damage );
```

## Method Signature Resolution

When a method has overloads, use `sol::resolve`:

```cpp
// Method: int get_value() const
SET_FX_T( get_value, int() const );

// Method: int get_value(std::string) const  
SET_FX_T( get_value, int( std::string ) const );

// Or with sol::resolve directly:
luna::set_fx( ut, "get_value",
    sol::resolve<int() const>( &UT_CLASS::get_value )
);
```

## Coding Conventions

Follow Cataclysm-BN C++23 style (see AGENTS.md):

```cpp
/// Doc comments with triple slash
auto function_name( int param ) -> int { return param * 2; }

// Use auto for types
auto ut = luna::new_usertype<UT_CLASS>( /*...*/ );

// Use trailing return types in lambdas
luna::set_fx( ut, "method",
    []( UT_CLASS &self ) -> int { return self.value; }
);

// Use designated initializers
auto result = my_struct{ .a = 1, .b = 2 };

// Use std::ranges over for loops
auto values = items
    | std::views::filter( []( const auto &v ) { return v.is_valid(); } )
    | std::ranges::to<std::vector>();

// Use snake_case for functions and variables
auto my_variable = 42;
```

## Testing Your Bindings

```lua
-- In Lua console (Debug menu -> Lua console)

-- Test constructor
local obj = MyClass.new()

-- Test method
local result = obj:some_method()

-- Test property
print(obj.some_field)

-- Test static function
local instance = MyClass.static_factory("arg")

-- Test inheritance
local derived = DerivedClass.new()
derived:base_method()  -- Should work if inheritance correct
```

## Build and Verify

```sh
# Build
cmake --build --preset linux-full --target cataclysm-bn-tiles cata_test-tiles

# Format
cmake --build build --target astyle

# Test
./out/build/linux-full/tests/cata_test-tiles "[lua]"
```

## Common Patterns Reference

### Units (mass, volume, energy, angle)

```cpp
// These are already bound, use as reference
luna::set_fx( ut, "from_gram", &units::from_gram<std::int64_t> );
luna::set_fx( ut, "to_gram", &units::to_gram<std::int64_t> );
```

### Time (duration, time_point)

```cpp
// Already bound, see catalua_bindings.cpp
```

### Coordinates (tripoint, point)

```cpp
// Already bound, see catalua_bindings_coords.cpp
```

## Troubleshooting

**Issue**: Linker errors about undefined references

- **Fix**: Include the header file containing the class implementation

**Issue**: `luna_traits<T>::impl is false`

- **Fix**: Add `LUNA_DOC` or appropriate macro to `catalua_luna_doc.h`

**Issue**: Ambiguous function call

- **Fix**: Use `SET_FX_T` or `sol::resolve` to specify exact signature

**Issue**: Type not copyable/movable

- **Fix**: Use `luna::no_constructor` and provide factory functions

**Issue**: Method returns reference but Lua sees copy

- **Fix**: Consider if reference is correct, or use pointers

## Files to Modify

1. `src/catalua_luna_doc.h` - Add LUNA_* macro
2. `src/catalua_bindings_*.cpp` - Implementation (choose or create)
3. `src/catalua_bindings.h` - Declare registration function
4. `src/catalua_bindings.cpp` - Call registration in `reg_all_bindings()`

## Reference Files

- Example complex class: `src/catalua_bindings_creature.cpp` (Character, Creature)
- Example with items: `src/catalua_bindings_item.cpp`
- Luna system: `src/catalua_luna.h`
- Utility macros: `src/catalua_bindings_utils.h`
- Integration docs: `docs/en/mod/lua/explanation/lua_integration.md`
