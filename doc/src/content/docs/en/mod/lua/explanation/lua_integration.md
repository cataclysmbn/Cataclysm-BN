---
title: C++ Lua integration
---

This document describes implementation details behind Lua integration in Cataclysm: Bright Nights.

BN uses Lua 5.3.6 to run scripts and relies on sol2 v3.3.0 for bindings on C++ side.

## C++ layout

### Lua source files

To simplify build setup and improve portability we bundle `Lua 5.3.6` source code in `src/lua/`
directory and have the build systems compile it and link into the game executable and library for
tests.

### Sol2 source files

Sol2 makes it easy to bundle, we have `sol2 v3.3.0` single-header amalgamated version in `src/sol/`
and just include it as needed. The header is quite large, so the less source files include it the
better.

- `sol/config.hpp` - Configuration header, we have a few options defined there
- `sol/forward.hpp` - Forward declarations, a lightweight header that should be included in game
  headers instead of `sol/sol.hpp`
- `sol/sol.hpp` - Main sol2 header file, quite large, avoid including in game headers

### Game source files

All Lua-related game source files have the `catalua` prefix.

If you want to add new bindings, consider looking at existing examples in `src/catalua_bindings.cpp`
and reading relevant part of Sol2 docs.

- `catalua.h` (and `catalua.cpp`) - Main Lua interface. It's the only header most of the codebase
  will have to include, and it provides a public interface.
- `catalua_sol.h` and `catalua_sol_fwd.h` - Wrappers for `sol/sol.hpp` and `sol/forward.hpp` with
  custom pragmas to make them compile.
- `catalua_bindings*` - Game Lua bindings live here.
- `catalua_console.h`(`.cpp`) - Ingame Lua console.
- `catalua_impl.h`(`.cpp`) - Implementation details for `catalua.h`(`.cpp`).
- `catalua_iuse_actor.h`(`.cpp`) - Lua-driven `iuse_actor`.
- `catalua_log.h`(`.cpp`) - In-memory logging for the console.
- `catalua_luna.h` - Usertype registration interface with automatic doc generation, aka `luna`.
- `catalua_luna_doc.h` - List of types registration through `luna` or exposed to its doc generator.
- `catalua_readonly.h`(`.cpp`) - Functions for marking Lua tables as read-only.
- `catalua_serde.h`(`.cpp`) - Lua table to/from JSON (de-)serialization.
- `catalua_type_operators.h` - Macro that helps with implementing bindings for string_ids
