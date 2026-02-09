#pragma once

#include <map>
#include <optional>
#include <set>
#include <string>
#include <type_traits>
#include <vector>

#include "calendar.h"
#include "catalua_sol.h"
#include "data_reader.h"
#include "translations.h"
#include "units.h"

// Forward declarations for type detection
template<typename T> class string_id;
template<typename T> class int_id;

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

        // Non-template overloads for basic types (defined in lua_table_wrapper.cpp)
        // These are preferred over the template for exact type matches
        bool read( const std::string &name, bool &value, bool throw_on_error = true ) const;
        bool read( const std::string &name, int &value, bool throw_on_error = true ) const;
        bool read( const std::string &name, double &value, bool throw_on_error = true ) const;
        bool read( const std::string &name, float &value, bool throw_on_error = true ) const;
        bool read( const std::string &name, std::string &value, bool throw_on_error = true ) const;
        bool read( const std::string &name, translation &value, bool throw_on_error = true ) const;
        bool read( const std::string &name, std::vector<std::string> &value,
                   bool throw_on_error = true ) const;
        bool read( const std::string &name, std::vector<translation> &value,
                   bool throw_on_error = true ) const;
        bool read( const std::string &name, time_duration &value,
                   bool throw_on_error = true ) const;
        bool read( const std::string &name, units::energy &value,
                   bool throw_on_error = true ) const;
        bool read( const std::string &name, units::mass &value,
                   bool throw_on_error = true ) const;

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

// ============================================================================
// Template implementations for LuaTableWrapper::read()
// These must be in the header so they can be instantiated in other translation units.
// Explicit specializations for basic types (bool, int, double, float, std::string,
// time_duration, units::energy, units::mass, translation) are in lua_table_wrapper.cpp.
// ============================================================================

// Type trait to detect if T has a .str() method (like string_id types)
template<typename T, typename = void>
struct has_str_method : std::false_type {};

template<typename T>
struct has_str_method<T, std::void_t<decltype( std::declval<T>().str() )>> : std::true_type {};

template<typename T>
inline constexpr bool has_str_method_v = has_str_method<T>::value;

// Type trait to detect string_id<X> types
template<typename T>
struct is_string_id : std::false_type {};

template<typename T>
struct is_string_id<string_id<T>> : std::true_type {};

template<typename T>
inline constexpr bool is_string_id_v = is_string_id<T>::value;

// Type trait to detect int_id<X> types
template<typename T>
struct is_int_id : std::false_type {};

template<typename T>
struct is_int_id<int_id<T>> : std::true_type {};

template<typename T>
inline constexpr bool is_int_id_v = is_int_id<T>::value;

// Type trait to detect std::optional<X> types
// Note: Named lua_is_optional to avoid conflict with is_optional in assign.h
template<typename T>
struct lua_is_optional : std::false_type {};

template<typename T>
struct lua_is_optional<std::optional<T>> : std::true_type {};

template<typename T>
inline constexpr bool lua_is_optional_v = lua_is_optional<T>::value;

// Type trait to extract inner type from std::optional
template<typename T>
struct optional_inner_type {};

template<typename T>
struct optional_inner_type<std::optional<T>> {
    using type = T;
};

template<typename T>
using optional_inner_type_t = typename optional_inner_type<T>::type;

// Type trait to detect std::vector<X> types
template<typename T>
struct is_std_vector : std::false_type {};

template<typename T, typename A>
struct is_std_vector<std::vector<T, A>> : std::true_type {};

template<typename T>
inline constexpr bool is_std_vector_v = is_std_vector<T>::value;

// Type trait to detect std::set<X> types
template<typename T>
struct is_std_set : std::false_type {};

template<typename T, typename C, typename A>
struct is_std_set<std::set<T, C, A>> : std::true_type {};

template<typename T>
inline constexpr bool is_std_set_v = is_std_set<T>::value;

// Note: LuaTableWrapper has non-template overloads of read() for basic types
// (bool, int, double, float, std::string, time_duration, units::energy, units::mass,
// translation, std::vector<std::string>, std::vector<translation>) defined in
// lua_table_wrapper.cpp. C++ overload resolution will prefer these over the template
// when the types match exactly.

