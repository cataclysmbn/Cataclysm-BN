---
description: Quick reference guide for Cataclysm-BN Lua binding patterns and macros
---

# Lua Binding Quick Reference

Comprehensive reference for adding Lua bindings to Cataclysm: Bright Nights.

## Luna Documentation Macros

Add to `src/catalua_luna_doc.h`:

```cpp
// Simple value type
LUNA_VAL( my_type, "MyType" )

// Type with documentation
LUNA_DOC( my_class, "MyClass" )

// Enum type
LUNA_ENUM( my_enum, "MyEnum" )

// string_id and int_id pair
LUNA_ID( my_type, "MyType" )
// Creates: MyTypeRaw, MyTypeId, MyTypeIntId

// Pointer type (can be owned by Lua)
LUNA_PTR_VAL( my_class, "MyClass" )
// Creates: MyClass, Detached<MyClass>, Unique<MyClass>
```

## Utility Macros (in binding functions)

Define `UT_CLASS` before using:

```cpp
#define UT_CLASS my_class

// Members
SET_MEMB( field_name )              // Read/write member
SET_MEMB_RO( field_name )           // Read-only member
SET_MEMB_N_RO( field, "lua_name" )  // Read-only with custom name

// Functions
SET_FX( method_name )                          // Simple method
SET_FX_T( method_name, signature )             // With type signature
SET_FX_N( method_name, "lua_name" )            // With custom name
SET_FX_N_T( method_name, "lua_name", sig )     // Both custom name and signature

#undef UT_CLASS
```

## Luna Functions

### Create Usertype

```cpp
// No inheritance, no constructor
auto ut = luna::new_usertype<T>(
    lua,
    luna::no_bases,
    luna::no_constructor
);

// With base classes
auto ut = luna::new_usertype<Derived>(
    lua,
    luna::bases<Base1, Base2>(),
    luna::no_constructor
);

// With constructors
auto ut = luna::new_usertype<T>(
    lua,
    luna::no_bases,
    luna::constructors<
        T(),                    // Default
        T( int ),              // One arg
        T( int, std::string )  // Multiple args
    >()
);

// With trailing return type in registration function
auto cata::detail::reg_my_type( sol::state &lua ) -> void
{
    auto ut = luna::new_usertype<T>( /*...*/ );
    // ...
}
```

### Set Members

```cpp
// Set mutable member
luna::set( ut, "field", &T::field );

// Set read-only member
luna::set( ut, "field", sol::readonly( &T::field ) );

// Set property with getter/setter
luna::set( ut, "prop", 
    sol::property( &T::get_prop, &T::set_prop )
);

// Set constant value
luna::set( ut, "CONSTANT", 42 );
```

### Set Functions

```cpp
// Simple function
luna::set_fx( ut, "method", &T::method );

// With signature resolution
luna::set_fx( ut, "method",
    sol::resolve<int() const>( &T::method )
);

// Static function
luna::set_fx( ut, "create", &T::create_instance );

// Lambda wrapper (single-line when possible)
luna::set_fx( ut, "custom",
    []( T &self, int arg ) -> std::string { return self.internal_method( arg ); }
);

// Overloaded function (specify exact signature)
luna::set_fx( ut, "overloaded",
    sol::resolve<int( std::string ) const>( &T::overloaded )
);
```

### Operators

```cpp
// Comparison operators
luna::set_fx( ut, sol::meta_function::equal_to, &T::operator== );
luna::set_fx( ut, sol::meta_function::less_than, &T::operator< );
luna::set_fx( ut, sol::meta_function::less_than_or_equal_to, &T::operator<= );

// Arithmetic operators
luna::set_fx( ut, sol::meta_function::addition, &T::operator+ );
luna::set_fx( ut, sol::meta_function::subtraction, &T::operator- );
luna::set_fx( ut, sol::meta_function::multiplication, &T::operator* );

// String conversion
luna::set_fx( ut, sol::meta_function::to_string,
    []( const T &obj ) -> std::string { return obj.to_string(); }
);
```

### Libraries (Global Functions)

```cpp
DOC( "Library description" );
luna::userlib lib = luna::begin_lib( lua, "mylib" );

// Add functions
luna::set_fx( lib, "func", &my_function );

// Add constants
luna::set( lib, "CONSTANT", VALUE );

// Finalize
luna::finalize_lib( lib );

// Usage in Lua: mylib.func(), mylib.CONSTANT
```

### Documentation

```cpp
// Type documentation
DOC( "Description line 1" );
DOC( "Description line 2" );

// Function parameters documentation
DOC_PARAMS(
    "param1: Type - Description",
    "param2: Type - Description",
    "returns: Type - Description"
);
```

## Common Type Patterns

### string_id / int_id

```cpp
// Registration (in catalua_bindings_ids.cpp)
reg_id<my_type, true>( lua );   // Has int_id
reg_id<my_type, false>( lua );  // No int_id

// Usage in Lua
local id = MyTypeId.new("my_id")
print(id:str())           -- Get string
print(id:is_valid())      -- Check validity
print(id:is_null())       -- Check if null
local obj = id:obj()      -- Get underlying object
local int_id = id:int_id() -- Get int_id (if supported)
```

### Enums

```cpp
// Registration
luna::set_enum<my_enum>( lua, {
    { "VALUE_ONE", my_enum::VALUE_ONE },
    { "VALUE_TWO", my_enum::VALUE_TWO }
});

/// Usage in Lua
// local val = MyEnum.VALUE_ONE
// if val == MyEnum.VALUE_TWO then
//     -- ...
// end
```

### Units Types

