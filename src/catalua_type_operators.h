#pragma once

/**
 * Due to some strange behavior in sol2, we need to define
 * comparison operators for types that are referenced by string_id or int_id.
 * For details, see https://github.com/ThePhD/sol2/issues/1264
 * string_id and int_id essentially act like smart pointers
 * and implement comparison operators, meaning T also has to implement them.
 *
 * You can use this macro as a quick solution, just pass the type as T
 * and the member/method that contains/returns the id as id_getter.
 *
 * Example implementation:
 * ```cpp
 * class ter_t {
 *   ter_str_id id;
 *   ...
 *   LUA_TYPE_OPS( ter_t, id );
 *   ...
 * }
 * ```
 */
#define LUA_TYPE_OPS( T, id_getter )                    \
    inline bool operator==( const T &rhs ) const {      \
        return (id_getter) == rhs.id_getter;              \
    };                                                  \
    inline bool operator<( const T &rhs ) const {       \
        return (id_getter) < rhs.id_getter;               \
    }


