#pragma once

#include "cursesdef.h"

#include <cstdint>
#include <memory>

class Character;

/** Gets size of single width terminal unit size value in pixels. */
auto termx_to_pixel_value() -> int;
/** Gets size of single height terminal unit size value in pixels. */
auto termy_to_pixel_value() -> int;

struct character_preview_window {
    enum OrientationType : std::uint8_t { TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT };
    struct Margin {
        int left = 0;
        int right = 0;
        int top = 0;
        int bottom = 0;
    };
    struct Orientation {
        OrientationType type = TOP_RIGHT;
        Margin margin = Margin{};
    };
    struct prepare_options {
        int nlines;
        int ncols;
        const Orientation *orientation;
        int hide_below_ncols;
    };

    virtual ~character_preview_window() = default;
    virtual auto init( Character *character ) -> void = 0;
    virtual auto prepare( const prepare_options &options ) -> void = 0;
    virtual auto zoom_in() -> void = 0;
    virtual auto zoom_out() -> void = 0;
    virtual auto toggle_clothes() -> void = 0;
    virtual auto display() const -> void = 0;
    virtual auto clear() const -> void = 0;
    virtual auto clothes_showing() const -> bool = 0;
};

namespace game_client
{
using character_preview_factory = auto( * )() -> std::unique_ptr<character_preview_window>;
auto set_character_preview_factory( character_preview_factory factory ) -> void;
auto make_character_preview() -> std::unique_ptr<character_preview_window>;
auto install_tiles_character_preview() -> void;
} // namespace game_client
