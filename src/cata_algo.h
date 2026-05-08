#pragma once

#include <algorithm>
#include <functional>
#include <map>
#include <cassert>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <ranges>

namespace cata
{

/**
 * @brief Sort elements of a sequence by their rating (the smaller the better).
 *
 * @param  begin           An input/output iterator.
 * @param  end             An input/output iterator.
 * @param  rating_func     A functor which calculates ratings (the outcome
 *                         must be comparable using '<' operator).
 *
 * The rating is calculated only once per each element.
 */
template<typename Iterator, typename RatingFunction>
void sort_by_rating( Iterator begin, Iterator end, RatingFunction rating_func )
{
    using value_t = typename Iterator::value_type;
    using pair_t = std::pair<value_t, decltype( rating_func( *begin ) )>;

    std::vector<pair_t> rated_entries;
    rated_entries.reserve( std::distance( begin, end ) );

    std::transform( begin, end, std::back_inserter( rated_entries ),
    [ &rating_func ]( const value_t &elem ) {
        return std::make_pair( elem, rating_func( elem ) );
    } );

    std::sort( rated_entries.begin(), rated_entries.end(), []( const pair_t &lhs, const pair_t &rhs ) {
        return lhs.second < rhs.second;
    } );

    std::transform( rated_entries.begin(), rated_entries.end(), begin, []( const pair_t &elem ) {
        return elem.first;
    } );
}

// Implementation detail of below find_cycles
// This explores one branch of the given graph depth-first
template<typename T>
void find_cycles_impl(
    const std::unordered_map<T, std::vector<T>> &edges,
    const T &v,
    std::unordered_set<T> &visited,
    std::unordered_map<T, T> &on_current_branch,
    std::vector<std::vector<T>> &result )
{
    bool new_vertex = visited.insert( v ).second;

    if( !new_vertex ) {
        return;
    }
    auto it = edges.find( v );
    if( it == edges.end() ) {
        return;
    }

    for( const T &next_v : it->second ) {
        if( next_v == v ) {
            // Trivial self-loop
            result.push_back( { v } );
            continue;
        }
        auto previous_match = on_current_branch.find( next_v );
        if( previous_match != on_current_branch.end() ) {
            // We have looped back to somewhere along the branch we took to
            // reach this vertex, so reconstruct the loop and save it.
            std::vector<T> loop;
            T on_path = v;
            while( true ) {
                loop.push_back( on_path );
                if( on_path == next_v ) {
                    break;
                }
                on_path = on_current_branch[on_path];
            }
            std::reverse( loop.begin(), loop.end() );
            result.push_back( loop );
        } else {
            on_current_branch.emplace( next_v, v );
            find_cycles_impl( edges, next_v, visited, on_current_branch, result );
            on_current_branch.erase( next_v );
        }
    }
}

// Find and return a list of all cycles in a directed graph.
// Each T defines a vertex.
// For a vertex a, edges[a] is a list of all the vertices connected by edges
// from a.
// It is acceptable for some vertex keys to be missing from the edges map, if
// those vertices have no out-edges.
// Complexity should be O(V+E)
// Based on https://www.geeksforgeeks.org/detect-cycle-in-a-graph/
template<typename T>
std::vector<std::vector<T>> find_cycles( const std::unordered_map<T, std::vector<T>> &edges )
{
    std::unordered_set<T> visited;
    std::unordered_map<T, T> on_current_branch;
    std::vector<std::vector<T>> result;

    for( const auto &p : edges ) {
        const T &root = p.first;

        on_current_branch.emplace( root, root );
        find_cycles_impl( edges, root, visited, on_current_branch, result );
        on_current_branch.erase( root );
        assert( on_current_branch.empty() );
    }

    return result;
}

/// @brief Group elements of a container into key-value map by a given selector function.
///
/// @tparam M map type to use for the result. defaults to std::map
/// @tparam C container type, usually inferred
/// @param selector mapped to each container. otuput type is used as a key for the result map
///
/// @return A map of key-value pairs, where the the value is a container of elements grouped by the key.
///
/// @example
/// ```cpp
/// group_by( {1, 2, 3, 4, 5}, []( int i ) -> std::string { return i % 2 == 0 ? "even" : "odd" } )
/// //=> Map{ "even": {2, 4}, "odd": {1, 3, 5} }
/// ```
///
/// @remark poor person's https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/group-by.html
template<template<typename...> typename M = std::map, typename C, typename F>
auto group_by( const C &c, F && selector )
{
    using K = std::invoke_result_t<F, std::decay_t<decltype( *c.begin() )>>;

    auto result = M<K, C> {};
    for( const auto &elem : c ) {
        result[selector( elem )].emplace_back( elem );
    }
    return result;
}

namespace ranges
{

/// @remark poor person's flatmap
inline constexpr auto flat_map = []( auto &&func )
{
    return std::views::transform( std::forward<decltype( func )>( func ) ) | std::views::join;
};

/// allows any invocable that accepts a range to be used in ranges pipeline
/// doesn't require @tparam Self to be a range adaptor closure
template <typename Self, std::ranges::input_range R> requires std::invocable<Self, R>
auto operator|( R &&r, const Self &&adaptor )
{
    return adaptor( std::forward<R>( r ) );
}

template <typename Proj>
struct MaxAdaptor {
    Proj proj;
    explicit MaxAdaptor( Proj s ) : proj( std::move( s ) ) {}

