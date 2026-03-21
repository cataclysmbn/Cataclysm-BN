# Code Review Guidelines for LLMs

- Comment only on objective, obvious mistakes
- All comments must be direct, actionable, PR-specific
- No summaries, assessments, or meta-commentary

### Using `suggestion` Blocks

- Use **ONLY** when change is completely self-contained (no new variables/functions, single file):
- **DON'T** use for multi-file changes or changes needing external context. Use regular code blocks instead.

## Security

- Avoid unbounded loops/recursion
- Validate JSON input, check array bounds
- Use smart pointers, be cautious with save file deserialization
