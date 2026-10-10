#pragma once

#include "point.h"

namespace game_ui
{
void init_ui();
/// Resizes the terminal as a window resize would, so the terrain window shows `cells`.
auto resize_terrain_window( point cells ) -> void;
} // namespace game_ui

// defined in sdltiles.cpp
void to_map_font_dim_width( int &w );
void to_map_font_dim_height( int &h );
void to_map_font_dimension( int &w, int &h );
void from_map_font_dimension( int &w, int &h );
void to_overmap_font_dimension( int &w, int &h );
void reinitialize_framebuffer( bool force_invalidate = false );


