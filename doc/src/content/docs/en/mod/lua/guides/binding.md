---
title: Binding new type
---

### Adding new type to the doc generator without binding internals

If a C++ type has not been registered in the doc generator, it will show up as
`<cppval: **gibberish** >`. To mitigate this problem, you can add
`LUNA_VAL( your_type, "YourType" )` in `catalua_luna_doc.h`, and the generator will use `YourType`
string for argument type.

### Binding new type to Lua

First, we need to register the new type with the bindings system. It needs to be done for many
reasons, including so that the doc generator understands it, and the runtime can deserialize from
JSON any Lua table that contains that type. If you don't you'll get a compile error saying
`Type must implement luna_traits<T>`.

1. In `catala_luna_doc.h`, add declaration for your type. For example, if we're binding an imaginary
   `horde` type (which is a `struct`), it will be a single line near the top of the file:
   ```cpp
   struct horde;
   ```
   Complex templated types may need to actually pull in the relevant header, but please avoid it as
   it heavily impacts compilation times.

2. In the same file, register your type with the doc generator. Continuing with the `horde` example,
   it's done like this:
   ```cpp
   LUNA_VAL( horde, "Horde" );
   ```
   While C++ types use all kinds of style for their names, on Lua side they all should be in
   `CamelCase`.

Now we can actually get to the details. The bindings are implemented in `catalua_bindings*.cpp`
files. They are spread out into multiple `.cpp` files to speed up compilation and make it easy to
navigate, so you can put yours into any existing `catalua_bindings*.cpp` file or make your own
similar file. They are also spread out into functions, for the same reasons. Let's register our
`horde` type, and put it in a new file and a new function:

1. Add a new function declaration in `catalua_bindings.h`:
   ```cpp
   void reg_horde( sol::state &lua );
   ```
2. Call the function in `reg_all_bindings` in `catalua_bindings.cpp`:
   ```cpp
   reg_horde( lua );
   ```
3. Make a new file, `catalua_bindings_horde.cpp`, with the following contents:
   ```cpp
   #include "catalua_bindings.h"

   #include "horde.h" // Replace with the header where your type is defined

   void cata::detail::reg_horde( sol::state &lua )
   {
       sol::usertype<horde> ut =
           luna::new_usertype<horde>(
               lua,
               luna::no_bases,
               luna::constructors <
                   // Define your actual constructors here
                   horde(),
                   horde( const point & ),
                   horde( int, int )
                   > ()
               );
       
       // Register all needed members
       luna::set( ut, "pos", &horde::pos );
       luna::set( ut, "size", &horde::size );

       // Register all needed methods
       luna::set_fx( ut, "move_to", &horde::move_to );
       luna::set_fx( ut, "update", &horde::update );
       luna::set_fx( ut, "get_printable_name", &horde::get_printable_name );

       // Add (de-)serialization functions so we can carry
       // our horde over the save/load boundary
       reg_serde_functions( ut );

       // Add more stuff like arithmetic operators, to_string operator, etc.
   }
   ```
4. That's it. Your type is now visible in Lua under name `Horde`, and you can use the binded methods
   and members.

### Binding new type to Lua (using Neovim's regex)

