# Code Review Guidelines for LLMs

- Comment only on objective, obvious mistakes
- All comments must be direct, actionable, PR-specific
- No summaries, assessments, or meta-commentary

### Using `suggestion` Blocks

- Use **ONLY** when change is completely self-contained (no new variables/functions, single file):
- **DON'T** use for multi-file changes or changes needing external context. Use regular code blocks instead.

## Code Style

- Functions with >3 parameters should use an options struct with designated initializers
- Use `auto` with trailing return types
- Prefer `std::optional`/`std::expected` over returning `nullptr` or error codes

## Security

- Avoid unbounded loops/recursion
- Validate JSON input, check array bounds
- Use smart pointers, be cautious with save file deserialization
