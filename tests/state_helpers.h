#pragma once
#ifndef CATA_TESTS_STATE_HELPERS_H
#define CATA_TESTS_STATE_HELPERS_H

#include "enum_bitset.h"

enum class test_state : int {
    avatar,
    creature,
    npc,
    vehicle,
    map,
    time,
    name,
    arena,
    weather,
    num,
};

using state = test_state;

template<>
struct enum_traits<test_state> {
    static constexpr test_state last = test_state::num;
};

inline auto operator|( const test_state lhs, const test_state rhs ) -> enum_bitset<test_state>
{
    return enum_bitset<test_state>().set( lhs ).set( rhs );
}

inline auto operator|( enum_bitset<test_state> lhs,
                       const test_state rhs ) -> enum_bitset<test_state>
{
    lhs.set( rhs );
    return lhs;
}

auto clear_states( const enum_bitset<test_state> &states ) -> void;
auto clear_all_state() -> void;

#endif // CATA_TESTS_STATE_HELPERS_H
