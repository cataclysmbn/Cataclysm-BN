# Cataclysm: Bright Nights - Agent Guidelines

Read `docs/` and `justfile` first. Large legacy codebase: search for an existing solution before writing one.

## C++ Conventions (NEVER VIOLATE)

```c++
const auto foo = 3; //< **MUST** use `auto`; `const` before `auto`.

auto bar() -> int; //< **MUST** use trailing return types, also in aliases: std::function<auto( int ) -> bool>.
auto baz() -> int&; // *NOPAD*  //< **MUST** append `// *NOPAD*` to reference/pointer returns and `<=>` (astyle bug).
auto qux() -> int { return 42; } //< **MUST** use single-line functions and lambdas whenever possible.

auto qux = my_struct{ .a = 1, .b = 2 }; //< **MUST** use designated initializers.
auto two_value() -> my_data; //< **SHOULD NOT** return std::pair/std::tuple; use a struct.
auto may_have_value() -> std::optional<int>; //< **MUST** use std::optional when a value may be absent.
auto may_fail() -> std::expected<int, std::string>; //< **MUST** use std::expected when it may fail.

/// **MUST** use triple slash doc comments and snake_case names.
struct comparable {
  int x;
  int y;
  auto operator<=>( const comparable & ) const = default; // *NOPAD* //< **MUST** use `<=>` for comparisons.
}

auto values = xs
  | filter( []( const auto & v ) { return v.is_valid(); } ) //< **SHOULD** use `auto` lambda params
  | transform( &value::get ) //< prefer member/function references over lambdas
  | ranges::to<std::vector>();

namespace { // **MUST** use anonymous namespace over `static`.

struct button_options { //< **MUST** use an options struct for >3 parameters.
  point pos;
  std::string text;
  nc_color fg = c_white;
};
auto print_button( const catacurses::window &w, const button_options &opts ) -> void;
// call site: print_button( w, { .pos = p, .text = "OK" } ); //< **MUST** use designated initializers; never spell button_options{ ... } when inferable.

} // namespace
```

- Collections: **MUST** use `std::ranges`/`std::views`/`ranges::to`/`cata_algo.h` with function-local `using namespace std::views;` and `namespace ranges = std::ranges;`. No manual `it++` loops unless mutation requires it; keep a plain `for` when index access, early break, mutation, or nesting is clearer (no `iota` + `for_each` to avoid loops).
- Modern C++23. Keep unused parameter names as comments: `bool /*is_avatar*/`.
- Avoid modifying headers with >10 usages (SHOULD NOT); add a new header with pure functions.
- New files go in a subdirectory so clang-format covers them.
- Check matcher semantics before using string prefixes: `OtMatchType.PREFIX` / `is_ot_match` take the base token without trailing `_` (`"robofachq"`); the matcher itself requires the `_`.
- Bug fixes keep requested behavior and visible content; fix the cause instead of disabling the feature, unless the user asks to remove it.

## Lua

- Type every function parameter with EmmyLua/LuaLS `---@param` (`---@class`/`---@field` for table shapes), including existing and local helpers; match the file's style. No `---@return` only for enforcement when inferable.
- Fix missing binding types at the binding/doc-generation source, never by hard-coding classes in `data/raw/generate_types.lua`.
- Test C++ Lua bindings with real bound objects; Lua-only mocks are supplementary.

## Workflow

1. Issue link: fetch it with `gh`, then `git gtr new <type>/<issue-id>/<slug>` (`feat`, `fix`, `refactor`, `chore`, `build`, `ci`).
2. Format touched files before building or testing (`just fmt-cpp <files>`, `just fmt-json <files>`, `deno fmt`, `deno task dprint fmt`). If only a broad target fits (`cmake --build build --target format` / `style-json-parallel`), revert unrelated formatter changes.
3. Build the game together with tests:
   ```sh
   cmake --preset linux-full
   cmake --build --preset linux-full --target cataclysm-bn-tiles cata_test-tiles
   ```
4. Add/update Catch2 tests in `tests/` and run checks:
   ```sh
   ./out/build/linux-full/tests/cata_test-tiles "[optional-filter]"
   ./build-scripts/lint-json.sh
   ./out/build/linux-full/cataclysm-bn-tiles --check-mods
   deno task docs:gen  # when lua_annotations.lua or docs/en/mod/lua/reference/lua.md conflict
   ```
   UI regression tests follow the production refresh path; do not prepare layout or paging manually.
5. Commit atomically with [Conventional Commits](./docs/en/contribute/changelog_guidelines.md); no body/footer unless critical.
6. PR: [template](./.github/pull_request_template.md), no fluff, **SHOULD** use ASD-STE100 Simplified Technical English. `git push && gh pr create --web --fill`. Track `gh pr checks` until done; fix branch-owned failures, rerun or report transient ones with the failing job.

If a mistake is found during a task, update AGENTS.md or the skill and fix the current branch before summarizing.

## i18n / PO

- Do not reduce requested string/context coverage; handle every named meaning. Add loader support when JSON context needs it.
- After touching PO files, fix every error from `msgfmt -f -c -o /tmp/ko.mo lang/po/ko.po` (Korean) and `./tools/check_po_printf_format.py`; never dismiss them as pre-existing.
- Translating docs: look up the glossary first, e.g. `rg -C2 -i 'speedway' lang/po/ko.po | rg -v '^(#:|--)' | head -n 20`.

## Data and Review

- Digital files keep at least 1 ml volume; zero-volume items are rejected, including scanned books.
- When generated or pulled files stop being tracked, verify ignore rules (`git status --ignored` or run the generator) and that every CI and release consumer still gets them.
- References: [Code Style](./docs/en/dev/explanation/code_style.md), [Building](./docs/en/dev/guides/building/cmake.md), [Formatting](./docs/en/dev/guides/formatting.md), [Dev Index](./docs/en/dev/), [LLM Review Guide](./.github/llm_review_guide.md). Use `_( "text" )` for L10n.
