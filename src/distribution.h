#pragma once

#include "memory_fast.h"

class JsonIn;

struct int_distribution_impl;

// This represents a probability distribution over the integers, which is
// abstract and can be read from a JSON definition
class int_distribution
{
    public:
        int_distribution();
        explicit int_distribution( int value );

        int minimum() const;
        int sample( int scale = 1 ) const;
        std::string description() const;

        void deserialize( JsonIn & );
    private:
        shared_ptr_fast<int_distribution_impl> impl_;
};


