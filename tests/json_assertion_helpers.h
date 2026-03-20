#pragma once

#ifndef CATA_TESTS_JSON_ASSERTION_HELPERS_H
#define CATA_TESTS_JSON_ASSERTION_HELPERS_H

#include "catch/catch.hpp"

#include <cstdlib>
#include <cmath>
#include <filesystem>
#include <map>
#include <limits>
#include <ranges>
#include <stdexcept>
#include <sstream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "fstream_utils.h"
#include "json.h"

namespace json_snapshot
{

struct compare_options {
    bool ignore_deep_object_order = true;
    bool pretty_print = true;
};

struct value {
    using array_t = std::vector<value>;
    using object_t = std::map<std::string, value>;
    using storage_t = std::variant<std::nullptr_t, bool, double, std::string, array_t, object_t>;

    storage_t data = nullptr;

    auto operator==( const value & ) const -> bool = default;
};

inline auto read_value( JsonIn &jsin ) -> value;
inline auto read_value( const JsonObject &jo, const std::string &name ) -> value;
inline auto read_value( const JsonArray &ja, size_t index ) -> value;

inline auto read_object( const JsonObject &jo ) -> value::object_t
{
    auto result = value::object_t {};
    for( const JsonMember &member : jo ) {
        if( member.is_comment() ) {
            continue;
        }
        result.emplace( member.name(), read_value( jo, member.name() ) );
    }
    return result;
}

inline auto read_array( const JsonArray &ja ) -> value::array_t
{
    auto result = value::array_t {};
    result.reserve( ja.size() );
    std::ranges::for_each( std::views::iota( size_t { 0 }, ja.size() ), [&]( const size_t index ) {
        result.push_back( read_value( ja, index ) );
    } );
    return result;
}

inline auto read_value( const JsonObject &jo, const std::string &name ) -> value
{
    if( jo.has_object( name ) ) {
        return value{ .data = read_object( jo.get_object( name ) ) };
    }
    if( jo.has_array( name ) ) {
        return value{ .data = read_array( jo.get_array( name ) ) };
    }
    if( jo.has_string( name ) ) {
        return value{ .data = jo.get_string( name ) };
    }
    if( jo.has_bool( name ) ) {
        return value{ .data = jo.get_bool( name ) };
    }
    if( jo.has_null( name ) ) {
        return value{};
    }
    return value{ .data = jo.get_float( name ) };
}

inline auto read_value( const JsonArray &ja, const size_t index ) -> value
{
    if( ja.has_object( index ) ) {
        return value{ .data = read_object( ja.get_object( index ) ) };
    }
    if( ja.has_array( index ) ) {
        return value{ .data = read_array( ja.get_array( index ) ) };
    }
    if( ja.has_string( index ) ) {
        return value{ .data = ja.get_string( index ) };
    }
    if( ja.has_bool( index ) ) {
        return value{ .data = ja.get_bool( index ) };
    }
    if( ja.has_null( index ) ) {
        return value{};
    }
    return value{ .data = ja.get_float( index ) };
}

inline auto read_value( JsonIn &jsin ) -> value
{
    if( jsin.test_object() ) {
        return value{ .data = read_object( jsin.get_object() ) };
    }
    if( jsin.test_array() ) {
        return value{ .data = read_array( jsin.get_array() ) };
    }
    if( jsin.test_string() ) {
        return value{ .data = jsin.get_string() };
    }
    if( jsin.test_bool() ) {
        return value{ .data = jsin.get_bool() };
    }
    if( jsin.test_null() ) {
        jsin.skip_null();
        return value{};
    }
    return value{ .data = jsin.get_float() };
}

inline auto write_value( JsonOut &jsout, const value &json_value ) -> void
{
    if( const auto *null_value = std::get_if<std::nullptr_t>( &json_value.data ) ) {
        ( void )null_value;
        jsout.write_null();
        return;
    }
    if( const auto *bool_value = std::get_if<bool>( &json_value.data ) ) {
        jsout.write( *bool_value );
        return;
    }
    if( const auto *number_value = std::get_if<double>( &json_value.data ) ) {
        if( std::trunc( *number_value ) == *number_value &&
            *number_value >= static_cast<double>( std::numeric_limits<int64_t>::min() ) &&
            *number_value <= static_cast<double>( std::numeric_limits<int64_t>::max() ) ) {
            jsout.write( static_cast<int64_t>( *number_value ) );
        } else {
            jsout.write( *number_value );
        }
        return;
    }
    if( const auto *string_value = std::get_if<std::string>( &json_value.data ) ) {
        jsout.write( *string_value );
        return;
    }
    if( const auto *array_value = std::get_if<value::array_t>( &json_value.data ) ) {
        jsout.start_array();
        std::ranges::for_each( *array_value, [&]( const value & entry ) {
            write_value( jsout, entry );
        } );
        jsout.end_array();
        return;
    }

    jsout.start_object();
    const auto &object_value = std::get<value::object_t>( json_value.data );
    std::ranges::for_each( object_value, [&]( const std::pair<const std::string, value> &entry ) {
        jsout.member( entry.first );
        write_value( jsout, entry.second );
    } );
    jsout.end_object();
}

inline auto canonicalize_json( const std::string &json,
                               const compare_options &opts ) -> std::string
{
    if( !opts.ignore_deep_object_order ) {
        return json;
    }

    auto input = std::istringstream( json );
    auto jsin = JsonIn( input );
    const auto parsed = read_value( jsin );

    auto output = std::ostringstream {};
    auto jsout = JsonOut( output, opts.pretty_print );
    write_value( jsout, parsed );
    return output.str();
}

inline auto read_text_file( const std::string &path ) -> std::string
{
    auto result = std::string {};
    const auto ok = read_from_file( path, [&]( std::istream & fin ) {
        result.assign( std::istreambuf_iterator<char>( fin ), std::istreambuf_iterator<char>() );
    } );
    if( !ok ) {
        throw std::runtime_error( "Failed to read snapshot file: " + path );
    }
    return result;
}

inline auto update_snapshots() -> bool
{
    const auto *env = std::getenv( "CATA_UPDATE_JSON_SNAPSHOTS" );
    return env != nullptr && *env != '\0' && std::string_view( env ) != "0";
}

inline auto write_snapshot_file( const std::string &path,
                                 const std::string &contents ) -> void
{
    const auto dir = std::filesystem::path( path ).parent_path();
    if( !dir.empty() ) {
        std::filesystem::create_directories( dir );
    }
    auto writer = [&contents]( std::ostream & fout ) {
        fout << contents;
    };
    if( !write_to_file( path, writer ) ) {
        throw std::runtime_error( "Failed to write snapshot file: " + path );
    }
}

class equals_json_matcher : public Catch::MatcherBase<std::string>
{
    public:
        equals_json_matcher( std::string expected_json, compare_options options ) :
            expected_json_( std::move( expected_json ) ), options_( options ) {}

