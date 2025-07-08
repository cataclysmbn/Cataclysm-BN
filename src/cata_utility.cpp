#include "cata_utility.h"
#include "fstream_utils.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <exception>
#include <fstream>
#include <iterator>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

#include "debug.h"
#include "enum_conversions.h"
#include "filesystem.h"
#include "json.h"
#include "options.h"
#include "output.h"
#include "rng.h"
#include "translations.h"
#include "units.h"

#if defined (_WIN32) && !defined (_MSC_VER)
#include <ext/stdio_filebuf.h>
#endif

static double pow10( unsigned int n )
{
    double ret = 1;
    double tmp = 10;
    while( n ) {
        if( n & 1 ) {
            ret *= tmp;
        }
        tmp *= tmp;
        n >>= 1;
    }
    return ret;
}

double round_up( double val, unsigned int dp )
{
    // Some implementations of std::pow does not return the accurate result even
    // for small powers of 10, so we use a specialized routine to calculate them.
    const double denominator = pow10( dp );
    return std::ceil( denominator * val ) / denominator;
}

int modulo( int v, int m )
{
    // C++11: negative v and positive m result in negative v%m (or 0),
    // but this is supposed to be mathematical modulo: 0 <= v%m < m,
    const int r = v % m;
    // Adding m in that (and only that) case.
    return r >= 0 ? r : r + ( m * ( 1 - r / m ) );
}

bool isBetween( int test, int down, int up )
{
    return test > down && test < up;
}

// --- Library functions ---
// This stuff could be moved elsewhere, but there
// doesn't seem to be a good place to put it right now.

double logarithmic( double t )
{
    return 1 / ( 1 + std::exp( -t ) );
}

double logarithmic_range( int min, int max, int pos )
{
    const double LOGI_CUTOFF = 4;
    const double LOGI_MIN = logarithmic( -LOGI_CUTOFF );
    const double LOGI_MAX = logarithmic( +LOGI_CUTOFF );
    const double LOGI_RANGE = LOGI_MAX - LOGI_MIN;

    if( min >= max ) {
        debugmsg( "Invalid interval (%d, %d).", min, max );
        return 0.0;
    }

    // Anything beyond (min,max) gets clamped.
    if( pos <= min ) {
        return 1.0;
    } else if( pos >= max ) {
        return 0.0;
    }

    // Normalize the pos to [0,1]
    double range = max - min;
    double unit_pos = ( pos - min ) / range;

    // Scale and flip it to [+LOGI_CUTOFF,-LOGI_CUTOFF]
    double scaled_pos = LOGI_CUTOFF - 2 * LOGI_CUTOFF * unit_pos;

    // Get the raw logistic value.
    double raw_logistic = logarithmic( scaled_pos );

    // Scale the output to [0,1]
    return ( raw_logistic - LOGI_MIN ) / LOGI_RANGE;
}

double normal_cdf( double x, double mean, double stddev )
{
    return 0.5 * ( 1.0 + std::erf( ( x - mean ) / ( stddev * M_SQRT2 ) ) );
}

int bound_mod_to_vals( int val, int mod, int max, int min )
{
    if( val + mod > max && max != 0 ) {
        mod = std::max( max - val, 0 );
    }
    if( val + mod < min && min != 0 ) {
        mod = std::min( min - val, 0 );
    }
    return mod;
}

double clamp_to_width( double value, int width, int &scale )
{
    return clamp_to_width( value, width, scale, nullptr );
}

double clamp_to_width( double value, int width, int &scale, bool *out_truncated )
{
    if( out_truncated != nullptr ) {
        *out_truncated = false;
    }
    if( value >= std::pow( 10.0, width ) ) {
        // above the maximum number we can fit in the width without decimal
        // show the biggest number we can without decimal
        // flag as truncated
        value = std::pow( 10.0, width ) - 1.0;
        scale = 0;
        if( out_truncated != nullptr ) {
            *out_truncated = true;
        }
    } else if( scale > 0 ) {
        for( int s = 1; s <= scale; s++ ) {
            // 1 decimal separator + "s"
            int scale_width = 1 + s;
            if( width > scale_width && value >= std::pow( 10.0, width - scale_width ) ) {
                // above the maximum number we can fit in the width with "s" decimals
                // show this number with one less decimal than "s"
                scale = s - 1;
                break;
            }
        }
    }
    return value;
}

