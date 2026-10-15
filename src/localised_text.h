#pragma once

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

#include "translations.h"

/// A message the way a client can render it from the engine's catalogues: the untranslated format and
/// the values of its placeholders, in order. The wire form is `{key, ctx?, plural?, args?}`.
struct localised_text {
    using arg = std::variant<std::int64_t, double, std::string>;

    std::string key;
    std::optional<std::string> ctx;
    /// The count that selects the plural form.
    std::optional<double> plural;
    std::vector<arg> args;

    auto operator==( const localised_text & ) const -> bool = default; // *NOPAD*
};

namespace localised
{

/// A placeholder value a client can render: a number or a string. Anything else keeps the line a plain string.
template<typename T>
auto to_arg( const T &value ) -> std::optional<localised_text::arg>
{
    using plain = std::remove_cvref_t<T>;
    if constexpr( std::is_same_v<plain, bool> || std::is_same_v<plain, char> ) {
        return std::nullopt;
    } else if constexpr( std::is_integral_v<plain> ) {
        return static_cast<std::int64_t>( value );
    } else if constexpr( std::is_floating_point_v<plain> ) {
        return static_cast<double>( value );
    } else if constexpr( std::is_convertible_v<const plain &, std::string_view> ) {
        return std::string( std::string_view( value ) );
    } else {
        return std::nullopt;
    }
}

/// `format` with the values of `values`; nullopt when the format has no key or a value is not renderable.
template<typename ...Args>
auto from_format( const translation &format,
                  const Args &... values ) -> std::optional<localised_text>
{
    const auto key = format.key();
    const auto converted = std::vector<std::optional<localised_text::arg>> { to_arg( values )... };
    if( !key || std::ranges::any_of( converted, []( const auto & value ) { return !value; } ) ) {
        return std::nullopt;
    }
    auto result = localised_text{ .key = key->msgid, .ctx = key->context };
    std::ranges::transform( converted, std::back_inserter( result.args ), []( const auto & value ) { return *value; } );
    return result;
}

} // namespace localised