// General template implementation for LuaTableWrapper::read()
// This handles types not covered by explicit specializations.
template<typename T>
bool LuaTableWrapper::read( const std::string &name, T &value, bool throw_on_error ) const
{
    if( !has_member( name ) ) {
        return false;
    }
    mark_visited( name );
    sol::object obj = table_[name];

    // Handle string_id types (e.g., trait_id, bionic_id) - construct from string
    if constexpr( is_string_id_v<T> || ( has_str_method_v<T> && !std::is_same_v<T, std::string> ) ) {
        if( obj.is<std::string>() ) {
            value = T( obj.as<std::string>() );
            return true;
        }
        if( throw_on_error ) {
            throw_error( "expected string for string_id type", name );
        }
        return false;
    }
    // Handle int_id types - construct from string
    else if constexpr( is_int_id_v<T> ) {
        if( obj.is<std::string>() ) {
            value = T( obj.as<std::string>() );
            return true;
        }
        if( throw_on_error ) {
            throw_error( "expected string for int_id type", name );
        }
        return false;
    }
    // Handle std::optional<X> types
    else if constexpr( lua_is_optional_v<T> ) {
        using InnerType = optional_inner_type_t<T>;
        // If the value is nil, return nullopt
        if( obj.is<sol::lua_nil_t>() ) {
            value = std::nullopt;
            return true;
        }
        // Try to read the inner value using the same read logic
        // We already marked this as visited, so we can try to parse the value directly
        InnerType inner_value{};
        // For basic types, do direct conversion
        if constexpr( std::is_same_v<InnerType, int> ) {
            if( obj.is<int>() ) {
                value = obj.as<int>();
                return true;
            }
            if( obj.is<double>() ) {
                value = static_cast<int>( obj.as<double>() );
                return true;
            }
        } else if constexpr( std::is_same_v<InnerType, double> || std::is_same_v<InnerType, float> ) {
            if( obj.is<double>() || obj.is<int>() ) {
                value = static_cast<InnerType>( obj.is<double>() ? obj.as<double>() :
                                                static_cast<double>( obj.as<int>() ) );
                return true;
            }
        } else if constexpr( std::is_same_v<InnerType, std::string> ) {
            if( obj.is<std::string>() ) {
                value = obj.as<std::string>();
                return true;
            }
        } else if constexpr( std::is_same_v<InnerType, bool> ) {
            if( obj.is<bool>() ) {
                value = obj.as<bool>();
                return true;
            }
        } else if constexpr( is_string_id_v<InnerType> || has_str_method_v<InnerType> ) {
            if( obj.is<std::string>() ) {
                value = InnerType( obj.as<std::string>() );
                return true;
            }
        }
        // Fallback: try direct sol conversion
        if( obj.is<InnerType>() ) {
            value = obj.as<InnerType>();
            return true;
        }
        if( throw_on_error ) {
            throw_error( "failed to read optional value", name );
        }
        return false;
    }
    // Handle std::vector<X> types
    else if constexpr( is_std_vector_v<T> ) {
        using ValueType = typename T::value_type;
        if( obj.is<sol::table>() ) {
            sol::table tbl = obj.as<sol::table>();
            value.clear();
            for( auto &pair : tbl ) {
                // Try to read each element
                if constexpr( std::is_same_v<ValueType, std::string> ) {
                    if( pair.second.is<std::string>() ) {
                        value.push_back( pair.second.template as<std::string>() );
                    }
                } else if constexpr( std::is_same_v<ValueType, int> ) {
                    if( pair.second.is<int>() ) {
                        value.push_back( pair.second.template as<int>() );
                    } else if( pair.second.is<double>() ) {
                        value.push_back( static_cast<int>( pair.second.template as<double>() ) );
                    }
                } else if constexpr( is_string_id_v<ValueType> || has_str_method_v<ValueType> ) {
                    if( pair.second.is<std::string>() ) {
                        value.push_back( ValueType( pair.second.template as<std::string>() ) );
                    }
                } else if constexpr( std::is_same_v<ValueType, translation> ) {
                    if( pair.second.is<std::string>() ) {
                        value.push_back( translation::to_translation( pair.second.template as<std::string>() ) );
                    }
                }
                // Add more element types as needed
            }
            return true;
        }
        // Single value -> single-element vector
        if constexpr( is_string_id_v<ValueType> || has_str_method_v<ValueType> ) {
            if( obj.is<std::string>() ) {
                value.clear();
                value.push_back( ValueType( obj.as<std::string>() ) );
                return true;
            }
        }
        if( throw_on_error ) {
            throw_error( "expected array", name );
        }
        return false;
    }
    // Handle std::set<X> types
    else if constexpr( is_std_set_v<T> ) {
        using ValueType = typename T::value_type;
        if( obj.is<sol::table>() ) {
            sol::table tbl = obj.as<sol::table>();
            value.clear();
            for( auto &pair : tbl ) {
                if constexpr( std::is_same_v<ValueType, std::string> ) {
                    if( pair.second.is<std::string>() ) {
                        value.insert( pair.second.template as<std::string>() );
                    }
                } else if constexpr( is_string_id_v<ValueType> || has_str_method_v<ValueType> ) {
                    if( pair.second.is<std::string>() ) {
                        value.insert( ValueType( pair.second.template as<std::string>() ) );
                    }
                }
                // Add more element types as needed
            }
            return true;
        }
        // Single value -> single-element set
        if constexpr( is_string_id_v<ValueType> || has_str_method_v<ValueType> ) {
            if( obj.is<std::string>() ) {
                value.clear();
                value.insert( ValueType( obj.as<std::string>() ) );
                return true;
            }
        }
        if( throw_on_error ) {
            throw_error( "expected array for set", name );
        }
        return false;
    }
    // Handle enum types - read as string and convert
    else if constexpr( std::is_enum_v<T> ) {
        if( obj.is<std::string>() ) {
            // This requires io::string_to_enum to be available
            // For now, try direct conversion from int if it's an int
            if( throw_on_error ) {
                throw_error( "enum reading from Lua not yet fully supported", name );
            }
            return false;
        }
        if( obj.is<int>() ) {
            value = static_cast<T>( obj.as<int>() );
            return true;
        }
        if( throw_on_error ) {
            throw_error( "expected int or string for enum", name );
        }
        return false;
    }
    // Try direct sol conversion as last resort
    else {
        if( obj.is<T>() ) {
            value = obj.as<T>();
            return true;
        }
        if( throw_on_error ) {
            throw_error( "failed to read value (type mismatch)", name );
        }
        return false;
    }
}
