# C++ Code Style Guide

All of the C++ code in the project is styled, you should run any changes you make through astyle
before pushing a pull request.

We are using astyle version 3.1. Version 3.0.1 will disagree in only a few places, while Version 3.6.6 will disagree in nearly every file.

Blocks of code can be passed through astyle to ensure that their formatting is correct:

```sh
astyle --style=1tbs --attach-inlines --indent=spaces=4 --align-pointer=name --max-code-length=100 --break-after-logical --indent-classes --indent-switches --indent-preproc-define --indent-col1-comments --min-conditional-indent=0 --pad-oper --add-braces --convet-tabs --unpad-paren --pad-paren-in --keep-one-line-blocks
```

These options are mirrored in `.astylerc`, `doc/CODE_STYLE.txt` and
`msvc-full-features/AStyleExtension-Cataclysm-BN.cfg`

For example, from `vi`, set marks a and b around the block, then:

```sh
:'a,'b ! astyle --style=1tbs --attach-inlines --indent=spaces=4 --align-pointer=name --max-code-length=100 --break-after-logical --indent-classes --indent-switches --indent-preproc-define --indent-col1-comments --min-conditional-indent=0 --pad-oper --add-braces --convet-tabs --unpad-paren --pad-paren-in --keep-one-line-blocks
```

See [DEVELOPER_TOOLING.md](../reference/tooling) for other environments.

## Code Example

Here's an example that illustrates the most common points of style:

```cpp
int foo( int arg1, int *arg2 )
{
    if( arg1 < 5 ) {
        switch( *arg2 ) {
            case 0:
                return arg1 + 5;
                break;
            case 1:
                return arg1 + 7;
                break;
            default:
                return 0;
                break;
        }
    } else if( arg1 > 17 ) {
        int i = 0;
        while( i < arg1 ) {
            printf( _( "Really long message that's pointless except for the number %d and for its "
                       "length as it's illustrative of how to break strings properly.\n" ), i );
        }
    }
    return 0;
}
```

## Code Guidelines

These are less generic guidelines and more pain points we've stumbled across over time.

- Prefer immutable values and declare variables with `const`. Less moving parts mean more predictable code flow.
- Prefer `int`.
  - `long` in particular is problematic since it is _not_ a larger type than int on some platforms
    we support.
  - Using integral value larger than 32 bits should be avoided. Use `int64_t` if it's really necessary.
  - `uint` is also a problem, it has poor behavior when overflowing and should be avoided for
    general purpose programming.
    - If you need binary data, `unsigned int` or `unsigned char` are fine, but you should probably
      use a `std::bitset` instead.
  - `float` is to be avoided, but has valid uses.
- Use [`auto` keyword](https://learn.microsoft.com/en-us/cpp/cpp/auto-cpp?view=msvc-170) where it makes sense to do so, for example:
  - Prefer [trailing return types](https://en.wikipedia.org/wiki/Trailing_return_type) in function declarations. Long return types obscure function name and makes reading class methods a painful experience.
  ```cpp
  class Bar;
  auto foo( int a ) -> int
  {
      const Bar &bar = some_function();

      return is_bar_ok( bar ) ? 42 : 404;
  }
  ```
  - Use for `decltype` style generic functions
  ```diff
  template<typename A, typename B>
  - decltype(std::declval<A&>() * std::declval<B&>()) multiply(A a, B b)
  + auto multiply( A a, B b ) -> decltype( a * b )
  {
      return a*b;
  }
  ```
  - Aliasing for long iterator declarations
  ```diff
    std::map<int, std::map<std::string, some_long_typename>> some_map;

  - std::map<int, std::map<std::string, some_long_typename>>::iterator iter = some_map.begin();
  + auto iter = some_map.begin();
  ```
  - Required for Lambda declarations
  ```cpp
  auto two_times = []( int a ) { return a * 2; };
  ```
  - Doesn't otherwise sacrifice readability for expedience. Options for inlay type hinting are available in popular code editor such as [vscode](https://github.com/clangd/vscode-clangd).

- Avoid `using namespace` for standard namespaces.
- Avoid adding new member methods to classes unless required.
  ```diff
  // this function does not access non-public data members or member methods in the class, and thus can be made a free function
  - std::string Character::profession_description() const
  - {
  -     return prof->description( male );
  - }
  + auto profession_description( const Character &c ) -> std::string
  + {
  +     return c.prof->description( c.male );
  + }
  ```

## Function Parameters

When a function has **more than 3 parameters**, use an options struct instead of a long parameter list. This improves:
- **Readability**: Named fields are self-documenting
- **Maintainability**: Adding/removing parameters doesn't break all call sites
- **Usability**: Designated initializers make call sites clear

### Before (Bad - Too many parameters)

```cpp
void print_button( const catacurses::window &w, point pos, 
                   const std::string &text, nc_color fg, nc_color bg,
                   std::function<void()> on_click, bool enabled );

// Call site is unclear
print_button( win, point( 10, 5 ), "OK", c_green, c_black, 
              []{ do_action(); }, true );
```

### After (Good - Options struct)

```cpp
struct button_options {
    point pos;
    std::string text;
    nc_color fg = c_white;
    nc_color bg = c_black;
    std::function<void()> on_click;
    bool enabled = true;
};

auto print_button( const catacurses::window &w, const button_options &opts ) -> void;

// Call site is self-documenting with designated initializers
print_button( win, {
    .pos = point( 10, 5 ),
    .text = "OK",
    .fg = c_green,
    .on_click = []{ do_action(); }
} );
```

### Guidelines

- **Required parameters** (no default): Put first in struct, no default value
- **Optional parameters**: Provide sensible defaults
- **Pass by const reference**: `const options_struct &opts` to avoid copies
- **Naming**: Use `<function_name>_options` or `<feature>_config` for the struct name