float multi_lerp( const std::vector<std::pair<float, float>> &points, float x )
{
    size_t i = 0;
    while( i < points.size() && points[i].first <= x ) {
        i++;
    }

    if( i == 0 ) {
        return points.front().second;
    } else if( i >= points.size() ) {
        return points.back().second;
    }

    // How far are we along the way from last threshold to current one
    const float t = ( x - points[i - 1].first ) /
                    ( points[i].first - points[i - 1].first );

    // Linear interpolation of values at relevant thresholds
    return ( t * points[i].second ) + ( ( 1 - t ) * points[i - 1].second );
}

static std::ios_base::openmode cata_ios_mode_to_std( std::ios_base::openmode dir, cata_ios_mode m )
{
    std::ios_base::openmode ret = dir;
    if( static_cast<int>( m ) & static_cast<int>( cata_ios_mode::binary ) ) {
        ret |= std::ios_base::binary;
    }
    if( static_cast<int>( m ) & static_cast<int>( cata_ios_mode::app ) ) {
        ret |= std::ios_base::app;
    }
    return ret;
}

#if defined (_WIN32) && !defined (_MSC_VER)
// On Linux/MacOS, UTF-8 paths work out of the box, and we pass the string as is.
// On Windows, narrow API does not recognize paths encoded with UTF-8,
// so we have to jump through hoops to use wide API:
// * with Visual Studio, there's a non-standard fstream constructor that
//   accepts wstring for path; we just need to convert UTF-8 to UTF-16.
// * when cross-compiling with mingw64, there's a non-standard __gnu_cxx::stdio_filebuf
//   that allows creating fstream from FILE, and _wfopen that allows opening FILE
//   using wide string for path.
// Although Windows 10 insider build 17035 (November 2017) enables narrow API to use UTF-8
// paths, we can't rely on it here for backwards compatibility.

static std::wstring cata_ios_mode_to_c( bool out, cata_ios_mode m )
{
    std::wstring ret;
    if( out ) {
        if( static_cast<int>( m ) & static_cast<int>( cata_ios_mode::app ) ) {
            ret = L"a";
        } else {
            ret = L"w";
        }
    } else {
        ret = L"r";
    }
    if( static_cast<int>( m ) & static_cast<int>( cata_ios_mode::binary ) ) {
        ret += L"b";
    }
    return ret;
}

cata_ofstream &cata_ofstream::operator=( cata_ofstream &&x )
noexcept
{
    _stream = std::move( x._stream );
    _buffer = std::move( x._buffer );
    _file = x._file;
    x._file = nullptr;
    _mode = x._mode;
    return *this;
}

cata_ofstream &cata_ofstream::open( const std::string &path )
{
    std::wstring mode = cata_ios_mode_to_c( true, _mode );

    _file = _wfopen( utf8_to_wstr( path ).c_str(), mode.c_str() );
    if( !_file ) {
        // failed
        return *this;
    }

    std::ios_base::openmode smode = cata_ios_mode_to_std( std::ios_base::out, _mode );

    _buffer = std::make_unique<__gnu_cxx::stdio_filebuf<char>>( _file, smode );
    _stream = std::make_unique<std::ostream>( &*_buffer );

    return *this;
}

bool cata_ofstream::is_open()
{
    return _file;
}

void cata_ofstream::close()
{
    if( _stream ) {
        _stream->flush();
        _stream.reset();
    }
    _buffer.reset();
    if( _file ) {
        fclose( _file );
        _file = nullptr;
    }
}

#else // defined (_WIN32) && !defined (_MSC_VER)

cata_ofstream &cata_ofstream::operator=( cata_ofstream &&x )
noexcept
{
    _stream = std::move( x._stream );
    _mode = x._mode;
    return *this;
}

cata_ofstream &cata_ofstream::open( const std::string &path )
{
    std::ios_base::openmode mode = cata_ios_mode_to_std( std::ios_base::out, _mode );

#if defined (_MSC_VER)
    _stream = std::make_unique<std::ofstream>( utf8_to_wstr( path ), mode );
#else
    _stream = std::make_unique<std::ofstream>( path, mode );
#endif
    return *this;
}

bool cata_ofstream::is_open()
{
    return _stream && _stream->is_open();
}

