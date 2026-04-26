#include "string_utils.h"

#include "cached_options.h"
#include "catacharset.h"
#include "color.h"
#include "name.h"
#include "pinyin.h"
#include "translations.h"

#include <algorithm>
#include <locale>
#include <sstream>

bool lcmatch( const std::string &str, const std::string &qry )
{
    auto temp_locale = std::locale{};
    if( temp_locale.name() != "en_US.UTF-8" && temp_locale.name() != "C" ) {
        auto &f = std::use_facet<std::ctype<wchar_t>>( temp_locale );
        std::wstring wneedle = utf8_to_wstr( qry );
        std::wstring whaystack = utf8_to_wstr( str );

        f.tolower( whaystack.data(), whaystack.data() + whaystack.size() );
        f.tolower( wneedle.data(), wneedle.data() + wneedle.size() );

        if( whaystack.find( wneedle ) != std::wstring::npos ) {
            return true;
        }
    } else {
        std::string needle;
        needle.reserve( qry.size() );
        std::transform( qry.begin(), qry.end(), std::back_inserter( needle ), tolower );

        std::string haystack;
        haystack.reserve( str.size() );
        std::transform( str.begin(), str.end(), std::back_inserter( haystack ), tolower );

        if( haystack.find( needle ) != std::string::npos ) {
            return true;
        }
    }

    if( use_pinyin_search ) {
        return pinyin::pinyin_match( utf8_to_utf32( to_lower_case( str ) ),
                                     utf8_to_utf32( to_lower_case( qry ) ) );
    }

    return false;
}

bool lcmatch( const translation &str, const std::string &qry )
{
    return lcmatch( str.translated(), qry );
}

bool lcequal( const std::string &str1, const std::string &str2 )
{
    return to_lower_case( str1 ) == to_lower_case( str2 );
}

bool match_include_exclude( const std::string &text, std::string filter )
{
    size_t iPos;
    bool found = false;

    if( filter.empty() ) {
        return false;
    }

    do {
        iPos = filter.find( ',' );

        std::string term = iPos == std::string::npos ? filter : filter.substr( 0, iPos );
        const bool exclude = term.substr( 0, 1 ) == "-";
        if( exclude ) {
            term = term.substr( 1 );
        }

        if( ( !found || exclude ) && lcmatch( text, term ) ) {
            if( exclude ) {
                return false;
            }

            found = true;
        }

        if( iPos != std::string::npos ) {
            filter = filter.substr( iPos + 1, filter.size() );
        }
    } while( iPos != std::string::npos );

    return found;
}

std::string join( const std::vector<std::string> &strings, const std::string &joiner )
{
    std::ostringstream buffer;

    for( auto a = strings.begin(); a != strings.end(); ++a ) {
        if( a != strings.begin() ) {
            buffer << joiner;
        }
        buffer << *a;
    }
    return buffer.str();
}

std::vector<std::string> string_split( const std::string &text_in, char delim_in )
{
    std::vector<std::string> elems;

    if( text_in.empty() ) {
        return elems; // Well, that was easy.
    }

    std::stringstream ss( text_in );
    std::string item;
    while( std::getline( ss, item, delim_in ) ) {
        elems.push_back( item );
    }

    if( text_in.back() == delim_in ) {
        elems.emplace_back( "" );
    }

    return elems;
}

int ci_find_substr( const std::string &str1, const std::string &str2 )
{
    std::locale loc = std::locale();

    std::string::const_iterator it = std::search( str1.begin(), str1.end(), str2.begin(), str2.end(),
    [&]( const char str1_in, const char str2_in ) {
        return std::toupper( str1_in, loc ) == std::toupper( str2_in, loc );
    } );
    if( it != str1.end() ) {
        return it - str1.begin();
    } else {
        return -1;    // not found
    }
}

bool wildcard_match( const std::string &text_in, const std::string &pattern_in )
{
    std::string text = text_in;

    if( text.empty() ) {
        return false;
    } else if( text == "*" ) {
        return true;
    }

    std::vector<std::string> pattern = string_split( wildcard_trim_rule( pattern_in ), '*' );

    if( pattern.size() == 1 ) { // no * found
        return ( text.length() == pattern[0].length() && ci_find_substr( text, pattern[0] ) != -1 );
    }

    for( auto it = pattern.begin(); it != pattern.end(); ++it ) {
        if( it == pattern.begin() && !it->empty() ) {
            if( text.length() < it->length() ||
                ci_find_substr( text.substr( 0, it->length() ), *it ) == -1 ) {
                return false;
            }

            text = text.substr( it->length(), text.length() - it->length() );
        } else if( it == pattern.end() - 1 && !it->empty() ) {
            if( text.length() < it->length() ||
                ci_find_substr( text.substr( text.length() - it->length(),
                                             it->length() ), *it ) == -1 ) {
                return false;
            }
        } else {
            if( !( *it ).empty() ) {
                int pos = ci_find_substr( text, *it );
                if( pos == -1 ) {
                    return false;
                }

                text = text.substr( pos + static_cast<int>( it->length() ),
                                    static_cast<int>( text.length() ) - pos );
            }
        }
    }

    return true;
}

std::string wildcard_trim_rule( const std::string &pattern_in )
{
    std::string pattern = pattern_in;
    size_t pos = pattern.find( "**" );

    //Remove all double ** in pattern
    while( pos != std::string::npos ) {
        pattern = pattern.substr( 0, pos ) + pattern.substr( pos + 1, pattern.length() - pos - 1 );
        pos = pattern.find( "**" );
    }

    return pattern;
}

