#include "get_version.h" // IWYU pragma: associated

#include <string>
#include <string_view>

#if (defined(_WIN32) || defined(MINGW)) && !defined(GIT_VERSION) && !defined(CROSS_LINUX) && !defined(_MSC_VER)

#ifndef VERSION
#define VERSION "0.1"
#endif

#else

#include "version.h"

#endif

namespace
{
constexpr auto compile_date = std::string_view { __DATE__ };

auto month_number( const std::string_view month ) -> std::string_view
{
    if( month == "Jan" ) {
        return "01";
    }
    if( month == "Feb" ) {
        return "02";
    }
    if( month == "Mar" ) {
        return "03";
    }
    if( month == "Apr" ) {
        return "04";
    }
    if( month == "May" ) {
        return "05";
    }
    if( month == "Jun" ) {
        return "06";
    }
    if( month == "Jul" ) {
        return "07";
    }
    if( month == "Aug" ) {
        return "08";
    }
    if( month == "Sep" ) {
        return "09";
    }
    if( month == "Oct" ) {
        return "10";
    }
    if( month == "Nov" ) {
        return "11";
    }
    if( month == "Dec" ) {
        return "12";
    }
    return "00";
}

auto build_date_iso() -> std::string
{
#if defined(BUILD_TIMESTAMP)
    auto ts = std::string_view { BUILD_TIMESTAMP };
    if( ts.size() >= 10 ) {
        return std::string( ts.substr( 0, 10 ) );
    }
#endif

    if( compile_date.size() < 11 ) {
        return "0000-00-00";
    }

    const auto year = compile_date.substr( 7, 4 );
    const auto month = month_number( compile_date.substr( 0, 3 ) );
    auto day = std::string( compile_date.substr( 4, 2 ) );
    if( !day.empty() && day[0] == ' ' ) {
        day[0] = '0';
    }
    return std::string( year ) + "-" + std::string( month ) + "-" + day;
}
} // namespace

const char *getVersionString()
{
    static const std::string version_string = []() {
        return std::string( VERSION ) + " (" + build_date_iso() + ")";
    }
    ();
    return version_string.c_str();
}