void cata_ofstream::close()
{
    if( _stream ) {
        _stream->close();
        _stream.reset();
    }
}

#endif // defined (_WIN32) && !defined (_MSC_VER)

cata_ofstream::cata_ofstream() = default;

cata_ofstream::cata_ofstream( cata_ofstream &&x )
noexcept
{
    *this = std::move( x );
}

cata_ofstream::~cata_ofstream()
{
    close();
}

bool cata_ofstream::fail()
{
    return !_stream || _stream->fail();
}

bool cata_ofstream::bad()
{
    return !_stream || _stream->bad();
}

void cata_ofstream::flush()
{
    _stream->flush();
}

std::ostream &cata_ofstream::operator*()
{
    return *_stream;
}

std::ostream *cata_ofstream::operator->()
{
    return &*_stream;
}

#if defined (_WIN32) && !defined (_MSC_VER)

cata_ifstream &cata_ifstream::operator=( cata_ifstream &&x )
noexcept
{
    _stream = std::move( x._stream );
    _buffer = std::move( x._buffer );
    _file = x._file;
    x._file = nullptr;
    _mode = x._mode;
    return *this;
}

cata_ifstream &cata_ifstream::open( const std::string &path )
{
    std::wstring mode = cata_ios_mode_to_c( false, _mode );

    _file = _wfopen( utf8_to_wstr( path ).c_str(), mode.c_str() );
    if( !_file ) {
        // failed
        return *this;
    }

    std::ios_base::openmode smode = cata_ios_mode_to_std( std::ios_base::in, _mode );

    _buffer = std::make_unique<__gnu_cxx::stdio_filebuf<char>>( _file, smode );
    _stream = std::make_unique<std::istream>( &*_buffer );

    return *this;
}

bool cata_ifstream::is_open()
{
    return _file;
}

void cata_ifstream::close()
{
    if( _stream ) {
        _stream.reset();
    }
    _buffer.reset();
    if( _file ) {
        fclose( _file );
        _file = nullptr;
    }
}

#else // defined (_WIN32) && !defined (_MSC_VER)

cata_ifstream &cata_ifstream::operator=( cata_ifstream &&x )
noexcept
{
    _stream = std::move( x._stream );
    _mode = x._mode;
    return *this;
}

cata_ifstream &cata_ifstream::open( const std::string &path )
{
    std::ios_base::openmode mode = cata_ios_mode_to_std( std::ios_base::in, _mode );

#if defined (_MSC_VER)
    _stream = std::make_unique<std::ifstream>( utf8_to_wstr( path ), mode );
#else
    _stream = std::make_unique<std::ifstream>( path, mode );
#endif
    return *this;
}

bool cata_ifstream::is_open()
{
    return _stream && _stream->is_open();
}

void cata_ifstream::close()
{
    if( _stream ) {
        _stream->close();
        _stream.reset();
    }
}

#endif // defined (_WIN32) && !defined (_MSC_VER)

cata_ifstream::cata_ifstream() = default;

cata_ifstream::cata_ifstream( cata_ifstream &&x )
noexcept
{
    *this = std::move( x );
}

cata_ifstream::~cata_ifstream()
{
    close();
}

bool cata_ifstream::fail()
{
    return !_stream || _stream->fail();
}

bool cata_ifstream::bad()
{
    return !_stream || _stream->bad();
}

std::istream &cata_ifstream::operator*()
{
    return *_stream;
}

std::istream *cata_ifstream::operator->()
{
    return &*_stream;
}

/**
 * If fail_message is provided, this method will eat any exceptions and display a popup with the
 * exception detail and the message. If fail_message is not provided, the exception will be
 * propagated.
 *
 * To eat any exceptions and not display a popup, pass the empty string as fail_message.
 *
 * @param path The path to write to.
 * @param writer The function that writes to the file.
 * @param fail_message The message to display if the write fails.
 * @return True if the write was successful, false otherwise.
 */
bool write_to_file( const std::string &path, file_write_fn &writer, const char *const fail_message )
{
    try {
        // Any of the below may throw. ofstream_wrapper will clean up the temporary path on its own.
        ofstream_wrapper fout( path, cata_ios_mode::binary );
        writer( fout.stream() );
        fout.close();
        return true;
    } catch( const std::exception &err ) {
        if( fail_message && fail_message[0] != '\0' ) {
            popup( _( "Failed to write %1$s to \"%2$s\": %3$s" ), fail_message, path.c_str(), err.what() );
        } else if( fail_message == nullptr ) {
            std::throw_with_nested( std::runtime_error( "file write failed: " + path ) );
        }
        return false;
    }
}

