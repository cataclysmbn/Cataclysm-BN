#pragma once

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "catalua_sol.h"
#include "data_reader.h"
#include "translations.h"

class JsonOut;
class LuaArrayWrapper;

/**
 * Wrapper around sol::table that implements the DataReader interface,
 * mirroring JsonObject's API as closely as possible.
 *
 * This enables existing data loading code to work with Lua tables
 * without modification, using the mandatory()/optional() helpers
 * from generic_factory.h.
 *
 * Key features:
 * - Tracks source location (file:line) for error messages via Lua debug info
 * - Tracks visited members for debugging (like JsonObject's unvisited member warnings)
 * - Supports JSON-compatible translation format: {str="text"} or {str="sing", str_pl="plur"}
 * - Can serialize back to JSON for data dumping
 */
class LuaTableWrapper
{
    private:
        sol::table table_;
        data_source_location source_loc_;
        mutable std::set<std::string> visited_members_;
        mutable bool report_unvisited_ = true;

        void mark_visited( const std::string &name ) const;
        void capture_source_location();

    public:
        LuaTableWrapper();
        explicit LuaTableWrapper( const sol::table &tbl );
        LuaTableWrapper( const sol::table &tbl, const data_source_location &loc );
        ~LuaTableWrapper();

        // Disable copy to match JsonObject semantics
        LuaTableWrapper( const LuaTableWrapper & ) = delete;
        LuaTableWrapper &operator=( const LuaTableWrapper & ) = delete;
        LuaTableWrapper( LuaTableWrapper && ) = default;
        LuaTableWrapper &operator=( LuaTableWrapper && ) = default;

        // Source location for error messages
        data_source_location get_source_location() const {
            return source_loc_;
        }
        std::string line_number() const;

        // Allow skipping unvisited member checks (for extend/delete subtables)
        void allow_omitted_members() const {
            report_unvisited_ = false;
        }

        // Check if the wrapper is valid (has a table)
        bool is_valid() const;
        explicit operator bool() const {
            return is_valid();
        }

        // Member existence checking - mirrors JsonObject interface
        bool has_member( const std::string &name ) const;
        bool has_null( const std::string &name ) const;
        bool has_bool( const std::string &name ) const;
        bool has_number( const std::string &name ) const;
        bool has_int( const std::string &name ) const {
            return has_number( name );
        }
        bool has_float( const std::string &name ) const {
            return has_number( name );
        }
        bool has_string( const std::string &name ) const;
        bool has_array( const std::string &name ) const;
        bool has_object( const std::string &name ) const;

        // Value reading (throwing version - for mandatory fields)
        bool get_bool( const std::string &name ) const;
        int get_int( const std::string &name ) const;
        double get_float( const std::string &name ) const;
        std::string get_string( const std::string &name ) const;
        LuaArrayWrapper get_array( const std::string &name ) const;
        LuaTableWrapper get_object( const std::string &name ) const;

        // Value reading (with defaults - for optional fields)
        bool get_bool( const std::string &name, bool fallback ) const;
        int get_int( const std::string &name, int fallback ) const;
        double get_float( const std::string &name, double fallback ) const;
        std::string get_string( const std::string &name, const std::string &fallback ) const;

        // Array helpers - mirrors JsonObject interface
        std::vector<int> get_int_array( const std::string &name ) const;
        std::vector<std::string> get_string_array( const std::string &name ) const;

        // Tags helper (for flags, etc.) - mirrors JsonObject interface
        template<typename T = std::string, typename Res = std::set<T>>
        Res get_tags( const std::string &name ) const;

        // Templated read() that mirrors JsonObject::read()
        // Returns true if value was read, false if member not found
        // throw_on_error controls behavior when member exists but is wrong type
        template<typename T>
        bool read( const std::string &name, T &value, bool throw_on_error = true ) const;

        // Error handling - mirrors JsonObject interface
        [[noreturn]] void throw_error( const std::string &err ) const;
        [[noreturn]] void throw_error( const std::string &err, const std::string &name ) const;
        void show_warning( const std::string &err ) const;
        void show_warning( const std::string &err, const std::string &name ) const;

        // Access to underlying table (for advanced use)
        const sol::table &raw_table() const;

        // JSON serialization for data dumper
        std::string to_json_string() const;
        void write_json( JsonOut &jsout ) const;
};

/**
 * Array wrapper that mirrors JsonArray's interface.
 *
 * Supports both iterative access (next_*) and random access (get_*).
 * Note: Lua arrays are 1-indexed internally, but this wrapper uses 0-indexed
 * access to match JsonArray.
 */
class LuaArrayWrapper
{
    private:
        sol::table table_;
        mutable size_t index_ = 0;
        size_t size_ = 0;
        data_source_location source_loc_;

    public:
        LuaArrayWrapper();
        explicit LuaArrayWrapper( const sol::table &tbl );
        LuaArrayWrapper( const sol::table &tbl, const data_source_location &loc );

        // Allow copy (like JsonArray)
        LuaArrayWrapper( const LuaArrayWrapper & ) = default;
        LuaArrayWrapper &operator=( const LuaArrayWrapper & ) = default;
        LuaArrayWrapper( LuaArrayWrapper && ) = default;
        LuaArrayWrapper &operator=( LuaArrayWrapper && ) = default;

        size_t size() const {
            return size_;
        }
        bool empty() const {
            return size_ == 0;
        }
        bool has_more() const {
            return index_ < size_;
        }

        // Iterative access (advances internal index) - mirrors JsonArray
        bool next_bool();
        int next_int();
        double next_float();
        std::string next_string();
        LuaArrayWrapper next_array();
        LuaTableWrapper next_object();
        void skip_value();

        // Random access by index (0-based) - mirrors JsonArray
        bool get_bool( size_t index ) const;
        int get_int( size_t index ) const;
        double get_float( size_t index ) const;
        std::string get_string( size_t index ) const;
        LuaArrayWrapper get_array( size_t index ) const;
        LuaTableWrapper get_object( size_t index ) const;

        // Type checking for current element
        bool test_bool() const;
        bool test_int() const;
        bool test_float() const;
        bool test_string() const;
        bool test_array() const;
        bool test_object() const;

        // Templated read for current element
        template<typename T>
        bool read_next( T &value );

        // Error handling
        [[noreturn]] void throw_error( const std::string &err ) const;
        [[noreturn]] void throw_error( const std::string &err, size_t index ) const;

        // Range-based for loop support (iterates as LuaTableWrapper for objects)
        class const_iterator;
        const_iterator begin() const;
        const_iterator end() const;
};

/**
 * Iterator for LuaArrayWrapper to support range-based for loops.
 * Each element is returned as a sol::object for flexibility.
 */
class LuaArrayWrapper::const_iterator
{
    private:
        const LuaArrayWrapper *parent_;
        size_t index_;

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = sol::object;
        using difference_type = std::ptrdiff_t;
        using pointer = const sol::object *;
        using reference = sol::object;

        const_iterator( const LuaArrayWrapper *parent, size_t index );

        reference operator*() const;
        const_iterator &operator++();
        const_iterator operator++( int );
        bool operator==( const const_iterator &other ) const;
        bool operator!=( const const_iterator &other ) const;
};

// Static assert to verify LuaTableWrapper satisfies DataReader concept
// (actual check happens in lua_table_wrapper.cpp to avoid header dependency issues)

/**
 * Utility function to write a sol::object as JSON.
 * Used by data dumper to serialize Lua values.
 */
void write_lua_value_as_json( JsonOut &jsout, const sol::object &val );
