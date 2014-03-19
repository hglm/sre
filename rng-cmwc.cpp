/*

Copyright (c) 2014 Harm Hanemaaijer <fgenfb@yahoo.com>

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

*/


#include <stdlib.h>
#include <stdint.h>

#include "sreRandom.h"

/* Complementary-multiply-with-carry random number generator. */

#define PHI 0x9e3779b9

void sreCMWCRNG::Initialize(unsigned int _state_size) {
    state_size = _state_size;
    c = 362436;
#if defined(__GNUC__) && defined(SRE_RANDOM_OPTIMIZE_ALIGNMENT_AND_PACKING_WITH_GCC)
    if (!posix_memalign((void **)&Q, SRE_LINE_SIZE, state_size * sizeof(unsigned int)))
        // When aligned allocation fails, fall back to the new operator below.  
#endif
    Q = new unsigned int[state_size];
    _index = _state_size - 1;
    Seed(0);
}

// Create a random number generator data structure and returns it. The RNG is initialized
// with a seed of 0.

sreCMWCRNG::sreCMWCRNG(unsigned int _state_size) {
    Initialize(_state_size);
}

sreCMWCRNG::sreCMWCRNG() {
    Initialize(SRE_RANDOM_CMWC_RNG_DEFAULT_STATE_SIZE);
}

sreCMWCRNG::~sreCMWCRNG() {
#if defined(__GNUC__) && defined(SRE_RANDOM_OPTIMIZE_ALIGNMENT_AND_PACKING_WITH_GCC)
    free(Q);
#else
    delete [] Q;
#endif
}

// Seed the random number generator with an unsigned integer from 0 to 2^32 - 1.

void sreCMWCRNG::Seed(unsigned int seed) {
    int i;
    Q[0] = seed;
    Q[1] = seed + PHI;
    Q[2] = seed + PHI + PHI;
    for (i = 3; i < state_size; i++)
        Q[i] = Q[i - 3] ^ Q[i - 2] ^ PHI ^ i;
}
 
// Return a random integer value from 0 to 2^32 - 1;

unsigned int sreCMWCRNG::Random32() {
    uint64_t t;
    const uint64_t a = 18782LL;
    unsigned int x;
    const unsigned int r = 0xfffffffe;
    _index = (_index + 1) & (state_size - 1);
    t = a * Q[_index] + c;
    c = (t >> 32);
    x = t + c;
    if (x < c) {
         x++;
         c++;
    }
    Q[_index] = r - x;
    return Q[_index];
}

