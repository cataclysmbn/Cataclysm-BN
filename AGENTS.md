# Cataclysm: Bright Nights - Agent Guidelines

## C++23 & Style

- **Standard**: Use C++23: `auto`, trailing returns (`->`), designated initializers, `std::ranges`, `std::optional`, `std::expected`, `<=>`.
- **Formatting**: Prefer single-line functions. Append `// *NOPAD*` to `<=>` and `-> <type>&` to prevent astyle bugs.
- **Headers**: Avoid modifying headers with >10 usages. Create new file with pure functions.
- **Refactoring**: Apply Boyscout rule to modified paths.

## Workflow

1. **Context**: Fetch issue details via GitHub MCP (if applicable). `git switch -c <type>/<issue-id>/<slug>` (Types: feat, fix, refactor, chore, build, ci).
2. **Develop**: Follow [Code Style](./docs/en/dev/explanation/code_style.md). Use `_( "text" )` for L10n.
3. **Test**: Build and verify. Create/update `tests/` (Catch2).
4. **Commit**: Run astyle. Follow Atomic Commit and [Conventional Commits](./docs/en/contribute/changelog_guidelines.md). **No body/footer unless critical.**
5. **PR**: **MUST** follow [Template](./.github/pull_request_template.md) structure exactly. **MUST** be terse.

## Build & Verify

```sh
# Build Game & Tests
cmake --preset linux-full
cmake --build --preset linux-full --target cataclysm-bn-tiles cata_test-tiles

# Run Tests
./out/build/linux-full/tests/cata_test-tiles "[optional-filter]"

# Validate JSON
./build-scripts/lint-json.sh

# Check Mods (validates mod JSON files)
./out/build/linux-full/cataclysm-bn-tiles --check-mods

# Generate Lua Documentation (if conflicts with lua_annotations.lua or docs/en/mod/lua/reference/lua.md)
deno task docs:gen
```

## References

- **Docs**: [Building](./docs/en/dev/guides/building/cmake.md), [Formatting](./docs/en/dev/guides/formatting.md), [Dev Index](./docs/en/dev/).
- **Review**: [LLM Guide](./.github/llm_review_guide.md).
