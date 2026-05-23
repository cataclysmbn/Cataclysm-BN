#pragma once

#include <string>

namespace pinyin
{

/// Match a query string against all pinyin combinations of the input text.
auto pinyin_match( const std::u32string &text, const std::u32string &query ) -> bool;

} // namespace pinyin
