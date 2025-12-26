#pragma once

#include <cassert>
#include <cstddef>
#include <iterator>
#include <optional>
#include <ranges>
#include <utility>

#include "vpart_position.h"
#include "vehicle.h"

enum class part_status_flag : int;

/**
 * Exposes (multiple) parts of one vehicle as @ref vpart_reference.
 * Uses an arbitrary @ref range_type to supply
 * - the vehicle reference,
 * - information which parts should be iterated over (and which
 *   should be skipped).
 */
template<typename range_type>
class vehicle_part_iterator
{
    private:
        const range_type *range_ = nullptr;
        std::optional<vpart_reference> vp_;

        auto range() const -> const range_type & { return *range_; } // *NOPAD*

        auto skip_to_next_valid( size_t i ) -> void {
            while( i < range().part_count() &&
                   !range().matches( i ) ) {
                ++i;
            }
            if( i < range().part_count() ) {
                vp_.emplace( range().vehicle(), i );
            } else {
                vp_.reset();
            }
        }

    public:
        using value_type = vpart_reference;
        using difference_type = std::ptrdiff_t;
        using pointer = const vpart_reference *;
        using reference = const vpart_reference &;
        using iterator_category = std::forward_iterator_tag;
        using iterator_concept = std::forward_iterator_tag;

        vehicle_part_iterator() = default;

        vehicle_part_iterator( const range_type *r, size_t i ) : range_( r ) {
            assert( i <= range().part_count() );
            skip_to_next_valid( i );
        }

        vehicle_part_iterator( const vehicle_part_iterator & ) = default;
        vehicle_part_iterator &operator=( const vehicle_part_iterator & ) = default;

        auto operator*() const -> reference {
            assert( vp_ );
            return *vp_;
        }

        auto operator->() const -> pointer {
            assert( vp_ );
            return &*vp_;
        }

        auto operator++() -> vehicle_part_iterator & { // *NOPAD*
            assert( vp_ );
            skip_to_next_valid( vp_->part_index() + 1 );
            return *this;
        }

        auto operator++( int ) -> vehicle_part_iterator {
            auto tmp = *this;
            ++( *this );
            return tmp;
        }

        auto operator==( const vehicle_part_iterator &rhs ) const -> bool {
            if( vp_.has_value() != rhs.vp_.has_value() ) {
                return false;
            }
            if( !vp_.has_value() ) {
                return true;
            }
            return &vp_->vehicle() == &rhs.vp_->vehicle() && vp_->part_index() == rhs.vp_->part_index();
        }
};

/**
 * The generic range, it misses the `bool contained(size_t)` function that is
 * required by the iterator class. You need to derive from it and implement
 * that function. It uses the curiously recurring template pattern (CRTP),
 * so use your derived range class as @ref range_type.
 */
template<typename range_type>
class generic_vehicle_part_range : public std::ranges::view_interface<range_type>
{
    private:
        ::vehicle *vehicle_ = nullptr;

        generic_vehicle_part_range() = default;
        generic_vehicle_part_range( ::vehicle &v ) : vehicle_( &v ) { }

    public:
        // Templated because see top of file.
        template<typename T = ::vehicle>
        auto part_count() const -> size_t {
            return static_cast<const T &>( *vehicle_ ).part_count();
        }

        using iterator = vehicle_part_iterator<range_type>;

        auto begin() const -> iterator {
            return iterator( static_cast<const range_type *>( this ), 0 );
        }

        auto end() const -> iterator {
            return iterator( static_cast<const range_type *>( this ), part_count() );
        }

        auto vehicle() const -> ::vehicle& { // *NOPAD*
            return *vehicle_;
        }
        friend range_type;
};

/** A range that contains all parts of the vehicle. */
class vehicle_part_range : public generic_vehicle_part_range<vehicle_part_range>
{
    public:
        vehicle_part_range( ::vehicle &v ) : generic_vehicle_part_range( v ) { }

        auto matches( const size_t /*part*/ ) const -> bool { return true; }
};

/** A range that contains parts that have a given feature and (optionally) are not broken. */
template<typename feature_type>
class vehicle_part_with_feature_range : public
    generic_vehicle_part_range<vehicle_part_with_feature_range<feature_type>>
{
    private:
        feature_type feature_;
        part_status_flag required_;

    public:
        vehicle_part_with_feature_range( ::vehicle &v, feature_type f, part_status_flag r ) :
            generic_vehicle_part_range<vehicle_part_with_feature_range<feature_type>>( v ),
                    feature_( std::move( f ) ), required_( r ) { }

        auto matches( size_t part ) const -> bool;
};


