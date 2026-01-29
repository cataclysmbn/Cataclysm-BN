#pragma once

#include <iosfwd>
#include <string>
#include <vector>

namespace cata
{
struct lua_state;

/**
 * Dump Lua-defined data to JSON format for tooling compatibility.
 *
 * This enables tools like the HHG extractor and migration scripts to
 * process Lua-defined data identically to JSON-defined data.
 *
 * @param state The Lua state containing loaded data
 * @param types Types to dump (e.g., "effect_type", "mutation", "bionic").
 *              If empty, dumps all supported types.
 * @param output Output stream for JSON (typically std::cout)
 * @return true if successful, false on error
 */
bool dump_lua_data( lua_state &state,
                    const std::vector<std::string> &types,
                    std::ostream &output );

/**
 * Get list of all supported data types for dumping.
 */
std::vector<std::string> get_dumpable_lua_data_types();

} // namespace cata
