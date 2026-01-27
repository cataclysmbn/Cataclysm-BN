# Cataclysm-BN Development Skills

This directory contains Claude Code command skills for developing Cataclysm: Bright Nights.

## Available Skills

### Lua Bindings

### 1. `add-lua-binding-simple`

**For**: Simple types like `string_id`, enums, basic data structures

**Use when**:

- Adding bindings for ID types (`my_type` → `MyTypeId`)
- Exposing enums to Lua
- Binding simple read-only data structures
- Quick, straightforward bindings

**Example**:

```
Add Lua binding for ammunition_type
```

### 2. `add-lua-binding-complex`

**For**: Complex C++ classes with methods, inheritance, constructors

**Use when**:

- Binding classes with public APIs
- Working with inheritance hierarchies
- Need constructors, methods, properties
- Complex object interactions

**Example**:

```
Add comprehensive Lua bindings for the Vehicle class
```

### 3. `add-lua-binding-api`

**For**: Global game functions and utilities

**Use when**:

- Creating new Lua API libraries (like `game.*`, `map.*`)
- Exposing global helper functions
- Adding RNG, UI, or utility functions
- Need namespace organization

**Example**:

```
Add Lua API for weather functions
```

### 4. `lua-binding-reference`

**For**: Quick reference and troubleshooting

**Use when**:

- Need syntax reminder
- Looking up macro definitions
- Checking patterns and examples
- Debugging binding issues

## Quick Start

### For Lua Bindings

1. **Determine what you're binding**:
   - Simple type (ID, enum)? → `add-lua-binding-simple`
   - Complex class? → `add-lua-binding-complex`
   - Global functions? → `add-lua-binding-api`

2. **Invoke the skill**:
   ```
   /add-lua-binding-simple ammunition_type
   ```

3. **Follow the step-by-step instructions** in the skill output

4. **Reference `lua-binding-reference`** for syntax and patterns

## File Organization

### Lua Bindings

After adding bindings, you'll modify:

- `src/catalua_luna_doc.h` - Type name registration (LUNA_* macros)
- `src/catalua_bindings_*.cpp` - Implementation
- `src/catalua_bindings.h` - Function declarations (if needed)
- `src/catalua_bindings.cpp` - Registration calls (if needed)

## Testing

All skills include testing instructions:

1. Build: `cmake --build --preset linux-full --target cataclysm-bn-tiles`
2. Format: `cmake --build build --target astyle`
3. Test in Lua console (Debug menu)

## Project Context

### Lua Binding System

- **Lua Version**: 5.3.6 (bundled in `src/lua/`)
- **Binding Library**: Sol2 v3.3.0 (in `src/sol/`)
- **Custom System**: Luna (automatic documentation generation)
- **Documentation**: Auto-generated to `docs/en/mod/lua/reference/lua.md`

## Official Documentation

### General

- Code Style: `docs/en/dev/explanation/code_style.md`
- Building: `docs/en/dev/guides/building/cmake.md`
- Agent Guidelines: `AGENTS.md`

### Lua Integration

- Integration: `docs/en/mod/lua/explanation/lua_integration.md`
- Style Guide: `docs/en/mod/lua/explanation/lua_style.md`
- API Reference: `docs/en/mod/lua/reference/lua.md` (auto-generated)

## Contributing

When adding code:

1. **Follow AGENTS.md**: C++23 conventions (auto, trailing returns, designated initializers, ranges)
2. **Build and test**: Use cmake presets
3. **Format code**: Run astyle before committing
4. **Follow patterns**: Look for existing similar code first

### Lua Binding Additions

1. Use the Luna system for documentation
2. Group related bindings in appropriate files
3. Test thoroughly in Lua console

## Support

For issues or questions:

- **Lua Bindings**: Check `lua-binding-reference`, review `src/catalua_bindings_*.cpp`
- **General**: Consult official docs, check AGENTS.md
