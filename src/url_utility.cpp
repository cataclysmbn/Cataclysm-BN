#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include "debug.h"
#include "url_utility.h"

void open_url( const std::string &url )
{
    static const std::string executable =
#if defined(_WIN32)
        "start \"\"";
#elif defined(__linux__) || defined(__FreeBSD__)
        "xdg-open";
#elif defined(__APPLE__)
        "open";
#elif defined(__ANDROID__)
        "am start -a android.intent.action.VIEW -d";
#endif
    const std::string command = executable + " \"" + url + "\"";

    const int exitcode = std::system( command.data() );
    if( exitcode != 0 ) {
        debugmsg( "Failed to open URL: %s\nAttemped command was: %s", url, command );
    }
}

std::string encode_url( const std::string &text )
{
    std::ostringstream escaped;
    escaped.fill( '0' );
    escaped << std::hex;

    const auto accepted = std::string{"-_.~"};
    const auto is_accepted = [&accepted]( const char c ) {
        return std::isalnum( c ) || accepted.find( c ) != std::string::npos;
    };

    for( const auto &c : text ) {
        if( is_accepted( c ) ) {
            escaped << c;
        } else {
            escaped << std::uppercase << '%' << std::setw( 2 )
                    << static_cast<int>( static_cast<unsigned char>( c ) )
                    << std::nouppercase;
        }
    }

    return escaped.str();
}
