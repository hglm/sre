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
#ifndef _WIN32
#include <sys/time.h>
#include <pthread.h>
#else
#include <time.h>
#include <windows.h>
#endif

#include "sre.h"
#include "sreRandom.h"

#ifdef _WIN32

#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
  #define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else
  #define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif

static void gettimeofday(struct timeval *tv, struct timezone *tz) {
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

sreRNG::sreRNG() {
    storage = 0;
    storage_size = 0;
    SetLastPowerOfTwoData(0, 0); 
#ifdef SRE_RANDOM_CACHE_NON_POWER_OF_TWO
    SetLastNonPowerOfTwoData(0, 0);
#endif
}

// Randomize the seed of the random number generator with a value from the system timer.

void sreRNG::SeedWithTimer() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    /* The multiplication by 1000000 will overflow, but that is not important. */
    Seed(tv.tv_sec * 1000000 + tv.tv_usec);
}

#if 0

// This function is now provided inline in the class definition.

unsigned int sreRNG::RandomBits(unsigned int n) {
    unsigned int r;
    unsigned int mask = ((unsigned int)1 << n) - 1;
    if (storage_size >= n) {
        r = storage & mask;
        storage >>= n;
        storage_size -= n;
        return r;
    }
    r = Random32();
#if SRE_STORAGE_SIZE == 64
    // Add 32 - n bits.
    storage <<= 32 - n;
    storage += r >> n;
#else
    storage |= ((r & (0xFFFFFFFF ^ mask)) >> n) << storage_size;
#endif
    storage_size += 32 - n;
    return r & mask;
}

#else

// Helper function for the inline version of RandomBits(n_bits) for when
// the storage size is known to be insufficient.

unsigned int sreRNG::RandomBitsNeedStorage(unsigned int n_bits) {
    unsigned int r = Random32();
    // Just append the new bits at the end of the storage, possibly losing bits.
    // The higher order n_bits bits of r will be used for the return value.
    storage += r << storage_size;
#if SRE_STORAGE_SIZE == 64
    // Nothing to do for 64-bit storage; there will always be room for at least
    // 32 bits since the maximum request size is 32.
#else
    // Adjust the storage size, limiting it to 32.
    storage_size = (storage_size & 32) + (((storage_size & 32) >> 5) ^ 1) * storage_size;
#endif
    storage_size += 32 - n_bits;
    return r >> (32 - n_bits);
}

#endif

#ifdef SRE_RANDOM_LOG2_LOOKUP_TABLE

// Table with log2(n) + 1, indexed with n = 0 to n = 256. If n is not
// a power of two (there is a remainder), bit 7 is set (0x80 added). For n = 0,
// the table value is 40 (bit 6 is set).
const unsigned char sre_internal_random_table[257] SRE_ALIGNED(256) = {
    0x40, 1, 2, 0x82, 3, 0x83, 0x83, 0x83, 4, 0x84, 0x84, 0x84, 0x84 ,0x84, 0x84, 0x84, 
    5, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85 ,0x85, 0x85, 0x85, 
    6, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86 ,0x86, 0x86, 0x86, 
    0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86 ,0x86, 0x86, 0x86,
    7, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87 ,0x87, 0x87, 0x87, 
    0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87 ,0x87, 0x87, 0x87, 
    0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87 ,0x87, 0x87, 0x87, 
    0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87 ,0x87, 0x87, 0x87, 
    8, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88 ,0x88, 0x88, 0x88, 
    0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88 ,0x88, 0x88, 0x88, 
    0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88 ,0x88, 0x88, 0x88, 
    0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88 ,0x88, 0x88, 0x88, 
    0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88 ,0x88, 0x88, 0x88, 
    0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88 ,0x88, 0x88, 0x88, 
    0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88 ,0x88, 0x88, 0x88, 
    0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88 ,0x88, 0x88, 0x88, 
    9
};

#endif

// RandomInt(int n): Return a random integer from 0 to n - 1.

#ifdef SRE_RANDOM_INT_INCLUDE_REMAINDER_STRATEGY

// Version using remainder optimized for relatively slow bits function (Random32()).
// When SRE_RANDOM_LOG2_LOOKUP_TABLE is enabled:
// This function has only a minor bias (1 part in 256) for non-power-of-two values
// <= 65535, a smaller bias for 65536 > non-power-of-two value  < (1 << 24), and as
// n increases above (1 << 24) the bias gradually increases to an unacceptable level
// for very large values for n because of the use of the modulo operator.
// n <= 0 is an invalid argument.
// When SRE_RANDOM_CALCULATE_LOG2 is enabled, the bias is controlled to 1 part in
// 256 except for very large values approaching INT_MAX where bias becomes
// unacceptable.

