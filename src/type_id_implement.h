#pragma once

/**
 * Macros with boilerplate code for implementing string_id<T> and int_id<T>
 * that work together with single generic_factory<T>.
 */

// Implement string_id<T> operations
#define IMPLEMENT_STRING_ID( T, factory )                       \
    /** @relates string_id */                                   \
    template<>                                                  \
    bool string_id<T>::is_valid() const                         \
    {                                                           \
        return (factory).is_valid(*this);                         \
    }                                                           \
    /** @relates string_id */                                   \
    template<>                                                  \
    const T& string_id<T>::obj() const                          \
    {                                                           \
        return (factory).obj(*this);                              \
    }

// Implement int_id<T> operations
#define IMPLEMENT_INT_ID( T, factory )                          \
    template<>                                                  \
    bool int_id<T>::is_valid() const                            \
    {                                                           \
        return (factory).is_valid(*this);                         \
    }                                                           \
    template<>                                                  \
    const T& int_id<T>::obj() const                             \
    {                                                           \
        return (factory).obj(*this);                              \
    }

// Implement string_id<T> <-> int_id<T> conversions
#define IMPLEMENT_INT_STRING_ID_CONVERSIONS( T, factory )       \
    /** @relates string_id */                                   \
    template<>                                                  \
    int_id<T> string_id<T>::id() const                          \
    {                                                           \
        return (factory).convert(*this, int_id<T>(-1));           \
    }                                                           \
    template<>                                                  \
    int_id<T>::int_id(const string_id<T>& id)                   \
    {                                                           \
        *this = id.id();                                        \
    }                                                           \
    template<>                                                  \
    const string_id<T>& int_id<T>::id() const                   \
    {                                                           \
        return (factory).convert(*this);                          \
    }

// Implement string_id<T> and int_id<T> operations, as well as conversions between them
#define IMPLEMENT_STRING_AND_INT_IDS( T, factory )              \
    IMPLEMENT_STRING_ID( T, factory )                           \
    IMPLEMENT_INT_ID( T, factory )                              \
    IMPLEMENT_INT_STRING_ID_CONVERSIONS( T, factory )           \