Binding classes/structs to Lua by hand can be quite tedious, which is why another way to bind a
class is by transforming its header file. For the third step of the second part from
[previously](#binding-new-type-to-lua), it's possible to use Neovim's built-in regex and C++ macros
to bind the class for us.

1. Make a copy of the class definition.
2. Apply both: `%s@class \([^{]\)\+\n*{@private:@` `%s@struct \([^{]\)\+\n*{@public:@`
3. Manually remove the constructors/unwanted methods at the beginning.
4. Delete all `private`/`protected` methods: `%s@\(private:\|protected:\)\_.\{-}\(public:\|};\)@\2`
5. Remove `};` at the end of the class definition.
6. Delete `public` labels: `%s@ *public:\n@`
7. Delete comments: `%s@\( *\/\*\_.\{-}\*\/\n\{,1\}\)\|\( *\/\/\_.\{-}\(\n\)\)@\3@g`
8. Unindent until there is zero base indentation.
9. Turn method definitions into declarations: `%s@ *{\(}\|\_.\{-}\n^}\)@;`
10. Push most method declarations into a single line: `%s@\((\|,\)\n *@\1@g`
11. Remove default values: `%s@ *= *\_.\{-}\( )\|;\|,\)@\1@g`
12. Remove `overriden`/`static` methods/members and `using`s:
    `%s@.*\(override\|static\|using\).*\n@@g`
13. Remove `template`s: `%s@^template<.*>\n.*\n@@g`
14. Remove `virtual` tag: `%s@^virtual *@`
15. Check if all lines end in a semicolon: `%s@\([^;]\)\n@\0@gn`
16. Count how many functions there are: `%s@\(.*(.*).*\)@@nc`
17. Push first found function to the end: `%s@\(.*(.*).*\)\n\(\n*\)\(\_.*\)@\3\1\2`
18. Now you'll want to repeat step 16 for the number of matches in step 15 minus one. For Neovim,
    input the match count minus one, '@', then ':', e.g. '217@:' repeats the last command 217 times.
19. Clean up new lines: `%s@\n\{3,}@\r\r`
20. Wrap methods into a macro: `%s@\(.*\) \+\([^ ]\+\)\((.*\);@SET_FX_T( \2, \1\3 );`
21. Wrap members into a macro; make sure to select which lines to affect first:
    `s@.\{-}\([^ ]\+\);@SET_MEMB( \1 );`
22. Make the previously multi-line method declarations span multiple lines again:
    `%s@\(,\)\([^ ]\)@\1\r        \2@g`

Now what's left to do is to take the chunk of text and use it in a Lua binding. Continuing with the
horde example, this is how the code should look like with these macros:

```cpp
#include "catalua_bindings.h"
#include "catalua_bindings_utils.h"

#include "horde.h" // Replace with the header where your type is defined

void cata::detail::reg_horde( sol::state &lua )
{
    #define UT_TYPE horde
    sol::usertype<UT_TYPE> ut =
    luna::new_usertype<UT_TYPE>(
        lua,
        luna::no_bases,
        luna::constructors <
            // Define your actual constructors here
            UT_TYPE(),
            UT_TYPE( const point & ),
            UT_TYPE( int, int )
            > ()
       );

    // Register all needed members
    SET_MEMB( pos );
    SET_MEMB( size );

    // Register all needed methods
    SET_FX_T( move_to, ... ); // Instead of ..., there'd be the type declaration of the method.
    SET_FX_T( update, ... );
    SET_FX_T( get_printable_name, ... );

    // Add (de-)serialization functions so we can carry
    // our horde over the save/load boundary
    reg_serde_functions( ut );

    // Add more stuff like arithmetic operators, to_string operator, etc.
    // ...
    #undef UT_TYPE // #define UT_TYPE horde
}
```

This method of binding to Lua lacks template method bindings and may be broken: compiler errors,
linker freezes, so it's best to assume these bindings will be broken by default, only needing slight
fixes / manual additions.

### Binding new enum to Lua

Binding enums is similar to binding types. Let's bind an imaginary `horde_type` enum here:

1. If enum does not have an explicitly defined container (the `: type` part after `enum name` in the
   header where it's defined), you'll have to specify the container first, for example:
   ```diff
     // hordes.h
   - enum class horde_type {
   + enum class horde_type : int {
       animals,
       robots,
       zombies
     }
   ```
2. Add the declaration to `catalua_luna_doc.h`
   ```cpp
   enum horde_type : int;
   ```
3. Register it in `catalua_luna_doc.h` with
   ```cpp
   LUNA_ENUM( horde_type, "HordeType" )
   ```
4. Ensure the enum implements the automatic conversion to/from `std::string`, see
   `enum_conversions.h` for details. Some enums will already have it, but most won't. Usually it's
   just a matter of specializing `enum_traits<T>` for your enum `T` in the header, then defining
   `io::enum_to_string<T>` in the `.cpp` file with enum -> string conversion. Some enums won't have
   the "last" value required for `enum_traits<T>`. In that case, you'd have to add one:
   ```diff
     enum class horde_type : int {
       animals,
       robots,
   -   zombies
   +   zombies,
   +   num_horde_types
     }
   ```
   Note that this only works for "monotonic" enums, i.e. ones that start with 0 and don't skip any
   values. In the example above, `animals` has implicit value of `0`, robots has implicit value of
   `1` and `zombies` has implicit value of `2`, so we can easily add `num_horde_types`, which will
   have correct and expected implicit value of `3`.
5. Bind enum fields in `reg_enums` function in `catalua_bindings.cpp`:
   ```cpp
   reg_enum<horde_type>( lua );
   ```
   This uses the automatic convertion from step 4, so we have equal names between JSON and Lua.

### Binding new `string_id<T>` or `int_id<T>` to Lua

Binding these can be done separately from binding `T` itself.

1. Register your type `T` with the doc generator if you haven't already (see
   [relevant docs](#adding-new-type-to-the-doc-generator-without-binding-internals)).
2. Replace `LUNA_VAL` from step 1 with `LUNA_ID`.
3. Ensure your type `T` implements operators `<` and `==`. It's usually easy implement them
   manually, and can be done semi-automatically with macro `LUA_TYPE_OPS` found in
   `catalua_type_operators.h`.
4. Ensure your type `T` has a null `string_id`. You can add one if it doesn't exist in
   `string_id_null_ids.cpp`. Use the `MAKE_CLASS_NULL_ID` macro if `T` is defined as a class,
   `MAKE_STRUCT_NULL_ID` macro otherwise.
5. Ensure your type's `T` `string_id` has `obj()` and `is_valid()` methods implemented. These
   methods are implemented on a case-by-case basis. Checking other `string_id`s as example is
   recommended.
6. In `catalua_bindings_ids.cpp`, add the header where your type T is defined:
   ```cpp
   #include "your_type_definition.h"
   ```
7. In `reg_game_ids` function, register it like so:
   ```cpp
   reg_id<T, true>( lua );
   ```

That `true` can be replaced with `false` if you only want to bind `string_id<T>` and don't care
about (or can't implement) `int_id<T>`.

You may get linker errors at this stage, e.g. about `is_valid()` or `NULL_ID()` methods, which are
for various reasons not implemented forall string or int ids. In this case, you'll have to define
these manually, see relevant docs on `string_id` and `int_id` for more info.

And that's it. Now, your type `T` will show up in Lua with `Raw` postfix, `string_id<T>` will have
`Id` postfix, and `int_id<T>` will have `IntId` postfix. As example, for
`LUNA_ID( horde, "Horde" )`, we'll get:

- `horde` -> `HordeRaw`
- `string_id<horde>` -> `HordeId`
- `int_id<horde>` -> `HordeIntId` All type conversions between the 3 are implemented automatically
  by the system. Actual fields and methods of `T` can be binded to Lua same way as usual.
