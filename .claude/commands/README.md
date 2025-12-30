# Cataclysm-BN Development Skills

This directory contains Claude Code command skills for developing Cataclysm: Bright Nights.

## Available Skills

### UI Development

#### `add-mouse-support`
**For**: Adding mouse interaction (click/hover) to UI elements

**Use when**:
- Making menus, buttons, or lists clickable
- Adding hover effects to UI elements
- Migrating keyboard-only UI to support mouse
- Need context menus or tooltips

**Example**:
```
Add mouse support to the inventory list
```

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

### For UI Development

1. **Determine your task**:
   - Adding mouse support? → `add-mouse-support`

2. **Invoke the skill**:
   ```
   /add-mouse-support inventory-list
   ```

3. **Follow the step-by-step instructions** and choose the appropriate approach

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

### Mouse Support
When adding mouse support to UI:
- Your UI file (e.g., `src/main_menu.cpp`, `src/inventory_ui.cpp`)
- Reference: `src/mouse_input.h`, `src/output.h` (usually no changes needed)

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

### Mouse Support System
- **Issue**: #7736 - Canvas-based mouse callback system
- **PR**: #7737
- **Implementation**: Commits da47bfa → 4ce6f29d14
- **Example**: `src/main_menu.cpp` (working reference)

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

### UI Development
- Mouse System: Issue #7736, `src/mouse_input.h`, `src/output.h`
- Input Handling: `src/input.h`, `src/ui_manager.h`

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

### Mouse Support Additions
1. Choose appropriate approach (callbacks vs manual hit-testing)
2. Test in both TILES and curses modes
3. Verify coordinate conversion for pixel→cell

### Lua Binding Additions
1. Use the Luna system for documentation
2. Group related bindings in appropriate files
3. Test thoroughly in Lua console

## Support

For issues or questions:
- **Mouse Support**: Check `add-mouse-support` skill, review `src/main_menu.cpp`
- **Lua Bindings**: Check `lua-binding-reference`, review `src/catalua_bindings_*.cpp`
- **General**: Consult official docs, check AGENTS.md
