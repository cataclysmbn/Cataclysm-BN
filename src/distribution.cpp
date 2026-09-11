#include "distribution.h"

#include <random>

#include "json.h"
#include "random/rng.h"

struct int_distribution_impl {
    virtual ~int_distribution_impl() = default;
    virtual int minimum() const = 0;
    virtual int sample( int scale = 1 ) = 0;
    virtual std::string description() const = 0;
};

struct fixed_distribution : int_distribution_impl {
    int value;

    explicit fixed_distribution( int v )
        : value( v )
    {}

    int minimum() const override {
        return value;
    }

    int sample( int scale ) override {
        return value * scale;
    }

    std::string description() const override {
        return string_format( "Fixed(%d)", value );
    }
};

struct range_distribution : int_distribution_impl {
    int min;
    int max;

    explicit range_distribution( int min, int max )
        : min( min )
        , max( max )
    {}

    int minimum() const override {
        return std::min( min, max );
    }

    int sample( int scale ) override {
        return rng( min * scale, max * scale );
    }

    std::string description() const override {
        return string_format( "Range(%d - %d)", min, max );
    }
};

struct poisson_distribution : int_distribution_impl {
    double mean;

    explicit poisson_distribution( double mean )
        : mean( mean )
    {}

    int minimum() const override {
        return 0;
    }

    int sample( int scale ) override {
        std::poisson_distribution<int> dist( mean * scale );
        return dist( rng_get_engine() );
    }

    std::string description() const override {
        std::poisson_distribution<int> dist( mean );
        return string_format( "Poisson(%.0f)", dist.mean() );
    }
};

struct chance_distribution : int_distribution_impl {
    double chance;

    explicit chance_distribution( double chance )
        : chance( chance )
    {}

    int minimum() const override {
        return 0;
    }

    int sample( int scale ) override {
        return ( chance * scale ) >= rng_float( 0, 1 ) ? 1 : 0;
    }

    std::string description() const override {
        return string_format( "Chance(%.2f)", chance );
    }
};

int_distribution::int_distribution()
    : impl_( make_shared_fast<fixed_distribution>( 0 ) )
{}

int_distribution::int_distribution( int v )
    : impl_( make_shared_fast<fixed_distribution>( v ) )
{}

int int_distribution::minimum() const
{
    return impl_->minimum();
}

int int_distribution::sample( int scale ) const
{
    return impl_->sample( scale );
}

std::string int_distribution::description() const
{
    return impl_->description();
}

void int_distribution::deserialize( JsonIn &jsin )
{
    if( jsin.test_int() ) {
        int v = jsin.get_int();
        impl_ = make_shared_fast<fixed_distribution>( v );
    } else if( jsin.test_object() ) {
        JsonObject jo = jsin.get_object();
        if( jo.has_member( "poisson" ) ) {
            double mean = jo.get_float( "poisson", true );
            impl_ = make_shared_fast<poisson_distribution>( mean );
        } else if( jo.has_member( "chance" ) ) {
            double chance = jo.get_float( "chance", true );
            impl_ = make_shared_fast<chance_distribution>( chance );
        } else {
            jo.throw_error( R"(Expected "poisson" member)" );
        }
    } else if( jsin.test_array() ) {
        JsonArray ja = jsin.get_array();
        if( ja.size() != 2 ) {
            ja.throw_error( "Range should be in format [min, max]." );
        }
        impl_ = make_shared_fast<range_distribution>( ja.get_int( 0 ), ja.get_int( 1 ) );
    } else {
        jsin.error( "expected number, array, or object" );
    }
}
