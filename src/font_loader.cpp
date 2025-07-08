#include "font_loader.h"

#include <algorithm>

void ensure_unifont_loaded( std::vector<std::string> &font_list )
{
    if( std::ranges::find( font_list, "unifont" ) == font_list.end() ) {
        font_list.emplace_back( PATH_INFO::fontdir() + "unifont.ttf" );
    }
}

void font_loader::load_throws( const std::string &path )
{
    try {
        cata_ifstream stream = std::move( cata_ifstream().mode( cata_ios_mode::binary ).open( path ) );
        JsonIn json( *stream );
        JsonObject config = json.get_object();
        if( config.has_string( "typeface" ) ) {
            typeface.emplace_back( config.get_string( "typeface" ) );
        } else {
            config.read( "typeface", typeface );
        }
        if( config.has_string( "map_typeface" ) ) {
            map_typeface.emplace_back( config.get_string( "map_typeface" ) );
        } else {
            config.read( "map_typeface", map_typeface );
        }
        if( config.has_string( "overmap_typeface" ) ) {
            overmap_typeface.emplace_back( config.get_string( "overmap_typeface" ) );
        } else {
            config.read( "overmap_typeface", overmap_typeface );
        }

        ensure_unifont_loaded( typeface );
        ensure_unifont_loaded( map_typeface );
        ensure_unifont_loaded( overmap_typeface );

    } catch( const std::exception &err ) {
        throw std::runtime_error( std::string( "loading font settings from " ) + path + " failed: " +
                                  err.what() );
    }
}

void font_loader::load()
{
    const std::string user_fontconfig = PATH_INFO::user_fontconfig();
    const std::string fontconfig = PATH_INFO::fontconfig();
    bool try_user = true;
    if( !file_exist( user_fontconfig ) ) {
        if( !copy_file( fontconfig, user_fontconfig ) ) {
            try_user = false;
            DebugLog( DL::Error, DC::SDL ) << "failed to create user font config file "
                                           << user_fontconfig;
        }
    }
    if( try_user ) {
        font_loader copy = *this;
        try {
            load_throws( user_fontconfig );
            return;
        } catch( const std::exception &e ) {
            DebugLog( DL::Error, DC::SDL ) << e.what();
        }
        *this = copy;
    }
    try {
        load_throws( fontconfig );
    } catch( const std::exception &e ) {
        DebugLog( DL::Error, DC::SDL ) << e.what();
        abort();
    }
}
