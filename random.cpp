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
#include <stdio.h>
#include <string.h>
#include <math.h>
#ifdef __GNUC__
#include <sys/time.h>
#include <pthread.h>
#else
#include <time.h>
#include <windows.h>
#endif

#include "sre.h"

#ifndef __GNUC__

#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
  #define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else
  #define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif

void gettimeofday(struct timeval *tv, struct timezone *tz) {
    FILETIME ft;
    uint64_t tmpres = 0;
    if (NULL != tv) {
        GetSystemTimeAsFileTime(&ft);
        tmpres |= ft.dwHighDateTime;
        tmpres <<= 32;
        tmpres |= ft.dwLowDateTime;
        tmpres /= 10;  /*convert into microseconds*/
                       /*converting file time to unix epoch*/
        tmpres -= DELTA_EPOCH_IN_MICROSECS; 
        tv->tv_sec = (long)(tmpres / 1000000UL);
        tv->tv_usec = (long)(tmpres % 1000000UL);
    }
}

#endif


sreGenericRNG::sreGenericRNG() {
//    printf("sreGenericRNG constructor called.\n");
    storage = 0;
    storage_size = 0;
    last_random_n_power_of_2 = - 1; 
}

// Return a random integer value of 0 or 1.

int sreGenericRNG::RandomBit() {
    unsigned int r;
    if (storage_size > 0) {
       int bit;
       bit = storage & 0x1;
       storage >>= 1;
       storage_size--;
       return bit;
    }
    r = Random32();
    storage = (r & 0xFFFFFFFE) >> 1;
    storage_size = 31;
    return r & 0x1;
}

// Return a random integer value from 0 to 255.

int sreGenericRNG::Random8() {
    unsigned int r;
    if (storage_size >= 8) {
        r = storage & 0xFF;
        storage >>= 8;
        storage_size -= 8;
        return r;
    }
    r = Random32();
    storage += ((r & 0xFFFFFF00) >> 8) << storage_size;
    storage_size += 24;
    return r & 0xFF;
}

// Return a random integer value from 0 to 65535.

int sreGenericRNG::Random16() {
    unsigned int r;
    if (storage_size >= 16) {
        r = storage & 0xFFFF;
        storage >>= 16;
        storage_size -= 16;
        return r;
    }
    r = Random32();
    storage += ((r & 0xFFFF0000) >> 16) << storage_size;
    storage_size += 16;
    return r & 0xFFFF;
}



int sreGenericRNG::RandomBits(int n) {
    unsigned int r;
    unsigned int mask = (1 << n) - 1;
    if (storage_size >= n) {
        r = storage & mask;
        storage >>= n;
        storage_size -= n;
        return r;
    }
    r = Random32();
    storage |= ((r & (0xFFFFFFFF ^ mask)) >> n) << storage_size;
    storage_size += 32 - n;
    return r & mask;
}


// The following functions use the RNG implementation and could potentially
// use a different RNG from the one above.

// Randomize the seed of the random number generator with a value from the system timer.

void sreGenericRNG::SeedWithTimer() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    /* The multiplication by 1000000 will overflow, but that is not important. */
    Seed(tv.tv_sec * 1000000 + tv.tv_usec);
}

static unsigned char power_of_2_table[257] = {
    255, 0, 1, 255, 2, 255, 255, 255, 3, 255, 255, 255, 255 ,255, 255, 255, 
    4, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 ,255, 255, 255, 
    5, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 ,255, 255, 255, 
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 ,255, 255, 255,
    6, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 ,255, 255, 255, 
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 ,255, 255, 255, 
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 ,255, 255, 255, 
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 ,255, 255, 255, 
    7, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 ,255, 255, 255, 
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 ,255, 255, 255, 
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 ,255, 255, 255, 
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 ,255, 255, 255, 
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 ,255, 255, 255, 
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 ,255, 255, 255, 
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 ,255, 255, 255, 
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 ,255, 255, 255,
    8
};

