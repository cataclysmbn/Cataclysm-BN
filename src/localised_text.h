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
/// the values of its placeholders, in order. The wire form is `{key, ctx?, plural?, plural_key?, args?}`.
struct localised_text {
    /// A number, a string, or a message of its own: the one element of the vector.
    using arg = std::variant<std::int64_t, double, std::string, std::vector<localised_text>>;

    std::string key;
    std::optional<std::string> ctx;
    /// The count that selects the plural form.
    std::optional<double> plural;
    /// The msgid of the plural form, which a language with no catalogue shows for a count other than one.
    std::optional<std::string> plural_key;
    std::vector<arg> args;

    auto operator==( const localised_text & ) const -> bool = default; // *NOPAD*
};

namespace localised
{

/// `text` as the value of a placeholder.
inline auto nested( localised_text text ) -> localised_text::arg { return std::vector{ std::move( text ) }; }

/// `text` with the msgids it is looked up by and the count that picks its form; nullopt when it needs no translation.
inline auto of( const translation &text, const int count = 1 ) -> std::optional<localised_text>
{
    const auto key = text.key();
    if( !key ) { return std::nullopt; }
    return localised_text{ .key = key->msgid, .ctx = key->context,
                           .plural = key->msgid_plural ? std::optional{ static_cast<double>( count ) } :
                                     std::nullopt,
                           .plural_key = key->msgid_plural };
}

/// A value of a message that is a name: shown in the engine's language, worded by a client from its catalogue.
struct name {
    std::string shown;
    std::optional<localised_text> text;
};

/// `text` in its form for `count`.
inline auto name_of( const translation &text, const int count = 1 ) -> name
{
    return { .shown = text.translated( count ), .text = of( text, count ) };
}

/// What the native rendering of a message prints for `value`.
template<typename T>
auto shown( const T &value ) -> const T & { return value; } // *NOPAD*
inline auto shown( const name &value ) -> const std::string & { return value.shown; } // *NOPAD*

/// A placeholder value a client can render: a number, a string or a message. Anything else keeps the line a
/// plain string.
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
    } else if constexpr( std::is_same_v<plain, translation> ) {
        const auto text = of( value );
        return text ? nested( *text ) : localised_text::arg{ value.translated() };
    } else if constexpr( std::is_same_v<plain, name> ) {
        return value.text ? nested( *value.text ) : localised_text::arg{ value.shown };
    } else {
        return std::nullopt;
    }
}

/// `format` with the values of `values`; nullopt when the format has no key or a value is not renderable. The
/// format reads in its singular form, as `string_format` renders it.
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
