#include "message_types.h"
#include "string_formatter.h"

const std::vector<std::pair<game_message_type, const char *>> &msg_type_and_names()
{
    static const std::vector<std::pair<game_message_type, const char *>> type_n_names = {
        { m_good, translate_marker_context( "message type", "good" ) },
        { m_bad, translate_marker_context( "message type", "bad" ) },
        { m_mixed, translate_marker_context( "message type", "mixed" ) },
        { m_warning, translate_marker_context( "message type", "warning" ) },
        { m_info, translate_marker_context( "message type", "info" ) },
        { m_neutral, translate_marker_context( "message type", "neutral" ) },
        { m_debug, translate_marker_context( "message type", "debug" ) },
    };
    return type_n_names;
}

bool msg_type_from_name( game_message_type &type, const std::string &name )
{
    for( const auto &p : msg_type_and_names() ) {
        if( name == pgettext( "message type", p.second ) ) {
            type = p.first;
            return true;
        }
    }
    return false;
}