unsigned int sreRNG::RandomIntRemainderSlowBits(unsigned int n) {
    // Fast path to most common occurence of repeating power of two.
    if (n == GetLastPowerOfTwo()) {
        return RandomBits(GetLastPowerOfTwoShift());
    }
#ifdef SRE_RANDOM_LOG2_LOOKUP_TABLE
    // Handle 0 < n <= 256, recognizing powers of two, and and use modulo with a
    // measured number of bits for other values.
    if (n <= 256) {
        // Function returns log2(n) + 1 in bits 0-3, bit 7 is set if n is not
        // a power of two, is bit 6 set if n is zero.
        unsigned int value = GetTableValue(n, 0, 9);
        unsigned int shift = TableValueGetShift(value);
        if (TableValueIsPowerOfTwo(value)) {
            SetLastPowerOfTwoData(n, shift);
            return RandomBits(shift);
        }
        // Use modulo of a random value eight bits larger than 1 << (log2(n) + 1),
        // introducing only a minor bias for n <= 255.
        // Note that shift is guaranteed to be <= 7 at this point, so RandomBits()
        // will fit in an unsigned short.
        return (unsigned short)RandomBits(shift + 9) % (unsigned char)n;
    }
    // Handle 256 < n <= 65536, recognizing powers of two, and and use modulo with a
    // measured number of bits for other values.
    if (n <= 65536) {
        // Function returns log2(n) + 1 in bits 0-3, bit 7 is set if n is not
        // a power of two.
        unsigned int value = GetTableValue(n, 8, 9);
        unsigned int shift = TableValueGetShift(value) + 8;
        if ((n & 0xFF) == 0 && TableValueIsPowerOfTwo(value)) {
            SetLastPowerOfTwoData(n, shift);
            return RandomBits(shift);
        }
        // Use a modulo of a random value eight bits larger than 1 << (log2(n) + 1),
        // introducing only a minor bias.
        return RandomBits(shift + 9) % (unsigned short)n;
    }
#if 0
    // This optional code tries to save a few random bits generated for
    // 65536 < n < 2 ^ 24, but adds to the already present processing overhead.
    if (n <= (1 << 24)) {
        unsigned int value = GetTableValue(n, 16, 9);
        unsigned int shift = TableValueGetShift(value) + 16;
        if ((n & 0xFFFF) == 0 && TableValueIsPowerOfTwo(value)) {
            SetLastPowerOfTwoData(n, shift);
            return RandomBits(shift);
        }
        // Use a modulo of a random value eight bits larger than 1 << (log2(n) + 1),
        // introducing only a minor bias.
        // Note that shift is guaranteed to be <= 23 at this point.
        return RandomBits(shift + 9) % n;
    }
#endif
#else
    // SRE_RANDOM_CALCULATE_LOG2 is defined.
    unsigned int shift = CalculateBitsNeeded(n);
    // Detects powers of two.
    if (n == ((unsigned int)1 << shift)) {
        SetLastPowerOfTwoData(n, shift);
        return RandomBits(shift);
    }
    if (shift <= 24)
        return RandomBits(shift + 8) % n;
#endif
    return Random32() % n;
}

unsigned int sreRNG::RandomIntRemainderSlowBitsMax65536(unsigned int n) {
    // Fast path to most common occurence of repeating power of two.
    if (n == GetLastPowerOfTwo()) {
        return RandomBits(GetLastPowerOfTwoShift());
    }
#ifdef SRE_RANDOM_LOG2_LOOKUP_TABLE
    // Handle 0 < n <= 256, recognizing powers of two, and and use modulo with a
    // measured number of bits for other values.
    if (n <= 256) {
        // Function returns log2(n) + 1 in bits 0-3, bit 7 is set if n is not
        // a power of two, is bit 6 set if n is zero.
        unsigned int value = GetTableValue(n, 0, 9);
        unsigned int shift = TableValueGetShift(value);
        if (TableValueIsPowerOfTwo(value)) {
            SetLastPowerOfTwoData(n, shift);
            return RandomBits(shift);
        }
        // Use modulo of a random value eight bits larger than 1 << (log2(n) + 1),
        // introducing only a minor bias for n <= 255.
        // Note that shift is guaranteed to be <= 7 at this point, so RandomBits()
        // will fit in an unsigned short.
        return (unsigned short)RandomBits(shift + 9) % (unsigned char)n;
    }
    // Handle 256 < n <= 65536, recognizing powers of two, and and use modulo with a
    // measured number of bits for other values.
    // Function returns log2(n) + 1 in bits 0-3, bit 7 is set if n is not
    // a power of two.
    unsigned int value = GetTableValue(n, 8, 9);
    unsigned int shift = TableValueGetShift(value) + 8;
    if ((n & 0xFF) == 0 && TableValueIsPowerOfTwo(value)) {
        SetLastPowerOfTwoData(n, shift);
        return RandomBits(shift);
    }
    // Use a modulo of a random value eight bits larger than 1 << (log2(n) + 1),
    // introducing only a minor bias.
    return RandomBits(shift + 9) % (unsigned short)n;
#else
    // SRE_RANDOM_CALCULATE_LOG2 is defined.
    unsigned int shift = CalculateBitsNeededMax65536(n);
    // Detects powers of two.
    if (n == ((unsigned int)1 << shift)) {
        SetLastPowerOfTwoData(n, shift);
        return RandomBits(shift);
    }
    return RandomBits(shift + 8) % (unsigned short)n;
#endif
}

