#include "pinyin.h"

#include "third-party/pinyin/pinyin_data.hpp"

#include <algorithm>
#include <ranges>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{

auto indexed_pinyin_map() -> const std::unordered_map<char32_t, std::vector<std::u32string>> &
{
    static const auto indexed_map = []() {
        auto map = std::unordered_map<char32_t, std::vector<std::u32string>> {};

        std::ranges::for_each( pinyin_data, [&map]( const auto & entry ) {
            std::ranges::for_each( entry.second, [&map, &entry]( const auto current_char ) {
                auto &current_pinyins = map[current_char];
                if( std::ranges::find( current_pinyins, entry.first ) == current_pinyins.end() ) {
                    current_pinyins.push_back( entry.first );
                }
            } );
        } );

        return map;
    }
    ();

    return indexed_map;
}

} // namespace

namespace pinyin
{

auto pinyin_match( const std::u32string &text, const std::u32string &query ) -> bool
{
    const auto &pinyin_index = indexed_pinyin_map();

    auto combination_index = size_t{ 0 };
    auto all_combinations_tested = false;

    while( !all_combinations_tested ) {
        auto current_combination = std::u32string{};
        current_combination.reserve( text.length() * 6 );

        auto current_combination_index = combination_index;
        auto total_combinations = size_t{ 1 };

        for( const auto current_char : text ) {
            const auto found = pinyin_index.find( current_char );
            if( found == pinyin_index.end() ) {
                current_combination += current_char;
                continue;
            }

            const auto &current_char_pinyin_list = found->second;
            current_combination += current_char_pinyin_list.at(
                                       current_combination_index % current_char_pinyin_list.size() );
            current_combination_index /= current_char_pinyin_list.size();
            total_combinations *= current_char_pinyin_list.size();
        }

        if( current_combination.find( query ) != std::u32string::npos ) {
            return true;
        }

        ++combination_index;
        all_combinations_tested = combination_index >= total_combinations;
    }

    return false;
}

} // namespace pinyin
