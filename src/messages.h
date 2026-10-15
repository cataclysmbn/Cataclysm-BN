#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "cached_options.h"
#include "message_types.h"
#include "string_formatter.h"
#include "enums.h"
#include "debug.h"
#include "localised_text.h"

class JsonOut;
class JsonObject;
class translation;

namespace catacurses
{
class window;
} // namespace catacurses

namespace Messages
{
std::vector<std::pair<std::string, std::string>> recent_messages( size_t count );
void add_msg( std::string msg );
void add_msg( const game_message_params &params, std::string msg );
/// A line that also carries its untranslated format, for a client to render in its own language.
/// `msg` stays the rendering the native log and the saves keep.
void add_msg( const game_message_params &params, std::string msg,
              std::optional<localised_text> localised );
void clear_messages();
void deactivate();
size_t size();
bool has_undisplayed_messages();
void display_messages();
void display_messages( const catacurses::window &ipk_target, int left, int top, int right,
                       int bottom );
void serialize( JsonOut &json );
void deserialize( const JsonObject &json );
} // namespace Messages

void add_msg( std::string msg );
template<typename ...Args>
inline void add_msg( const std::string &msg, Args &&... args )
{
    return add_msg( string_format( msg, std::forward<Args>( args )... ) );
}
template<typename ...Args>
inline void add_msg( const char *const msg, Args &&... args )
{
    return add_msg( string_format( msg, std::forward<Args>( args )... ) );
}
template<typename ...Args>
inline void add_msg( const translation &msg, Args &&... args )
{
    return Messages::add_msg( m_neutral, string_format( msg, args... ),
                              localised::from_format( msg, args... ) );
}

void add_msg( const game_message_params &params, std::string msg );
template<typename ...Args>
inline void add_msg( const game_message_params &params, const std::string &msg, Args &&... args )
{
    if( params.type == m_debug && !debug_mode ) {
        return;
    }
    return add_msg( params, string_format( msg, std::forward<Args>( args )... ) );
}
template<typename ...Args>
inline void add_msg( const game_message_params &params, const char *const msg, Args &&... args )
{
    if( params.type == m_debug && !debug_mode ) {
        return;
    }
    return add_msg( params, string_format( msg, std::forward<Args>( args )... ) );
}
template<typename ...Args>
inline void add_msg( const game_message_params &params, const translation &msg, Args &&... args )
{
    if( params.type == m_debug && !debug_mode ) {
        return;
    }
    return Messages::add_msg( params, string_format( msg, args... ),
                              localised::from_format( msg, args... ) );
}