ofstream_wrapper::ofstream_wrapper( const std::string &path, const cata_ios_mode mode )
    : path( path )

{
    open( mode );
}

ofstream_wrapper::~ofstream_wrapper()
{
    try {
        close();
    } catch( ... ) {
        // ignored in destructor
    }
}

std::istream &safe_getline( std::istream &ins, std::string &str )
{
    str.clear();
    std::istream::sentry se( ins, true );
    std::streambuf *sb = ins.rdbuf();

    while( true ) {
        int c = sb->sbumpc();
        switch( c ) {
            case '\n':
                return ins;
            case '\r':
                if( sb->sgetc() == '\n' ) {
                    sb->sbumpc();
                }
                return ins;
            case EOF:
                if( str.empty() ) {
                    ins.setstate( std::ios::eofbit );
                }
                return ins;
            default:
                str += static_cast<char>( c );
        }
    }
}

bool read_from_file( const std::string &path, file_read_fn reader, bool optional )
{
    if( optional && !file_exist( path ) ) {
        return false;
    }

    try {
        cata_ifstream fin = std::move( cata_ifstream().mode( cata_ios_mode::binary ).open( path ) );
        if( !fin.is_open() ) {
            throw std::runtime_error( "opening file failed" );
        }
        reader( *fin );
        if( fin.bad() ) {
            throw std::runtime_error( "reading file failed" );
        }
        return true;

    } catch( const std::exception &err ) {
        debugmsg( _( "Failed to read from \"%1$s\": %2$s" ), path.c_str(), err.what() );
        return false;
    }
}

bool read_from_file_json( const std::string &path, file_read_json_fn reader, bool optional )
{
    return read_from_file( path, [&]( std::istream & fin ) {
        JsonIn jsin( fin, path );
        reader( jsin );
    }, optional );
}

void ofstream_wrapper::open( cata_ios_mode mode )
{
    if( dir_exist( path ) ) {
        throw std::runtime_error( "target path is a directory" );
    }

    // Create a *unique* temporary path. No other running program should
    // use this path. If the file exists, it must be of a *former* program
    // instance and can savely be deleted.
    temp_path = path + "." + get_pid_string() + ".temp";

    if( file_exist( temp_path ) ) {
        remove_file( temp_path );
    }

    file_stream = std::move( cata_ofstream().mode( mode ).open( temp_path ) );
    if( !file_stream.is_open() ) {
        throw std::runtime_error( "opening file failed" );
    }
}

void ofstream_wrapper::close()
{
    if( !file_stream.is_open() ) {
        return;
    }

    file_stream.flush();
    bool failed = file_stream.fail();
    file_stream.close();
    if( failed ) {
        // Remove the incomplete or otherwise faulty file (if possible).
        // Failures from it are ignored as we can't really do anything about them.
        remove_file( temp_path );
        throw std::runtime_error( "writing to file failed" );
    }
    if( !rename_file( temp_path, path ) ) {
        // Leave the temp path, so the user can move it if possible.
        throw std::runtime_error( "moving temporary file \"" + temp_path + "\" failed" );
    }
}