#endif

#ifdef SRE_RANDOM_LOG2_LOOKUP_TABLE

// Helper function to determine the needed bits to represent the range
// 0 to n - 1 for any n up to (1 << 31) using multiple table look-ups.
// The value returned is equivalent to floor(log2(n - 1)) + 1.

unsigned int sreRNG::GetBitsNeededLookupTable(unsigned int n) {
    unsigned int log2plusone = GetLog2PlusOneInRange(n, 24, 8);
    // log2plusone will be zero if no bits are set in bit range 24-31
    if (log2plusone != 0)
        goto end;
    log2plusone = GetLog2PlusOneInRange(n, 16, 8);
    if (log2plusone != 0)
        goto end;
    log2plusone = GetLog2PlusOneInRange(n, 8, 8);
    if (log2plusone != 0)
        goto end;
    log2plusone = GetLog2PlusOneInRange(n, 0, 8);
end :
    // Set shift to floor(log2(n)).
    unsigned int shift = log2plusone - 1;
    // If n is not a power of two, one more bit is needed.
    // Rely on the fact that bit 31 will be set when subtracting n from 2 ^ shift
    // and n is not power of two.
    shift += (((unsigned int)1 << shift) - n) >> 31;
    VALIDATE_BITS_NEEDED(n, shift);
    return shift;
}

unsigned int sreRNG::GetBitsNeededLookupTableMax65536(unsigned int n) {
    unsigned int log2plusone = GetLog2PlusOneInRange(n, 8, 8);
    if (log2plusone != 0)
        goto end;
    log2plusone = GetLog2PlusOneInRange(n, 0, 8);
    // When n = 2^16, log2plusone will be zero, but must be set to 17.
    // log2plusone values <= 16 must remain untouched.
    log2plusone += (((unsigned int)17 << 16) & ((1 << log2plusone) - 1)) >> 16;
end :
    unsigned int shift = log2plusone - 1;
    // If n is not a power of two, one more bit is needed.
    // Rely on the fact that bit 31 will be set when subtracting n from 2 ^ shift
    // and n is not power of two.
    shift += (((unsigned int)1 << shift) - n) >> 31;
    VALIDATE_BITS_NEEDED(n, shift);
    return shift;
}

unsigned int sreRNG::GetBitsNeededLookupTableMax256(unsigned int n) {
    unsigned int log2plusone = GetLog2PlusOneInRange(n, 0, 9);
    unsigned int shift = log2plusone - 1;
    // When n = 256, log2plusone will be zero, so shift will be 0xFFFFFFFF;
    // log2plusone will be zero. Set shift to 8 in this case. shift values
    // <= 7 must remain untouched.
    shift += ((unsigned int)(9 << 8) & shift) >> 8;
    // If n is not a power of two, one more bit is needed.
    // Rely on the fact that bit 31 will be set when subtracting n from 2 ^ shift
    // and n is not power of two.
    shift += (((unsigned int)1 << shift) - n) >> 31;
    VALIDATE_BITS_NEEDED(n, shift);
    return shift;
}

#endif

/* Calculate a random permutation of the numbers 0 to (n - 1). */

void sreRNG::CalculateRandomOrder(unsigned int *order, unsigned int n) {
    unsigned int i;
    for (i = 0; i < n; i++)
        order[i] = i;
    for (i = 0; i < n; i++) {
          unsigned int j, t;
          /* Swap element i with random element j. */
          j = RandomInt(n);
          t = order[i];
          order[i] = order[j];
          order[j] = t;
    }
}

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

// This global variable definition will trigger the constructor at
// program initialization time.
static sreCMWCRNG sre_internal_rng;
static sreRNG *sre_default_rng = NULL;

SRE_API sreRNG *sreGetDefaultRNG() {
   if (sre_default_rng == NULL)
       sre_default_rng = &sre_internal_rng;
   return sre_default_rng;
}

SRE_API void sreSetDefaultRNG(sreRNG* rng) {
    if (rng == NULL)
        sre_default_rng = &sre_internal_rng;
    else
        sre_default_rng = rng;
}