        auto match( const std::string &actual_json ) const -> bool override {
            actual_normalized_ = canonicalize_json( actual_json, options_ );
            expected_normalized_ = canonicalize_json( expected_json_, options_ );
            return actual_normalized_ == expected_normalized_;
        }

        auto describe() const -> std::string override {
            return "JSON matches expected snapshot";
        }

        auto actual_normalized() const -> const std::string & { // *NOPAD*
            return actual_normalized_;
        }

        auto expected_normalized() const -> const std::string & { // *NOPAD*
            return expected_normalized_;
        }

    private:
        std::string expected_json_;
        compare_options options_;
        mutable std::string actual_normalized_;
        mutable std::string expected_normalized_;
};

inline auto json_equals( std::string expected_json,
const compare_options &opts = compare_options {} ) -> equals_json_matcher {
    return equals_json_matcher( std::move( expected_json ), opts );
}

inline auto check_json_snapshot( const std::string &actual_json,
                                 const std::string &snapshot_path,
const compare_options &opts = compare_options {} ) -> void {
    const auto actual_normalized = canonicalize_json( actual_json, opts );
    if( update_snapshots() )
    {
        write_snapshot_file( snapshot_path, actual_normalized );
    }

    const auto expected_json = read_text_file( snapshot_path );
    const auto expected_normalized = canonicalize_json( expected_json, opts );
    CAPTURE( snapshot_path );
    CAPTURE( actual_normalized );
    CAPTURE( expected_normalized );
    CHECK_THAT( actual_json, json_equals( expected_json, opts ) );
}

} // namespace json_snapshot

#endif // CATA_TESTS_JSON_ASSERTION_HELPERS_H
