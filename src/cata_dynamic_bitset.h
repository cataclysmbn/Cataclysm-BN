#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

/**
 * Minimal runtime-sized bitset used for level_cache dirty flags.
 *
 * Provides the same interface subset as std::bitset<N> that is actually used
 * in the codebase: set/reset (whole or per-bit), none/any/all, operator[],
 * size(), and the bit-shift operators needed by shift_bitset_cache().
 *
 * Storage: packed uint64_t words, bit i lives in word i/64 at bit i%64.
 */
class cata_dynamic_bitset
{
    public:
        // Non-const reference proxy returned by operator[](size_t)
        struct reference {
            uint64_t &word;
            uint64_t  mask;
            reference( uint64_t &w, uint64_t m ) : word( w ), mask( m ) {}
            operator bool() const {
                return ( word & mask ) != 0;
            }
            reference &operator=( bool b ) {
                if( b ) {
                    word |= mask;
                } else {
                    word &= ~mask;
                }
                return *this;
            }
        };

        cata_dynamic_bitset() = default;

        explicit cata_dynamic_bitset( size_t n )
            : data_( ( n + 63 ) / 64, uint64_t( 0 ) ), size_( n ) {}

        size_t size() const {
            return size_;
        }

        // ----- whole-set operations -----

        void set() {
            for( auto &w : data_ ) {
                w = ~uint64_t( 0 );
            }
            // Clear unused high bits of the last word so that all() works correctly
            _trim_last_word();
        }

        void reset() {
            for( auto &w : data_ ) {
                w = 0;
            }
        }

        // ----- per-bit operations -----

        void set( size_t pos ) {
            data_[pos / 64] |= uint64_t( 1 ) << ( pos % 64 );
        }

        void reset( size_t pos ) {
            data_[pos / 64] &= ~( uint64_t( 1 ) << ( pos % 64 ) );
        }

        bool test( size_t pos ) const {
            return ( data_[pos / 64] >> ( pos % 64 ) ) & 1;
        }

        bool operator[]( size_t pos ) const {
            return test( pos );
        }

        reference operator[]( size_t pos ) {
            return reference( data_[pos / 64], uint64_t( 1 ) << ( pos % 64 ) );
        }

        // ----- query -----

        bool none() const {
            for( auto w : data_ ) {
                if( w ) {
                    return false;
                }
            }
            return true;
        }

        bool any() const {
            return !none();
        }

        bool all() const {
            if( data_.empty() ) {
                return true;
            }
            const size_t full_words = size_ / 64;
            for( size_t i = 0; i < full_words; ++i ) {
                if( data_[i] != ~uint64_t( 0 ) ) {
                    return false;
                }
            }
            const size_t rem = size_ % 64;
            if( rem ) {
                const uint64_t mask = ( uint64_t( 1 ) << rem ) - 1;
                if( ( data_[full_words] & mask ) != mask ) {
                    return false;
                }
            }
            return true;
        }

        // ----- shift operators (used by shift_bitset_cache) -----
        // >>= k : bit[i] = old bit[i+k]  (moves content towards lower indices, zeros high end)
        cata_dynamic_bitset &operator>>=( size_t k ) {
            if( k == 0 ) {
                return *this;
            }
            if( k >= size_ ) {
                reset();
                return *this;
            }
            const size_t word_shift = k / 64;
            const size_t bit_shift  = k % 64;
            const size_t n = data_.size();

            if( bit_shift == 0 ) {
                for( size_t i = 0; i + word_shift < n; ++i ) {
                    data_[i] = data_[i + word_shift];
                }
            } else {
                const size_t rbits = 64 - bit_shift;
                for( size_t i = 0; i + word_shift + 1 < n; ++i ) {
                    data_[i] = ( data_[i + word_shift] >> bit_shift ) |
                               ( data_[i + word_shift + 1] << rbits );
                }
                // last non-zero source word
                data_[n - word_shift - 1] = data_[n - 1] >> bit_shift;
            }
            for( size_t i = n - word_shift; i < n; ++i ) {
                data_[i] = 0;
            }
            return *this;
        }

        // <<= k : bit[i+k] = old bit[i]  (moves content towards higher indices, zeros low end)
        cata_dynamic_bitset &operator<<=( size_t k ) {
            if( k == 0 ) {
                return *this;
            }
            if( k >= size_ ) {
                reset();
                return *this;
            }
            const size_t word_shift = k / 64;
            const size_t bit_shift  = k % 64;
            const size_t n = data_.size();

            if( bit_shift == 0 ) {
                for( size_t i = n; i > word_shift; --i ) {
                    data_[i - 1] = data_[i - 1 - word_shift];
                }
            } else {
                const size_t rbits = 64 - bit_shift;
                for( size_t i = n; i > word_shift + 1; --i ) {
                    data_[i - 1] = ( data_[i - 1 - word_shift] << bit_shift ) |
                                   ( data_[i - 2 - word_shift] >> rbits );
                }
                data_[word_shift] = data_[0] << bit_shift;
            }
            for( size_t i = 0; i < word_shift; ++i ) {
                data_[i] = 0;
            }
            // Keep high bits clean
            _trim_last_word();
            return *this;
        }

    private:
        std::vector<uint64_t> data_;
        size_t                size_ = 0;

        void _trim_last_word() {
            const size_t rem = size_ % 64;
            if( rem && !data_.empty() ) {
                const uint64_t mask = ( uint64_t( 1 ) << rem ) - 1;
                data_.back() &= mask;
            }
        }
};