std::string obscure_message( const std::string &str, const std::function<char()> &f )
{
    //~ translators: place some random 1-width characters here in your language if possible, or leave it as is
    std::string gibberish_narrow = _( "abcdefghijklmnopqrstuvwxyz" );
    std::string gibberish_wide =
        //~ translators: place some random 2-width characters here in your language if possible, or leave it as is
        _( "に坂索トし荷測のンおク妙免イロコヤ梅棋厚れ表幌" );
    std::wstring w_gibberish_narrow = utf8_to_wstr( gibberish_narrow );
    std::wstring w_gibberish_wide = utf8_to_wstr( gibberish_wide );
    std::wstring w_str = utf8_to_wstr( str );
    // a trailing NULL terminator is necessary for utf8_width function
    char transformation[2] = { 0 };
    for( size_t i = 0; i < w_str.size(); ++i ) {
        transformation[0] = f();
        std::string this_char = wstr_to_utf8( std::wstring( 1, w_str[i] ) );
        // mk_wcwidth, which is used by utf8_width, might return -1 for some values, such as newlines 0x0A
        if( transformation[0] == -1 || utf8_width( this_char ) == -1 ) {
            // Leave unchanged
            continue;
        } else if( transformation[0] == 0 ) {
            // Replace with random character
            if( utf8_width( this_char ) == 1 ) {
                w_str[i] = random_entry( w_gibberish_narrow );
            } else {
                w_str[i] = random_entry( w_gibberish_wide );
            }
        } else {
            // Only support the case e.g. replace current character to symbols like # or ?
            if( utf8_width( transformation ) != 1 ) {
                debugmsg( "target character isn't narrow" );
            }
            // A 2-width wide character in the original string should be replace by two narrow characters
            w_str.replace( i, 1, utf8_to_wstr( std::string( utf8_width( this_char ), transformation[0] ) ) );
        }
    }
    std::string result = wstr_to_utf8( w_str );
    if( utf8_width( str ) != utf8_width( result ) ) {
        debugmsg( "utf8_width differ between original string and obscured string" );
    }
    return result;
}

std::string serialize_wrapper( const std::function<void( JsonOut & )> &callback )
{
    std::ostringstream buffer;
    JsonOut jsout( buffer );
    callback( jsout );
    return buffer.str();
}

void deserialize_wrapper( const std::function<void( JsonIn & )> &callback, const std::string &data )
{
    std::istringstream buffer( data );
    JsonIn jsin( buffer );
    callback( jsin );
}

/* compare against table of easter dates */
static bool is_easter( int day, int month, int year )
{
    if( month == 3 ) {
        switch( year ) {
            // *INDENT-OFF*
            case 2024: return day == 31;
            case 2027: return day == 28;
            default: break;
            // *INDENT-ON*
        }
    } else if( month == 4 ) {
        switch( year ) {
            // *INDENT-OFF*
            case 2021: return day == 4;
            case 2022: return day == 17;
            case 2023: return day == 9;
            case 2025: return day == 20;
            case 2026: return day == 5;
            case 2028: return day == 16;
            case 2029: return day == 1;
            case 2030: return day == 21;
            default: break;
            // *INDENT-ON*
        }
    }
    return false;
}

holiday get_holiday_from_time( std::time_t time, bool force_refresh )
{
    static holiday cached_holiday = holiday::none;
    static bool is_cached = false;

    if( force_refresh ) {
        is_cached = false;
    }
    if( is_cached ) {
        return cached_holiday;
    }

    is_cached = true;

    bool success = false;

    std::tm local_time;
    std::time_t current_time = time == 0 ? std::time( nullptr ) : time;

    /* necessary to pass LGTM, as threadsafe version of localtime differs by platform */
#if defined(_WIN32)

    errno_t err = localtime_s( &local_time, &current_time );
    if( err == 0 ) {
        success = true;
    }

#else

    success = !!localtime_r( &current_time, &local_time );

#endif

    if( success ) {

        const int month = local_time.tm_mon + 1;
        const int day = local_time.tm_mday;
        const int wday = local_time.tm_wday;
        const int year = local_time.tm_year + 1900;

        /* check date against holidays */
        if( month == 1 && day == 1 ) {
            cached_holiday = holiday::new_year;
            return cached_holiday;
        }
        // only run easter date calculation if currently March or April
        else if( ( month == 3 || month == 4 ) && is_easter( day, month, year ) ) {
            cached_holiday = holiday::easter;
            return cached_holiday;
        } else if( month == 7 && day == 4 ) {
            cached_holiday = holiday::independence_day;
            return cached_holiday;
        }
        // 13 days seems appropriate for Halloween
        else if( month == 10 && day >= 19 ) {
            cached_holiday = holiday::halloween;
            return cached_holiday;
        } else if( month == 11 && ( day >= 22 && day <= 28 ) && wday == 4 ) {
            cached_holiday = holiday::thanksgiving;
            return cached_holiday;
        }
        // For the 12 days of Christmas, my true love gave to me...
        else if( month == 12 && ( day >= 14 && day <= 25 ) ) {
            cached_holiday = holiday::christmas;
            return cached_holiday;
        }
    }
    // fall through to here if localtime fails, or none of the day tests hit
    cached_holiday = holiday::none;
    return cached_holiday;
}