```cpp
// Already bound: mass, volume, energy, angle

// Lua usage
local m = Mass.from_gram(1000)
print(m:to_kilogram())  -- 1

local v = Volume.from_liter(1)
print(v:to_milliliter())  -- 1000

local e = Energy.from_joule(1000)
print(e:to_kilojoule())  -- 1

local a = Angle.from_degrees(180)
print(a:to_radians())  -- ~3.14
```

### Time Types

```cpp
// Already bound: time_duration, time_point

// Lua usage
local dur = TimeDuration.from_seconds(60)
local now = game.get_time()
local future = now + dur
```

### Coordinates

```cpp
// Already bound: point, tripoint

// Lua usage
local p = Tripoint.new(10, 20, 0)
print(p.x, p.y, p.z)
local offset = Tripoint.new(1, 0, 0)
local new_p = p + offset
```

### Optional Values

```cpp
// C++ side
std::optional<int> maybe_value() const;

// Lua side - returns nil if empty
local val = obj:maybe_value()
if val then
    print("Got value:", val)
end
```

### Collections

```cpp
// std::vector, std::array auto-convert to Lua tables
std::vector<item> get_items() const;

// Lua usage
local items = char:get_items()
for i, item in ipairs(items) do
    print(item:tname())
end

// std::map auto-converts to Lua table
std::map<std::string, int> get_stats() const;

// Lua usage
local stats = char:get_stats()
for key, value in pairs(stats) do
    print(key, value)
end
```

### Detached Pointers (Lua ownership)

```cpp
// Function that returns owned object
luna::set_fx( ut, "remove_item",
    []( character &ch, item &it ) -> detached_ptr<item> { return detached_ptr<item>( ch.i_rem( &it ) ); } // *NOPAD*
);

// Function that takes ownership
luna::set_fx( ut, "add_item",
    []( character &ch, detached_ptr<item> it ) { ch.i_add( std::move( it ) ); }
);

/// Lua usage
// local item = char:remove_item(some_item)
// -- item is now owned by Lua
// char:add_item(item)
// -- ownership transferred back to C++
```

## Type Signature Examples

```cpp
// Const methods
int() const
std::string( int ) const
bool( const std::string & ) const

// Non-const methods
void()
void( int, std::string )
int&()  // NOPAD comment needed for references

// Static methods
static int create( std::string )

// Overloaded methods - specify exact signature
sol::resolve<int() const>( &T::get_value )
sol::resolve<int( std::string ) const>( &T::get_value )
```

## Function Parameter Types

```cpp
// By value
void func( int value );

// By const reference (preferred for non-primitives)
void func( const std::string &str );

// By reference (for output parameters)
void func( int &out_value );

// Pointers
void func( item *it );

// Optional parameters (sol::optional)
void func( int required, sol::optional<std::string> opt );

// Variadic arguments
void func( sol::variadic_args va );

// Tables
void func( sol::table config );
```

## Registration Functions

```cpp
// In catalua_bindings.h
namespace cata::detail {
    auto reg_my_domain( sol::state &lua ) -> void;
}

// In catalua_bindings_my_domain.cpp
auto cata::detail::reg_my_domain( sol::state &lua ) -> void
{
    // Register types and APIs
}

// In catalua_bindings.cpp, in reg_all_bindings()
auto cata::detail::reg_all_bindings( sol::state &lua ) -> void
{
    // ... existing
    reg_my_domain( lua );
}
```

## File Organization

```
src/
  catalua.h/cpp              - Main Lua interface
  catalua_luna.h             - Luna doc system
  catalua_luna_doc.h         - Type name mappings (LUNA_* macros)
  catalua_bindings.h/cpp     - Main bindings, units, constants
  catalua_bindings_utils.h   - Utility macros (SET_*, DOC)
  catalua_bindings_ids.cpp   - string_id/int_id bindings
  catalua_bindings_*.cpp     - Domain-specific bindings
```

## Build and Test

```sh
# Build
cmake --build --preset linux-full --target cataclysm-bn-tiles

# Format C++
cmake --build build --target astyle

# Test in Lua console
# Debug menu -> Lua console
# Or: Debug menu -> Info -> Submit bug report (U)
```

## Lua Testing Patterns

```lua
-- Test type creation
local obj = MyClass.new()

-- Test method call
local result = obj:method(arg)

-- Test property access
print(obj.field)
obj.field = new_value

-- Test inheritance
local derived = DerivedClass.new()
derived:base_method()  -- Should work

-- Test optional values
local maybe = obj:maybe_get()
if maybe then
    print("Got:", maybe)
end

-- Test collections
local items = obj:get_items()
for i, item in ipairs(items) do
    print(i, item)
end

-- Test API
local player = game.get_player()
player:add_msg("Hello from Lua!")
```

**Note**: C++ side should use trailing return types: `auto func() -> type { ... }`

## Common Issues and Solutions

| Issue                             | Solution                                        |
| --------------------------------- | ----------------------------------------------- |
| `luna_traits<T>::impl is false`   | Add `LUNA_*` macro to `catalua_luna_doc.h`      |
| Linker error: undefined reference | Include the header file with implementation     |
| Ambiguous function call           | Use `sol::resolve<signature>()`                 |
| Type not copyable                 | Use `luna::no_constructor` and provide factory  |
| Comparison operators missing      | Implement `operator==` and `operator<` for type |
| Build error with `// *NOPAD*`     | Only use for reference/pointer returns          |

## Key References

- **Integration Docs**: `docs/en/mod/lua/explanation/lua_integration.md`
- **Style Guide**: `docs/en/mod/lua/explanation/lua_style.md`
- **Luna System**: `src/catalua_luna.h`
- **Examples**: `src/catalua_bindings_creature.cpp`, `src/catalua_bindings_item.cpp`
- **Sol2 Docs**: https://sol2.readthedocs.io/
