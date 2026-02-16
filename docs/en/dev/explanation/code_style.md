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
auto foo( int arg1, int *arg2 ) -> int
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

### Prefer Immutable Values

- Prefer immutable values and declare variables with `const`. Less moving parts mean more predictable code flow.

### Use `int` or `int64_t`

- Prefer `int`.
  - `long` in particular is problematic since it is _not_ a larger type than int on some platforms
    we support.
  - Using integral value larger than 32 bits should be avoided. Use `int64_t` if it's really necessary.
- `uint` is also a problem, it has poor behavior when overflowing and should be avoided for
  general purpose programming.
  - If you need binary data, `unsigned int` or `unsigned char` are fine, but you should probably
    use a `std::bitset` instead.
- `float` is to be avoided, but has valid uses.

### Use Auto Keyword

Use [`auto` keyword](https://learn.microsoft.com/en-us/cpp/cpp/auto-cpp?view=msvc-170) where it makes sense to do so, for example:

#### Trailing Return Types

- Prefer [trailing return types](https://en.wikipedia.org/wiki/Trailing_return_type) in function declarations. Long return types obscure function name and makes reading class methods a painful experience.
  ```cpp
  class Bar;
  auto foo( int a ) -> int
  {
      const Bar &bar = some_function();

      return is_bar_ok( bar ) ? 42 : 404;
  }
  ```

#### Decltype Style Generic Functions

- Use for `decltype` style generic functions
  ```diff
  template<typename A, typename B>
  - decltype(std::declval<A&>() * std::declval<B&>()) multiply(A a, B b)
  + auto multiply( A a, B b ) -> decltype( a * b )
  {
      return a*b;
  }
  ```

#### Aliasing for Long Iterator Declarations

- Aliasing for long iterator declarations
  ```diff
    std::map<int, std::map<std::string, some_long_typename>> some_map;

  - std::map<int, std::map<std::string, some_long_typename>>::iterator iter = some_map.begin();
  + auto iter = some_map.begin();
  ```

#### Lambda Declarations

- Required for Lambda declarations
  ```cpp
  auto two_times = []( int a ) { return a * 2; };
  ```

#### Editor Type Hinting

- Doesn't otherwise sacrifice readability for expedience. Options for inlay type hinting are available in popular code editor such as [vscode](https://github.com/clangd/vscode-clangd).

### Avoid `using namespace`

- Avoid `using namespace` for standard namespaces, because it pollutes the global namespace.

### Avoid Member Methods

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
