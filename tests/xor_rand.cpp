#include "xor_rand.h"

// original: https://codingforspeed.com/using-faster-psudo-random-generator-xorshift/
unsigned int xor_rand() {
    static unsigned int x = 123456789;
    static unsigned int y = 362436069;
    static unsigned int z = 521288629;
    static unsigned int w = 88675123;

    const unsigned int t = x ^ (x << 11);

    // Rotate the static values (w rotation in return statement):
    x = y;
    y = z;
    z = w;

    return w = w ^ (w >> 19) ^ (t ^ (t >> 8));
}
