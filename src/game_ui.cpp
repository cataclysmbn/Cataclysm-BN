#include "game_ui.h"
#include "client_presentation.h"

void reinitialize_framebuffer( const bool force_invalidate )
{
    game_client::presentation().invalidate_framebuffer( force_invalidate );
}
void to_map_font_dimension( int &width, int &height )
{
    const auto size = game_client::presentation().font_dimensions( {width, height},
                      game_client::font_space::terminal, game_client::font_space::map );
    width = size.x;
    height = size.y;
}
void to_map_font_dim_width( int &width ) { auto height = 0; to_map_font_dimension( width, height ); }
void to_map_font_dim_height( int &height ) { auto width = 0; to_map_font_dimension( width, height ); }
void from_map_font_dimension( int &width, int &height )
{
    const auto size = game_client::presentation().font_dimensions( {width, height},
                      game_client::font_space::map, game_client::font_space::terminal );
    width = size.x;
    height = size.y;
}
void to_overmap_font_dimension( int &width, int &height )
{
    const auto size = game_client::presentation().font_dimensions( {width, height},
                      game_client::font_space::terminal, game_client::font_space::overmap );
    width = size.x;
    height = size.y;
}