template<typename Prep>
std::string trim( const std::string &s, Prep prep )
{
    auto wsfront = std::find_if_not( s.begin(), s.end(), [&prep]( int c ) {
        return prep( c );
    } );
    return std::string( wsfront, std::find_if_not( s.rbegin(),
    std::string::const_reverse_iterator( wsfront ), [&prep]( int c ) {
        return prep( c );
    } ).base() );
}

std::string trim( const std::string &s )
{
    return trim( s, []( int c ) {
        return isspace( c );
    } );
}

std::string trim_punctuation_marks( const std::string &s )
{
    return trim( s, []( int c ) {
        return ispunct( c );
    } );
}

using char_t = std::string::value_type;
std::string to_upper_case( const std::string &s )
{
    auto temp_locale = std::locale();
    if( temp_locale.name() != "en_US.UTF-8" && temp_locale.name() != "C" ) {
        const auto &f = std::use_facet<std::ctype<wchar_t>>( temp_locale );
        std::wstring wstr = utf8_to_wstr( s );
        f.toupper( wstr.data(), wstr.data() + wstr.size() );
        return wstr_to_utf8( wstr );
    }
    std::string res;
    std::transform( s.begin(), s.end(), std::back_inserter( res ), [&temp_locale]( char_t ch ) {
        return std::use_facet<std::ctype<char_t>>( temp_locale ).toupper( ch );
    } );
    return res;
}

std::string to_lower_case( const std::string &s )
{
    auto temp_locale = std::locale();
    if( temp_locale.name() != "en_US.UTF-8" && temp_locale.name() != "C" ) {
        const auto &f = std::use_facet<std::ctype<wchar_t>>( temp_locale );
        std::wstring wstr = utf8_to_wstr( s );
        f.tolower( wstr.data(), wstr.data() + wstr.size() );
        return wstr_to_utf8( wstr );
    }
    std::string res;
    std::transform( s.begin(), s.end(), std::back_inserter( res ), [&temp_locale]( char_t ch ) {
        return std::use_facet<std::ctype<char_t>>( temp_locale ).tolower( ch );
    } );
    return res;
}

void replace_name_tags( std::string &input )
{
    // these need to replace each tag with a new randomly generated name
    while( input.find( "<full_name>" ) != std::string::npos ) {
        replace_first( input, "<full_name>", Name::get( nameIsFullName ) );
    }
    while( input.find( "<family_name>" ) != std::string::npos ) {
        replace_first( input, "<family_name>", Name::get( nameIsFamilyName ) );
    }
    while( input.find( "<given_name>" ) != std::string::npos ) {
        replace_first( input, "<given_name>", Name::get( nameIsGivenName ) );
    }
    while( input.find( "<town_name>" ) != std::string::npos ) {
        replace_first( input, "<town_name>", Name::get( nameIsTownName ) );
    }
}

void replace_city_tag( std::string &input, const std::string &name )
{
    input = replace_all( input, "<city>", name );
}

void replace_first( std::string &input, const std::string &what, const std::string &with )
{
    if( what.empty() || what == with ) {
        return;
    }
    size_t len = what.length();
    size_t offset = input.find( what );
    if( offset != std::string::npos ) {
        input.replace( offset, len, with );
    }
}

std::string replace_all( std::string input, const std::string &what, const std::string &with )
{
    std::string text = std::move( input );

    // Check if there's something to replace (mandatory) and it's necessary (optional)
    // Second condition assumes that text is much longer than both &before and &after.
    if( what.empty() || what == with ) {
        return text;
    }

    const size_t what_len = what.length();
    const size_t with_len = with.length();
    size_t pos = 0;

    while( ( pos = text.find( what, pos ) ) != std::string::npos ) {
        text.replace( pos, what_len, with );
        pos += with_len;
    }

    return text;
}

std::string replace_colors( std::string text )
{
    static const std::vector<std::pair<std::string, std::string>> info_colors = {
        {"info", get_all_colors().get_name( c_cyan )},
        {"stat", get_all_colors().get_name( c_light_blue )},
        {"header", get_all_colors().get_name( c_magenta )},
        {"bold", get_all_colors().get_name( c_white )},
        {"dark", get_all_colors().get_name( c_dark_gray )},
        {"good", get_all_colors().get_name( c_green )},
        {"bad", get_all_colors().get_name( c_red )},
        {"neutral", get_all_colors().get_name( c_yellow )}
    };

    for( auto &elem : info_colors ) {
        text = replace_all( text, "<" + elem.first + ">", "<color_" + elem.second + ">" );
        text = replace_all( text, "</" + elem.first + ">", "</color>" );
    }

    return text;
}

std::string &capitalize_letter( std::string &str, size_t n )
{
    char c = str[n];
    if( !str.empty() && c >= 'a' && c <= 'z' ) {
        c += 'A' - 'a';
        str[n] = c;
    }

    return str;
}

std::string trim_whitespaces( const std::string &str )
{
    // Source: https://stackoverflow.com/a/1798170

    // NOLINTNEXTLINE(cata-text-style)
    const std::string whitespace = "\t ";
    const auto str_begin = str.find_first_not_of( whitespace );
    if( str_begin == std::string::npos ) {
        return "";    // no content
    }

    const auto str_end = str.find_last_not_of( whitespace );
    const auto str_range = str_end - str_begin + 1;

    return str.substr( str_begin, str_range );
}