    template <std::ranges::input_range R>
    requires std::copy_constructible<std::ranges::range_value_t<R>>
    auto operator()( R &&r ) const -> std::optional<std::ranges::range_value_t<R>> {
        auto current_it = std::ranges::begin( r );
        const auto last_it = std::ranges::end( r );

        if( current_it == last_it ) {
            return std::nullopt;
        }

        std::ranges::range_value_t<R> max_val = *current_it;
        auto max_proj = proj( max_val );
        ++current_it;

        while( current_it != last_it ) {
            auto current_proj = proj( *current_it );
            if( max_proj < current_proj ) {
                max_val = *current_it;
                max_proj = current_proj;
            }
            ++current_it;
        }
        return max_val;
    }
};

template <typename Proj>
struct MinAdaptor {
    Proj proj;
    explicit MinAdaptor( Proj s ) : proj( std::move( s ) ) {}

    template <std::ranges::input_range R>
    requires std::copy_constructible<std::ranges::range_value_t<R>>
    auto operator()( R &&r ) const -> std::optional<std::ranges::range_value_t<R>> {
        auto current_it = std::ranges::begin( r );
        const auto last_it = std::ranges::end( r );

        if( current_it == last_it ) {
            return std::nullopt;
        }

        std::ranges::range_value_t<R> min_val = *current_it;
        auto min_proj = proj( min_val );
        ++current_it;

        while( current_it != last_it ) {
            auto current_proj = proj( *current_it );
            if( current_proj < min_proj ) {
                min_val = *current_it;
                min_proj = current_proj;
            }
            ++current_it;
        }
        return min_val;
    }
};

template <typename T, typename BinOp>
struct FoldLeftAdaptor {
    T init;
    BinOp op;

    explicit FoldLeftAdaptor( T i, BinOp o ) : init( std::move( i ) ), op( std::move( o ) ) {}

    template <std::ranges::input_range R>
    auto operator()( R &&r ) const {
        auto accum = init;
        for( auto &&elem : r ) {
            accum = op( std::move( accum ), std::forward<decltype( elem )>( elem ) );
        }
        return accum;
    }
};

template <typename Proj>
inline auto max_by( Proj &&proj )
{
    return MaxAdaptor<std::decay_t<Proj>>( std::forward<Proj>( proj ) );
}
inline auto max()
{
    return MaxAdaptor( std::identity{} );
}

template <typename Proj>
inline auto min_by( Proj &&proj )
{
    return MinAdaptor<std::decay_t<Proj>>( std::forward<Proj>( proj ) );
}
inline auto min()
{
    return MinAdaptor( std::identity{} );
}

/// std::ranges::fold_left but pipeable
template <typename T, typename BinOp>
inline auto fold_left( T &&init, BinOp &&op )
{
    return FoldLeftAdaptor<std::decay_t<T>, std::decay_t<BinOp>>(
               std::forward<T>( init ), std::forward<BinOp>( op ) );
}

} // namespace ranges

} // namespace cata
