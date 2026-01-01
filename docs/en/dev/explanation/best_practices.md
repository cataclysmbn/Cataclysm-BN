# best practices

List of conventions used in the project, plus just plain good practices. Not rules per se, most of
the codebase doesn't meet those standards.

## Naming

Good names:

- Are not abbreviated if it could make meaning less obvious - `count`, not `cnt`
- Are obvious from context - `direction::left` is clear, `int rotate` requires reading the
  documentation
- Obey same conventions as nearby names - `max_stored_kcal` and `stored_kcal`, not
  `max_stored_calories` but `stored_kcal` or `caloried_stored`

`snake_case` is preferred for consistency with most of the project's code.

## Classes

- Don't add methods where a regular function in a namespace would work -
  `crafting::consume_tools(tool_list,Character &)`, not `Character.consume_tools(tool_list)`
- Don't add both getters and setters if all they do is read/write a field - those are just disguised
  fields
  - Getters without (public) setters are OK
- Use `private` where possible, `public` when not, `protected` only when there's a clear reason for
  it

## Types

- Avoid pointers, instead use references, `std::optional` or even a function overload where possible
- Avoid using `std::pair` and `std::tuple` in headers, instead create a named struct
- Avoid using `int` or `std::string` where an `enum class` would work

## Function Parameters

For functions with **more than 3 parameters**, use an options struct:

```cpp
// Bad: Long parameter list
void render_ui( const catacurses::window &w, point pos, point size,
                const std::string &title, nc_color title_color,
                bool show_border, border_style style );

// Good: Options struct with designated initializers
struct ui_render_options {
    point pos;
    point size;
    std::string title;
    nc_color title_color = c_white;
    bool show_border = true;
    border_style style = border_style::solid;
};

auto render_ui( const catacurses::window &w, const ui_render_options &opts ) -> void;

// Usage is clear and maintainable
render_ui( win, {
    .pos = point_zero,
    .size = point( 50, 20 ),
    .title = "Inventory"
} );
```

Benefits:

- Self-documenting call sites via designated initializers
- Easy to add/remove parameters without breaking existing code
- Optional parameters get sensible defaults

## File organization

Try to avoid including headers from other headers. This negatively impacts compilation times (both
clean and partial builds), especially when the header in question is already big and widely used
(such as `character.h`, `avatar.h`, `map.h`, `game.h`, `item.h`, `npc.h`, etc.), and frequently
pollutes source files with definitions they have no need to know.

Some tips here:

- Compiler doesn't always need to know the whole definition of a class, a declaration may be enough.
  Say, if in `character.h` the `vehicle` class is only ever used for a function argument of type
  `vehicle &` - references are always of the same size regardless of what type they refer to, so
  compiler only needs to know there exists a type named `vehicle`, not its internal details. Adding
  `class vehicle;` declaration in `character.h` is enough in this case.
- It's possible to use `pointer-to-implementation` idiom for class members to remove the need for
  the definition, see `src/pimpl.h` for implementation and explanation. There are many usage
  examples in the codebase (mainly `game.h`), but the overall idea is to replace class member with a
  pointer, and only require definition when accessing the member through pointer. This incurs a tiny
  performance overhead, but it's unnoticeable in most cases.
- If you're adding some functionality to a frequently included header, and that functionality is
  supposed to be used only in a few source files, consider creating a separate header instead. This
  way, your code won't make it into dozens or hundreds of compilation units which don't ever use it,
  and the compiler wouldn't spend time to build it all these dozens or hundreds of unneeded times.
- Including system and std headers is OK, most of them are precompiled anyway.
- Including common "utility" headers (`optional.h`, `calendar.h`, `coordinates.h`, etc.) is OK,
  those are already extensively used in many headers and source files, so not including them makes
  too little impact to warrant the inconvenience.
