#pragma once

#include <string>
#include <vector>

namespace localization
{

/// The catalogue files of a language in the order the game looks a string up in them: the base game's, then
/// the active mods'. An earlier file's entry wins unless only a later one has plural forms.
auto catalogue_files( const std::string &lang_id ) -> std::vector<std::string>;

} // namespace localization
