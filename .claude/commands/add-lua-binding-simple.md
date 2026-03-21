---
description: Add a simple Lua binding for a C++ type (string_id, enums, or simple classes)
argument-hint: TypeName [output-file]
---

# Add Simple Lua Binding

This command helps you add Lua bindings for simple C++ types like `string_id`, enums, or basic classes.

## What You'll Do

1. **Identify the type** you want to bind (e.g., `my_type`, `my_enum`)
2. **Choose the binding file** (usually `src/catalua_bindings_ids.cpp` for IDs, or a relevant `catalua_bindings_*.cpp`)
3. **Add Luna documentation** macro to `src/catalua_luna_doc.h`
4. **Register the binding** in the appropriate function
5. **Build and test** the changes

## Step-by-Step Instructions

### For string_id types

```cpp
// In src/catalua_luna_doc.h, add:
LUNA_ID( my_type, "MyType" )

// In src/catalua_bindings_ids.cpp, in reg_game_ids():
reg_id<my_type, has_int_id>( lua );
// Use true if type has int_id, false otherwise
```

### For enum types

```cpp
// In src/catalua_luna_doc.h, add:
LUNA_ENUM( my_enum, "MyEnum" )

// In appropriate catalua_bindings_*.cpp file:
{
    auto values = sol::initializers(
        "VALUE_ONE", my_enum::VALUE_ONE,
        "VALUE_TWO", my_enum::VALUE_TWO
    );
    luna::set_enum( lua, values );
}
```

**Note**: Use trailing return types and auto throughout (see patterns below).

### For simple data types (read-only)

```cpp
// In src/catalua_luna_doc.h, add:
LUNA_DOC( my_class, "MyClass" )

// In appropriate catalua_bindings_*.cpp:
#define UT_CLASS my_class
{
    auto ut = luna::new_usertype<UT_CLASS>(
        lua,
        luna::no_bases,
        luna::no_constructor
    );

    // Add members
    SET_MEMB( my_field );      // Mutable field
    SET_MEMB_RO( const_field ); // Read-only field
    SET_FX( my_method );        // Method
}
#undef UT_CLASS
```

## Important Patterns

### Documentation Comments

```cpp
DOC( "Description of the type" );
DOC( "Additional details about usage" );
```

### Member Macros

- `SET_MEMB(name)` - Mutable member variable or property
- `SET_MEMB_RO(name)` - Read-only member
- `SET_FX(name)` - Member function
- `SET_FX_T(name, signature)` - Member function with specific overload

### Luna Macros

- `LUNA_ID(CppType, "LuaName")` - For string_id/int_id pairs
- `LUNA_DOC(CppType, "LuaName")` - For documentation-only types
- `LUNA_ENUM(CppType, "LuaName")` - For enum types
- `LUNA_VAL(CppType, "LuaName")` - For value types
- `LUNA_PTR_VAL(CppType, "LuaName")` - For pointer types

## Testing

After adding bindings:

```sh
# Build
cmake --build --preset linux-full --target cataclysm-bn-tiles

# Test in Lua console (debug menu -> Lua console)
local my_id = MyTypeId.new("some_id")
print(my_id:str())
print(my_id:is_valid())
```

## Common Issues

1. **Linker errors**: Make sure the type header is included
2. **Missing comparison operators**: Some types need `operator==` and `operator<`
3. **Build errors**: Check macro syntax and semicolons

## Files to Modify

- `src/catalua_luna_doc.h` - Add LUNA_* macro
- `src/catalua_bindings_ids.cpp` - For ID types
- `src/catalua_bindings_*.cpp` - For other types (choose appropriate file)

## References

- Lua Integration Docs: `docs/en/mod/lua/explanation/lua_integration.md`
- Existing Examples: `src/catalua_bindings_ids.cpp`
- Luna System: `src/catalua_luna.h`
