#pragma once

#include <concepts>
#include <string>

#include "memory_fast.h"

/**
 * Source location for error reporting.
 * Works for both JSON (file:offset) and Lua (file:line).
 */
struct data_source_location {
    shared_ptr_fast<std::string> path;
    int line = 0;      // For Lua - line number
    int offset = 0;    // For JSON - byte offset
    bool is_lua = false;

    std::string to_string() const;
};

/** Throw error at given data source location. */
[[noreturn]]
void throw_error_at_data_loc( const data_source_location &loc, const std::string &message );

/** Show warning at given data source location. */
void show_warning_at_data_loc( const data_source_location &loc, const std::string &message );

// Forward declarations
class JsonObject;
class JsonArray;
class LuaTableWrapper;
class LuaArrayWrapper;

/**
 * Concept defining the interface that both JsonObject and LuaTableWrapper must satisfy.
 * This enables writing template functions that work with both data sources.
 *
 * The concept requires:
 * - Member existence checking (has_member, has_bool, has_int, etc.)
 * - Value reading with defaults (get_bool, get_int, get_float, get_string)
 * - Error handling (throw_error)
 * - Generic read() template method
 *
 * This allows the mandatory()/optional() helpers in generic_factory.h to work
 * with either JSON or Lua data sources without code duplication.
 */
template<typename T>
concept DataReader = requires( const T &reader, const std::string &name ) {
    // Member existence checking
    { reader.has_member( name ) } -> std::convertible_to<bool>;
    { reader.has_bool( name ) } -> std::convertible_to<bool>;
    { reader.has_int( name ) } -> std::convertible_to<bool>;
    { reader.has_float( name ) } -> std::convertible_to<bool>;
    { reader.has_number( name ) } -> std::convertible_to<bool>;
    { reader.has_string( name ) } -> std::convertible_to<bool>;
    { reader.has_array( name ) } -> std::convertible_to<bool>;
    { reader.has_object( name ) } -> std::convertible_to<bool>;

    // Value reading with defaults
    { reader.get_bool( name, true ) } -> std::convertible_to<bool>;
    { reader.get_int( name, 0 ) } -> std::convertible_to<int>;
    { reader.get_float( name, 0.0 ) } -> std::convertible_to<double>;
    { reader.get_string( name, std::string{} ) } -> std::convertible_to<std::string>;

    // Error handling
    { reader.throw_error( std::string{} ) };
    { reader.throw_error( std::string{}, name ) };

    // Allow omitted members (for extend/delete subtables)
    { reader.allow_omitted_members() };
};

/**
 * Concept for array readers (JsonArray and LuaArrayWrapper).
 */
template<typename T>
concept DataArray = requires( const T &arr ) {
    { arr.size() } -> std::convertible_to<size_t>;
    { arr.empty() } -> std::convertible_to<bool>;
    { arr.has_more() } -> std::convertible_to<bool>;
};

/**
 * Helper to check if a type is a DataReader at compile time.
 */
template<typename T>
struct is_data_reader : std::bool_constant<DataReader<T>> {};

template<typename T>
inline constexpr bool is_data_reader_v = is_data_reader<T>::value;