// Return an integer from 0 to n - 1.
// This function becomes biased as for very large values for n because of the use
// of the modulo operator.

int sreGenericRNG::RandomInt(int n) {
    // Fast path to most common occurence of repeating power of two.
    if (n == last_random_n_power_of_2) {
        return RandomBits(last_random_n_power_of_2_bit_count);
    }
    // Optimize power of two.
    if (n <= 256) {
        int bit_count = power_of_2_table[n];
        if (bit_count != 255) {
            last_random_n_power_of_2 = n;
            last_random_n_power_of_2_bit_count = bit_count;
            return RandomBits(bit_count);
        }
        return Random16() % n;
    }
    if (n <= 65536 && (n & 0xFF) == 0) {
        int bit_count = power_of_2_table[n >> 8];
        if (bit_count != 255) {
            last_random_n_power_of_2 = n;
            last_random_n_power_of_2_bit_count = bit_count + 8;
            return RandomBits(bit_count + 8);
        }
    }
    return Random32() % n;
}

/** 
 * Return a random double from 0 to range (exclusive).
 */

double sreGenericRNG::RandomDouble(double range) {
    return (double)Random32() * range / ((uint64_t)1 << 32);
}

/**
 * Return a random float from 0 to range (exclusive).
 */

float sreGenericRNG::RandomFloat(float range) {
    return (float)Random32() * range / ((uint64_t)1 << 32);
}

// Return a random float from min_bound to max_bound (exclusive).

float sreGenericRNG::RandomWithinBounds(float min_bound, float max_bound) {
    return min_bound + RandomFloat(max_bound - min_bound);
}

// Return a random double from min_bound to max_bound (exclusive).

double sreGenericRNG::RandomWithinBounds(double min_bound, double max_bound) {
    return min_bound + RandomDouble(max_bound - min_bound);
}

/* Calculate a random permutation of the numbers 0 to (n - 1). */

void sreGenericRNG::CalculateRandomOrder(int *order, int n) {
    int i;
    for (i = 0; i < n; i++)
        order[i] = i;
    for (i = 0; i < n; i++) {
          int j, t;
          /* Swap element i with random element j. */
          j = RandomInt(n);
          t = order[i];
          order[i] = order[j];
          order[j] = t;
    }
}

/* Complementary-multiply-with-carry random number generator. */

#define PHI 0x9e3779b9

// Create a random number generator data structure and returns it. The RNG is initialized
// with a seed of 0.

sreDefaultRNG::sreDefaultRNG() {
    state_size = SRE_DEFAULT_RNG_STATE_SIZE;
    c = 362436;
    Q = new unsigned int[state_size];
    _index = state_size - 1;
    Seed(0);
}

sreDefaultRNG::sreDefaultRNG(int _state_size) {
    state_size = _state_size;
    c = 362436;
    Q = new unsigned int[state_size];
    _index = _state_size - 1;
    Seed(0);
}

sreDefaultRNG::~sreDefaultRNG() {
    delete [] Q;
}

// Seed the random number generator with an unsigned integer from 0 to 2^32 - 1.

void sreDefaultRNG::Seed(unsigned int seed) {
    int i;
    Q[0] = seed;
    Q[1] = seed + PHI;
    Q[2] = seed + PHI + PHI;
    for (i = 3; i < state_size; i++)
        Q[i] = Q[i - 3] ^ Q[i - 2] ^ PHI ^ i;
}
 
// Return a random integer value from 0 to 2^32 - 1;

unsigned int sreDefaultRNG::Random32() {
    uint64_t t, a = 18782LL;
    unsigned int x, r = 0xfffffffe;
    _index = (_index + 1) & (state_size - 1);
    t = a * Q[_index] + c;
    c = (t >> 32);
    x = t + c;
    if (x < c) {
         x++;
         c++;
    }
    return (Q[_index] = r - x);
}
