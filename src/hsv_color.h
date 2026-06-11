#pragma once

#include "color.h"
#include "data_vars_cvt.h"
#include "hash_utils.h"
#include "json.h"
#include "line.h"
#include <unordered_map>

#if defined(TILES)
#include "sdl_wrappers.h"
#else
#include <cstdint>
#endif

struct RGBColor {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;

    constexpr RGBColor() = default;
    constexpr RGBColor( const uint8_t r, const uint8_t g, const uint8_t b, const uint8_t a ) : r{r}, g{g},
        b{b}, a{a} {}
#if defined(TILES)
    constexpr RGBColor( const SDL_Color &c ) : r( c.r ), g( c.g ), b( c.b ), a( c.a ) {}
    constexpr operator SDL_Color() const {
        return SDL_Color{ r, g, b, a };
    }
#endif
    void serialize( JsonOut & ) const;
    void deserialize( JsonIn & );

    static std::optional<RGBColor> try_parse( const std::string &str );
    static std::pair<RGBColor, std::string> random_named( std::string fuzzy_match = "" );
    static std::unordered_map<RGBColor, std::string> get_all_named_colors();

    static void load_named_color( const JsonObject &jo, const std::string &src );
    static void unload_names();

    std::string friendly_name() const;

    bool operator==( const RGBColor &other ) const {
        return r == other.r && g == other.g && b == other.b && a == other.a;
    }
};

struct RGBColorPair {
    RGBColor bg;
    RGBColor fg;
    void serialize( JsonOut & ) const;
    void deserialize( JsonIn & );
};

struct HSVColor {
    /// Hue: 0~393210 ( ( 1 << 16 ) - 1) * 6)
    uint32_t H;
    // Saturation: 0~65535
    uint16_t S;
    // Value: 0~255
    uint8_t V;
    // Alpha: 0~255
    uint8_t A;
};

//auto rgb_from_hex_string( std::string str ) -> RGBColor;
auto curses_color_to_RGB( const nc_color &color ) -> RGBColor;
auto hsv2rgb( HSVColor color ) -> RGBColor;
auto rgb2hsv( RGBColor color ) -> HSVColor;

template<> struct std::hash<RGBColor> {
    std::size_t operator()( const RGBColor &color ) const noexcept {
        std::size_t hash = 0;
        cata::hash_combine( hash, color.r );
        cata::hash_combine( hash, color.g );
        cata::hash_combine( hash, color.b );
        cata::hash_combine( hash, color.a );
        return hash;
    }
};

template<> struct std::hash<HSVColor> {
    std::size_t operator()( const HSVColor &color ) const noexcept {
        std::size_t hash = 0;
        cata::hash_combine( hash, color.H );
        cata::hash_combine( hash, color.S );
        cata::hash_combine( hash, color.V );
        cata::hash_combine( hash, color.A );
        return hash;
    }
};

namespace detail
{
struct RGBColorConverter {
    using value_type = RGBColor;
    bool operator()( const std::string &str, RGBColor &col ) const;
    bool operator()( const RGBColor &col, std::string &str ) const;
    static bool should_cache( const std::string &s, const RGBColor &c );
};
} // namespace detail

template<>
struct data_vars::type_converter<RGBColor> {
    using type = cached_converter<::detail::RGBColorConverter, 512>;
};
