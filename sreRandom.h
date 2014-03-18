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

#ifndef __SRE_RANDOM_H__
#define __SRE_RANDOM_H__

// Generic helper definitions for shared library support
#if defined _WIN32 || defined __CYGWIN__
  #define SRE_RANDOM_HELPER_SHARED_IMPORT __declspec(dllimport)
  #define SRE_RANDOM_HELPER_SHARED_EXPORT __declspec(dllexport)
  #define SRE_RANDOM_HELPER_SHARED_LOCAL
#else
  #if __GNUC__ >= 4
    #define SRE_RANDOM_HELPER_SHARED_IMPORT __attribute__ ((visibility ("default")))
    #define SRE_RANDOM_HELPER_SHARED_EXPORT __attribute__ ((visibility ("default")))
    #define SRE_RANDOM_HELPER_SHARED_LOCAL  __attribute__ ((visibility ("hidden")))
  #else
    #define SRE_RANDOM_HELPER_SHARED_IMPORT
    #define SRE_RANDOM_HELPER_SHARED_EXPORT
    #define SRE_RANDOM_HELPER_SHARED_LOCAL
  #endif
#endif

// Now we use the generic helper definitions above to define SRE_RANDOM_API and SRE_RANDOM_LOCAL.
// SRE_RANDOM_API is used for the public API symbols. It either DLL imports or DLL exports (or does nothing
// for static build). SRE_RANDOM_LOCAL is used for non-api symbols.

#ifdef SRE_RANDOM_SHARED
  // Defined if SRE is compiled as a shared library/DLL.
  #ifdef SRE_RANDOM_SHARED_EXPORTS
  // Defined if we are building the SRE shared library (instead of using it).
    #define SRE_RANDOM_API SRE_RANDOM_HELPER_SHARED_EXPORT
  #else
    #define SRE_RANDOM_API SRE_RANDOM_HELPER_SHARED_IMPORT
  #endif // SRE_RANDOM_SHARED_EXPORTS
  #define SRE_RANDOM_LOCAL SRE_RANDOM_HELPER_SHARED_LOCAL
#else // SRE_RANDOM_SHARED is not defined: this means SRE is a static lib.
  #define SRE_RANDOM_API
  #define SRE_RANDOM_LOCAL
#endif // SRE_RANDOM_SHARED

#include <stdint.h>  // For uint64_t.
#include <math.h>
#include <limits.h>  // For __WORDSIZE.
#include <float.h>   // For DBL_MAX.

// Random number generator classes, implemented as an independent library.
//
// Provided public member functions:
//
// Core functions:
//
// void Seed(unsigned int value)
//     Seed the RNG with a 32-bit value.
// void SeedWithTimer()
//     Seed the RNG with a 32-bit value derived from the system timer, which
//     has the effect of a random seed.
// unsigned int Random32()
//     Return 32 random bits. Used internally as the source of random bits.
// unsigned int RandomBits(unsigned int n_bits)
//     Get n random bits, 0 <= n <= 32.
// unsigned int RandomInt(unsigned int n)
//     Return a random unsigned integer in the range 0 to n - 1. Uses either
//     the empirical or remainder strategy. Detects and caches powers of two.
//     Consecutive calls with the same power of two will be fast.
// unsigned int RandomIntMax256(unsigned int n)
//     Version of RandomInt(n) optimized for n <= 256.
// unsigned int RandomIntMax65536(unsigned int n)
//     Version of RandomInt(n) optimized for n <= 2 ^ 16 (65536).
// unsigned int RandomIntPowerOfTwo(unsigned int n)
//     Optimized version of RandomInt() for when n is guaranteed to be a power
//     of two. Other values will result in undefined behaviour.
// unsigned int RandomIntPowerOfTwoWithShift(unsigned int shift)
//     Return random integer in any power of two range 0 to n - 1 with specified
//     shift = log2(n), n = 1 << shift. Different from RandomBits(shift) because
//     it caches the power of two range value.
// unsigned int RandomIntPowerOfTwoMax256(unsigned int n)
//     Optimized version of RandomInt() for when n is guaranteed to be a power
//     two less than or equal to 2 ^ 8 (256).
// unsigned int RandomIntPowerOfTwoMax65536(unsigned int n)
//     Optimized version of RandomInt() for when n is guaranteed to be a power
//     two less than or equal to 2 ^ 16.
// unsigned int RandomIntPowerOfTwoRepeat()
//     Repeat random integer function with the last power of two range previously
//     used in any of the RandomInt functions above, or the specialized function
//     below. This the fastest of the random integer functions.
// void RandomIntPowerOfTwoPrepareForRepeat(unsigned int n)
//     Prepare for power of two range so that it is cached, but do not return any
//     random number yet.
// unsigned int RandomIntGeneralRepeat()
//     Repeat random integer function with the general range previously set
//     with RandomIntGeneralPrepareForRepeat().
// void RandomIntGeneralPrepareForRepeat(unsigned int n)
//     Prepare for general range (either a power of two or not) so that it is cached,
//     but do not return any random number yet. Only RandomIntGeneralRepeat() will use
//     the cached information.
// int CalculateLog2(int n)
//     Efficiently calculate floor(log2(n)), valid for n >= 1.
// int CalculatePowerOfTwoShift(int n)
//     Efficiently calculate log2(n), returns - 1 if n is not a power of two.
//
// Floating point core functions:
//
// float RandomFloat(float range)
//     Return a random float from 0 to range (exclusive). A precision of 24 bits
//     is applied within the range.
// float RandomFloatHP(float range)
//     Return a random float from 0 to range (exclusive) with high precision. A
//     greater effective precision can be attained by taking more effort. Since
//     the effective precision of a 32-bit integer converted to float is about
//     29 bits, a higher effective precision can be reached by scaling the
//     integer (using 64-bit integer format). Execution speed depends on whether
//     the hardware has hardware support for 64-bit integer to float conversion.
// float RandomFloatLP(float range)
//     Return a random float from 0 to range (exclusive) with low precision
//     (16 random bits), which is faster because of lighter use of the RNG.
// double RandomDouble(double range)
//     Return a random double from 0 to range (exclusive) with reasonable precision.
//     64 bits of random input bits are used. At least double format precision
//     (53 bits) is applied within the range, provided the range is representable
//     in such precision.
//     Return a random double from 0 to range (exclusive) with low precision
//     (16 random bits), which is faster because of lighter use of the RNG.
// double RandomDoubleHP(double range)
//     Return a random double from 0 to range (exclusive) with very high precision.
//     A higher degree of precision can be achieved when the range is large
//     (much larger than 1.0, so that the return value can have higher effective
//     precision). This functions can be slower because it uses natural
//     exponent and logarithm calculations, but on PC-class CPUs the difference
//     is not large (helped by inlining and thereby allowing the compiler to
//     optimize scheduling by mixing integer and floating point instructions).
// double RandomDoubleLP(float range)
//     Return a random double from 0 to range (exclusive) with low precision.
//     A precision of 32 bits is applied within the range.
// double RandomDoubleVLP(float range)
//     Return a random double from 0 to range (exclusive) with very low precision.
//     A precision of 16 bits is applied within the range.
// float RandomFloatWithinBounds(float min_bound, float max_bound)
//     Return a random float from min_bound to max_bound (exclusive).
// float RandomFloatWithinBoundsHP(float min_bound, float max_bound);
//     Return a random float from min_bound to max_bound (exclusive) with
//     high precision.
// float RandomFloatWithinBoundsLP(float min_bound, float max_bound);
//     Return a random float from min_bound to max_bound (exclusive) with
//     low precision (16 random bits).
// double RandomDoubleWithinBounds(double min_bound, double max_bound);
//     Return a random double from min_bound to max_bound (exclusive).
// double RandomDoubleWithinBoundsHP(double min_bound, double max_bound);
//     Return a random double from min_bound to max_bound (exclusive) with
//     very high precision. For a (much) range that is greater than 1.0,
//     this function achieves higher precision. For ranges below or equal
//     to 1.0, it is the same as the HP function. Because of of rescaling
//     using natural exponent and logarithm operators, this function is
//     relatively slow for ranges above 1.0.
// double RandomDoubleWithinBoundsLP(double min_bound, double max_bound);
//     Return a random double from min_bound to max_bound (exclusive) with
//     low precision (32 random bits).
// double RandomDoubleWithinBoundsVLP(double min_bound, double max_bound);
//     Return a random double from min_bound to max_bound (exclusive) with
//     very low precision (16 random bits).
//
// Miscellaneous random number functions:
//
// void CalculateRandomOrder(unsigned int *order, unsigned int n);
//     Calculate a random permutation of the numbers 0 to (n - 1). The array
//     of integer indices must be preallocated. Random swaps are used while
//     traversing an initially ordered array. The statistical robustness of
//     this function has not been verified.
//    
// More specialized functions for  testing of different strategies:
//
// unsigned int RandomIntEmpirical(unsigned int n)
//     RandomInt() that will use the empirical strategy for ranges that are not
//     a power of two (see below). Detects powers of two. Only available when
//     the empirical strategy is included.
// unsigned int RandomIntRemainder(unsigned int n)
//     RandomInt() that will use the remainder strategy for ranges that are not
//     a power of two. Detects powers of two. Only available if the emperical
//     strategy is included.
// unsigned int RandomIntRemainderDirect(unsigned int n)
//     Trivial RandomInt() variant that directly uses the remainder
//     after division of a 32-bit random integer by the range. This is one of
//     the few RandomInt() version that does not cache a power of two range.
//
// sreCMWCRNG::sreCMWCRNG()
//     Constructor for the default complement-multiply-with-carry RNG
//     using the default state size (8).
// sreCMWCRNG::sreCMWCRNG(int state_size)
//     Constructor for the default complement-multiply-with-carry RNG
//     using the specified state size.

// Configuration.
//
// This module contains a number of features of general optimized random
// number generator class that must be selected/enabled at compile time. This
// primarily applies to the the strategy used for the general random integer
// function (sreRNG::RandomInt(int n)) which has the range as parameter and
// represents common use by certain applications. Other applications may
// almost exclusively use floating point random functions  (which are less
// subject to varying optimization strategies).

// Two strategies for producing random integers from a non-power-of-two range
// are implemented.

// One (SRE_RANDOM_EMPIRICAL) produces random integers of the minimum number
// bits that covers the whole range, and when the value produced is above the
// range, tries again. Although this produces variable execution times, and
// the loop can theoretically use many iterations in a worst-case scenario,
// the chance of an invalid value is at worst 50% (e.g. producing 8-bit
// integers in the range 0-255 when n = 129, range 0 to 128 (inclusive),
// which is just 128, a power of two). In practice, it seems to work well.

// The other strategy (SRE_RANDOM_INT_REMAINDER) is to use the remainder
// (modulo) function, basically and integer divide, to limit a larger number
// of bits to a specific integer range. This technique is subject to bias,
// more so when the number of random bits of the remainder input is not
// sufficiently higher than the number of bits required for the random
// integer range. Integer divide/ remainder is also notoriously slow even
// on modern CPUs. It is disabled by default.

// The following defines the strategy used by default by the general RandomInt(n)
// family of functions. One line must be uncommented.

#define SRE_RANDOM_INT_EMPIRICAL
// #define SRE_RANDOM_INT_REMAINDER

// The following definitions determine whether to include support for either
// of the two strategies. If a strategy was enabled above, it must be included
// here. However, when one of the strategies is included (but not used by
// default), it can be used directly, with either sreRNG::RandomIntEmpirical()
// or sreRNG::RandomIntRemainder(). One or both of these lines must be
// uncommented.
#define SRE_RANDOM_INT_INCLUDE_EMPIRICAL_STRATEGY
// #define SRE_RANDOM_INT_INCLUDE_REMAINDER_STRATEGY

// Two strategies are provided for determining whether an integer range value
// is power of two, and concurrently determining the highest order bit that
// is set in any integer range value (effectively calculating the rounded log2
// of an integer).

// One (SRE_RANDOM_LOG2_LOOKUP_TABLE) uses a 257-byte lookup-table to look up
// log2 and recognize powers of two in 8-bit chunks.

// The other (SRE_RANDOM_CALCULATE_LOG2) strategy uses a fairly large number
// of simple bit manipulations. Despite the fair number of operating terms,
// it is relatively fast on many systems, primarily because the code, being
// devoid of any conditional code, is subject to very high degree of compiler
// optimization (simplication and scheduling) and executes smoothly of modern
// pipelined CPUs.

#define SRE_RANDOM_CALCULATE_LOG2
// #define SRE_RANDOM_LOG2_LOOKUP_TABLE

// The SRE_RANDOM_PREFER_64_BIT_STORAGE option makes the class use
// 64 bit of random bit storage instead 32 bits, when available. This
///option is only effective with gcc when __WORDSIZE is defined
// as 64 (e.g. x64 PC platforms). Theoretically, 32-bit platforms
// that have some degree of a 64-bit machine instruction extension
// (at least for shift, move, and add), such as some modern ARM
// processors, could also benefit; on virtually all platforms, gcc
// can emulate 64-bit integer math as well. To force the use of
// 64 bits of storage regardless of platform, use
// SRE_RANDOM_FORCE_64_BIT_STORAGE.

#define SRE_RANDOM_PREFER_64_BIT_STORAGE
// #define SRE_RANDOM_FORCE_64_BIT_STORAGE

// The SRE_RANDOM_OPTIMIZE_ALIGNMENT_AND_PACKING_WITH_GCC option, when
// gcc/g++ is used, adds storage attributes to optimize alignment of
// and packing of the data fields of the random number generator class
// structure. It also enables alignment of the state array in the
// CMWC random number generator, using posix_memalign. Since the class
// has only a few fields, any effects is probably small, although the
// effect of aligning the state size in the RNG implementation may be
// greater.

#define SRE_RANDOM_OPTIMIZE_ALIGNMENT_AND_PACKING_WITH_GCC

// When SRE_RANDOM_BITS_NEEDED_VALIDATION_CHECK is defined, code is
// inserted to verify that the calculated number of bits needed for
// a particular range is correct. This makes the code slower and
// should only be enabled for debugging.

// #define SRE_RANDOM_BITS_NEEDED_VALIDATION_CHECK

// End of compile-time configuration definitions.

#ifdef SRE_RANDOM_FORCE_64_BIT_STORAGE

#define SRE_STORAGE_SIZE 64

#elifdef __GNUC__

#if (defined(SRE_RANDOM_PREFER_64_BIT_STORAGE) && __WORDSIZE == 64)
#define SRE_STORAGE_SIZE 64
#endif

#endif

#ifndef SRE_STORAGE_SIZE
#define SRE_STORAGE_SIZE 32
#endif

// Define macros to optimize memory usage and alignment in the
// class structure.

#ifndef __GNUC__

#define SRE_ALIGNED(n)
#define SRE_PACKED
#define LINE_SIZE 128

#else

#define SRE_ALIGNED(n) __attribute__ ((aligned(n)))
#define SRE_PACKED __attribute__ ((packed))
// Very rough guess at a cache line size usable for alignment,
#ifdef OPENGL_ES2
#define SRE_LINE_SIZE 64
#else
#define SRE_LINE_SIZE 128
#endif

#endif

#ifdef SRE_RANDOM_BITS_NEEDED_VALIDATION_CHECK
#define VALIDATE_BITS_NEEDED(n, n_bits) ValidateBitsNeeded(n, n_bits)

static void ValidateBitsNeeded(unsigned int n, unsigned int n_bits) {
    unsigned int mask = ((unsigned int)1 << n_bits) - 1;
    if ((n & mask) != (n & (~((unsigned int)1 << n_bits))) ||
    (n != ((unsigned int)1 << n_bits) && (n & (~mask)) != 0))
        printf("Warning: bits needed = %d is not optimal for n = %d.\n",
            n_bits, n);
}

#else
#define VALIDATE_BITS_NEEDED(n, n_bits)
#endif

#ifdef SRE_RANDOM_LOG2_LOOKUP_TABLE
extern const unsigned char sre_internal_random_table[257] SRE_ALIGNED(256);
#endif

class SRE_RANDOM_API  SRE_PACKED sreRNG {
private :
#if SRE_STORAGE_SIZE == 64
    // 64 bits of storage.
    uint64_t storage;
#else
    // 32 bits of storage.
    unsigned int storage;
#endif
    // The last power of two, for which the shift has been cached.
    unsigned int last_power_of_two;
    // The last non-power of two, for which the shift has been cached.
    unsigned int last_general_range;
    // Number of bits in storage (0 to 31/63).
    unsigned char storage_size;
    // The bit shift corresponding to the last power of two (log2(n)).
    unsigned char last_power_of_two_shift;
    // The bit shift corresponding to the last non-power of two (log2(n - 1) + 1).
    unsigned char last_general_range_shift;

    // Constructor and virtual functions.
public :
    sreRNG();
    virtual void Seed(unsigned int seed) = 0;
    // Return a random 32-bit value (32 random bits). Used as the source of
    // random bits by all member functions.
    virtual unsigned int Random32() = 0;

private :
    inline void SetLastPowerOfTwoData(unsigned int n, unsigned int shift) {
        last_power_of_two = n;
        last_power_of_two_shift = shift;
    }
    inline unsigned int GetLastPowerOfTwo() const {
        return last_power_of_two;
    }
    inline int GetLastPowerOfTwoShift() const {
        return last_power_of_two_shift;
    }
    inline void SetLastGeneralRangeData(unsigned int n, unsigned int shift) {
        last_general_range = n;
        last_general_range_shift = shift;
    }
    inline unsigned int GetLastGeneralRange() const {
        return last_general_range;
    }
    inline int GetLastGeneralRangeShift() const {
        return last_general_range_shift;
    }

#ifdef SRE_RANDOM_LOG2_LOOKUP_TABLE
    // The following lookup table functions do not actually belong in the
    // class itself, since they do not depend on it.
    // Returns floor(log2(n) from table. Valid for 1 <= n <= 256.
    // Returns - 1 for n = 0.
    inline int GetLog2(unsigned int n) const {
        return (sre_internal_random_table[n] & 0xF) - 1;
    }
    // Return floor(log2(n)) + 1 (rounded down) from table. Valid for n <= 256.
    // Returns 0 for n = 0.
    inline int GetLog2Plus1(unsigned int n) const {
        return sre_internal_random_table[n] & 0xF;
    }
    // Look up the power of two table value for n, looking at nu_bits from n
    // starting from shift_start.
    inline unsigned int GetTableValue(unsigned int n, unsigned int shift_start,
    unsigned int nu_bits) const {
        unsigned int mask = ((unsigned int)1 << nu_bits) - 1;
        return sre_internal_random_table[(n & (mask << shift_start)) >> shift_start];
    }
    // Get the bit shift corresponding to a looked up table value (returns 0 for an
    // original table index of 1, 1 for an index of 2, etc, representing the number
    // of bits of left shift from bit 0). Equal to floor(log2(n)) - shift_start for
    // the n and shift_start arguments previously provided for GetTableValue().
    inline unsigned int TableValueGetShift(int value) const {
        return (value & 0xF) - 1;
    }
    // Return the offset parameter if the zero flag is not set, zero otherwise.
    // offset must be <= 31.
    inline unsigned int TableValueGetOffsetIfNoZeroFlag(unsigned int value,
    unsigned int offset) const {
        // Use an expression yielding offset if the zero flag is not set, zero
        // otherwise.
        unsigned char zero_flag = value & 0x40;
        // When zero_flag = 0, the mask evaluates to 0xFF;
        // when zero_flag = 0x40, the mask evaluates to 0.
        return offset & ((zero_flag >> 6) - 1);
    }
    // Get the bit shift corresponding to the original table index with the given
    // starting bit (corresponding to the shift_start value from GetTableValue().
    // If no bits were set within the defined range of bits, zero is returned;
    // otherwise the adjusted shift is returned which is equal to starting_bit +
    // the shift associated with the table value and its bit range.
    inline unsigned int TableValueGetAdjustedShift(unsigned int value,
    unsigned int starting_bit) const {
        return TableValueGetOffsetIfNoZeroFlag(value, starting_bit) +
            TableValueGetShift(value);
    }
    // Whether the original table index was a power of two.
    inline bool TableValueIsPowerOfTwo(unsigned int value) const {
        return (value & 0xC0) == 0;
    }
    // Whether the original table index was zero.
    inline bool TableValueZeroFlag(unsigned int value) const {
        return (value & 0x40) != 0;
    }
    // Return log2(n) + 1 if n is a power of two in the range (1 << shift_start) to
    // (1 << (shift_start + bits - 1)). When n is not a power of two,
    // the function returns log2(n) + 1 if n is in the range
    // [(1 << shift_start) + 1, (1 << (shift_start + nu_bits - 1))>.
    // Otherwise (no bits within the specified bit range are set), the function
    // returns zero.
    // nu_bits must be in the range [1, 8].
    inline int GetLog2PlusOneInRange(unsigned int n, unsigned int shift_start,
    unsigned int nu_bits) const {
        unsigned int value = GetTableValue(n, shift_start, nu_bits);
        return (value & 0xF) + TableValueGetOffsetIfNoZeroFlag(value, shift_start);
    }
    unsigned int GetBitsNeededLookupTable(unsigned int n);
    unsigned int GetBitsNeededLookupTableMax256(unsigned int n);
    unsigned int GetBitsNeededLookupTableMax65536(unsigned int n);
#endif
#ifdef SRE_RANDOM_CALCULATE_LOG2
public :
    // Calculate floor(log2(n))). For a power of two, this is equivalent to the
    // number of bits needed to represent the range 0 to n - 1. For a non-power-of-two,
    // the return value is one less than the number of bits needed to represent
    // the range 0 to n - 1.
    inline unsigned int CalculateLog2(unsigned int n) const {
        // Set shift to 16 if bits 15-31 are non-zero, zero otherwise.
        unsigned int shift = (((n >> 16) + 0xFFFF) & 0x10000) >> 12;
        unsigned int bits = n >> shift;
        // Add 8 to shift if bits 8-15 of the highest non-zero half-word found previously
        // are non-zero.
        unsigned char byte_shift = (((bits >> 8) + 0xFF) & 0x100) >> 5;
        shift += byte_shift;
        bits >>= byte_shift;
        // Add 4 to shift if bits 4-7 of the highest non-zero byte found previously
        // are non-zero.
        unsigned char nibble_shift = (((bits >> 4) + 0xF) & 0x10) >> 2;
        shift += nibble_shift;
        bits >>= nibble_shift;
        // Add 2 to shift if bits 2-3 of the highest non-zero nibble found previously
        // are non-zero.
        unsigned char pair_shift = (((bits >> 2) + 0x3) & 0x4) >> 1;
        shift += pair_shift;
        bits >>= pair_shift;
        // Add 1 to shift if bit 1 of the highest non-zero pair found previously
        // is non-zero.
        shift += bits >> 1;
        return shift;
    }
private :
    // Calculate number of bits needed for an integer range of n (log2(n - 1) + 1).
    inline unsigned int CalculateBitsNeeded(unsigned int n) const {
        unsigned int shift = CalculateLog2(n);
        // If n is not a power of two, one more bit is needed.
        // Rely on the fact that bit 31 will be set when subtracting n from 2 ^ shift
        // and n is not power of two.
        shift += ((1 << shift) - n) >> 31;
        VALIDATE_BITS_NEEDED(n, shift);
	return shift;
    }
    // Calculate floor(log2(n)) when n is guaranteed to be <= 256, so that the
    // return value will be <= 8. Undefined for n > 256.
    inline unsigned int CalculateLog2Max256(unsigned int n) const {
        // Set shift to 4 if bits 4-7 of the highest non-zero byte found previously
        // are non-zero.
        unsigned int shift = (((n >> 4) + 0xF) & 0x10) >> 2;
        unsigned int bits = n >> shift;
        // Add 2 to shift if bits 2-3 of the highest non-zero nibble found previously
        // are non-zero.
        unsigned char pair_shift = (((bits >> 2) + 0x3) & 0x4) >> 1;
        shift += pair_shift;
        bits >>= pair_shift;
        // Add 1 to shift if bit 1 of the highest non-zero pair found previously
        // is non-zero.
        shift += bits >> 1;
        // When n = 2^16, set shift to 16 (shift will still be zero).
        shift += (n & 0x10000) >> 12;
        return shift;
    }
    // Calculate number of bits needed for an integer range of n (log2(n - 1) + 1),
    // when n <= 256.
    inline unsigned int CalculateBitsNeededMax256(unsigned int n) const {
        unsigned int shift = CalculateLog2Max256(n);
        // If n is not a power of two, one more bit is needed.
        // Rely on the fact that bit 31 will be set when subtracting n from 2 ^ shift
        // and n is not power of two.
        shift += ((1 << shift) - n) >> 31;
        VALIDATE_BITS_NEEDED(n, shift);
	return shift;
    }
    // Calculate floor(log2(n)) when n is guaranteed to be <= 2^16, so that the
    // return value will be <= 16. Undefined for n > 2^16.
    inline unsigned int CalculateLog2Max65536(unsigned int n) const {
        // Set shift to 8 if bits 7-15 are non-zero.
        unsigned int shift = (((n >> 8) + 0xFF) & 0x100) >> 5;
        unsigned bits = n >> shift;
        // Add 4 to shift if bits 4-7 of the highest non-zero byte found previously
        // are non-zero.
        unsigned char nibble_shift = (((bits >> 4) + 0xF) & 0x10) >> 2;
        shift += nibble_shift;
        bits >>= nibble_shift;
        // Add 2 to shift if bits 2-3 of the highest non-zero nibble found previously
        // are non-zero.
        unsigned char pair_shift = (((bits >> 2) + 0x3) & 0x4) >> 1;
        shift += pair_shift;
        bits >>= pair_shift;
        // Add 1 to shift if bit 1 of the highest non-zero pair found previously
        // is non-zero.
        shift += bits >> 1;
        // When n = 256, set shift to 8 (shift will still be zero).
        shift += (n & 0x100) >> 5;
        return shift;
    }
    // Calculate number of bits needed for an integer range of n (log2(n - 1) + 1),
    // when n <= 65536.
    inline unsigned int CalculateBitsNeededMax65536(unsigned int n) const {
        unsigned int shift = CalculateLog2Max65536(n);
        // If n is not a power of two, one more bit is needed.
        // Rely on the fact that bit 31 will be set when subtracting n from 2 ^ shift
        // and n is not power of two.
        shift += ((1 << shift) - n) >> 31;
        VALIDATE_BITS_NEEDED(n, shift);
	return shift;
    }
#endif

    // Helper functions.
    unsigned int RandomBitsNeedStorage(unsigned int n_bits);
    // Random integer helper functions.
#ifdef SRE_RANDOM_INT_INCLUDE_REMAINDER_STRATEGY
    // When SRE_RANDOM_INT_INCLUDE_REMAINDER_STRATEGY is enabled, the following
    // private helper functions may be utilized:
    // The random distribution of RandomIntSlowBits() has a small controlled bias for
    // small non-powers-of-two, and a larger one for values approaching INT_MAX.
    // range must be >= 1. This implementation of RandomInt optimized for the case
    // when the underlying random bits function (Random32) is relatively expensive.
    // Few bits are wasted. This is currently the default.
    unsigned int RandomIntRemainderSlowBits(unsigned int range);
    // Version of RandomIntRemaindreSlowBits optimized for range <= 256. It is
    // provided inline because of acceptable size.
    unsigned int RandomIntRemainderSlowBitsMax256(unsigned int n) {
        // Fast path to most common occurence of repeating power of two.
        if (n == GetLastPowerOfTwo()) {
            return RandomBits(GetLastPowerOfTwoShift());
        }
#ifdef SRE_RANDOM_LOG2_LOOKUP_TABLE
        // Function returns log2(n) + 1 in bits 0-3, bit 7 is set if n is not
        // a power of two, is bit 6 set if n is zero.
        unsigned int value = GetTableValue(n, 0, 9);
        // shift will be equivalent to floor(log2(n)).
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
#else
        // SRE_RANDOM_CALCULATE_LOG2 is defined.
        unsigned int shift = CalculateBitsNeededMax256(n);
        // Detects powers of two.
        if (n == ((unsigned int)1 << shift)) {
            SetLastPowerOfTwoData(n, shift);
            return RandomBits(shift);
        }
        return (unsigned short)RandomBits(shift + 8) % (unsigned char)n;
#endif
    }
    // Version of RandomIntRemainderSlowBits optimized for range <= 65536.
    unsigned int RandomIntRemainderSlowBitsMax65536(unsigned int range);

#if 0
    // Variant of RandomInt that is optimized for the case when the underlying
    // random bits function (Random32) is relatively fast. More bits from Random32()
    // may be discarded. This function is currently undefined/unused.
    unsigned int RandomIntRemainderFastBits(unsigned int range);
#endif
#endif

    // Start of the public member functions for random number generation.
public :
    // Seed the random number generator using the system time.
    void SeedWithTimer();
    // Get n random bits. This version works for 0 <= n <= 32.
    inline unsigned int RandomBits(unsigned int n_bits) {
        if (storage_size < n_bits)
            return RandomBitsNeedStorage(n_bits);
        unsigned int mask = ((unsigned int)1 << n_bits) - 1;
        unsigned int r = storage & mask;
        storage >>= n_bits;
        storage_size -= n_bits;
        return r;
    }

    // RandomInt(n): Return random integer from the range 0 to n - 1
#ifdef SRE_RANDOM_INT_INCLUDE_REMAINDER_STRATEGY
    // RandomInt() that uses the remainder method if necessary. Powers of two
    // are optimized.
    inline unsigned int RandomIntRemainder(unsigned int n) {
        return RandomIntRemainderSlowBits(n);
    }
    inline unsigned int RandomIntRemainderMax256(unsigned int n) {
        return RandomIntRemainderSlowBitsMax256(n);
    }
    inline unsigned int RandomIntRemainderMax65536(unsigned int n) {
        return RandomIntRemainderSlowBitsMax65536(n);
    }
#endif
#ifdef SRE_RANDOM_INT_INCLUDE_EMPIRICAL_STRATEGY
    // Empirical RandomInt() that determines the necessary number of bits
    // and keep trying when the value is above the range. Powers of two are
    // optimized.
    inline unsigned int RandomIntEmpirical(unsigned int n) {
        if (n == GetLastPowerOfTwo()) {
            unsigned int shift = GetLastPowerOfTwoShift();
            // Repeated bit sizes >= 20 trigger a lot of storage refills, so it is
            // faster to use Random32() and discard some bits.
            if (shift >= 20) {
                return Random32() & (n - 1);
            }
            return RandomBits(shift);
        }
        unsigned int shift;
        {
#ifdef SRE_RANDOM_CALCULATE_LOG2
            shift = CalculateBitsNeeded(n);
#else   // SRE_RANDOM_LOG2_LOOKUP_TABLE
            shift = GetBitsNeededLookupTable(n);
#endif
            if (((unsigned int)1 << shift) == n) {
                SetLastPowerOfTwoData(n, shift);
                return RandomBits(shift);
            }
        }
        for (;;) {
            // Keep trying until the value is within the range.
            unsigned int r = RandomBits(shift);
            if (r < n)
                return r;
        }
    }
    inline unsigned int RandomIntEmpiricalMax256(unsigned int n) {
        if (n == GetLastPowerOfTwo()) {
            unsigned int shift = GetLastPowerOfTwoShift();
            return RandomBits(shift);
        }
        unsigned int shift;
        {
#ifdef SRE_RANDOM_CALCULATE_LOG2
            shift = CalculateBitsNeededMax256(n);
#else   // SRE_RANDOM_LOG2_LOOKUP_TABLE
            shift = GetBitsNeededLookupTable(n);
#endif
            if (((unsigned int)1 << shift) == n) {
                SetLastPowerOfTwoData(n, shift);
                return RandomBits(shift);
            }
        }
        for (;;) {
            // Keep trying until the value is within the range.
            unsigned int r = RandomBits(shift);
            if (r < n)
                return r;
        }
    }
    inline unsigned int RandomIntEmpiricalMax65536(unsigned int n) {
        if (n == GetLastPowerOfTwo()) {
            unsigned int shift = GetLastPowerOfTwoShift();
            return RandomBits(shift);
        }
        unsigned int shift;
        {
#ifdef SRE_RANDOM_CALCULATE_LOG2
            shift = CalculateBitsNeededMax65536(n);
#else   // SRE_RANDOM_LOG2_LOOKUP_TABLE
            shift = GetBitsNeededLookupTable(n);
#endif
            if (((unsigned int)1 << shift) == n) {
                SetLastPowerOfTwoData(n, shift);
                return RandomBits(shift);
            }
        }
        for (;;) {
            // Keep trying until the value is within the range.
            unsigned int r = RandomBits(shift);
            if (r < n)
                return r;
        }
    }
#endif
    // Trivial RandomInt() variant that directly uses the remainder
    // after division of a 32-bit random integer by the range.
    unsigned int RandomIntRemainderDirect(unsigned int n) {
        return Random32() % n;
    }
    // Generic RandomInt, uses the compiled-in selected strategy (empirical or
    // remainder).
    inline unsigned int RandomInt(unsigned int n) {
#ifdef SRE_RANDOM_INT_EMPIRICAL
        return RandomIntEmpirical(n);
#else
        return RandomIntRemainder(n);
#endif
    }
    // Version of RandomInt, optimized for n <= 256.
    unsigned int RandomIntMax256(unsigned int n) {
#ifdef SRE_RANDOM_INT_EMPIRICAL
        return RandomIntEmpiricalMax256(n);
#else
        return RandomIntRemainderMax256(n);
#endif
    }
    // Version of RandomInt, optimized for n <= 65536.
    unsigned int RandomIntMax65536(unsigned int n) {
#ifdef SRE_RANDOM_INT_EMPIRICAL
        return RandomIntEmpiricalMax65536(n);
#else
        return RandomIntRemainderMax65536(n);
#endif
    }

    // Any power of two range 1 <= n <= 256.
    inline unsigned int RandomIntPowerOfTwoMax256(unsigned int n) {
#ifdef SRE_RANDOM_LOG2_LOOKUP_TABLE
        // A table lookup could be avoided if GetLastPowerOfTwo() == n,
        // but the resulting introduction of a conditional probably results
        // in slower code. We do however cache the power of two and its shift
        // for possible subsequent use by RandomInt() or RandomIntPowerOfTwoRepeat().
        unsigned int value = GetTableValue(n, 0, 9);
        unsigned int shift = TableValueGetShift(value);
        SetLastPowerOfTwoData(n, shift);
#else
        // Use the calculation method.
        unsigned int shift = CalculateLog2Max256(n);
#endif
        return RandomBits(shift);
    }
    // Any power of two range 1 <= n <= 2^16.
    inline unsigned int RandomIntPowerOfTwoMax65536(unsigned int n) {
#ifdef SRE_RANDOM_LOG2_LOOKUP_TABLE
        unsigned int shift =
            // Get 0 for n = 1, 1 for n = 2 etc. to 8 for n = 256.
            TableValueGetShift(GetTableValue(n, 0, 9)) +
            // Get 0 for n < 512, 9 for 512, 10 for 1024 etc. to
            // 17 for 2^17.
            TableValueGetAdjustedShift(GetTableValue(n, 9, 9), 9);
        SetLastPowerOfTwoData(n, shift);
#else
        // Use the calculation method.
        unsigned int shift = CalculateLog2Max65536(n);
#endif
        return RandomBits(shift);
    }
    // Random integer for general power-of-two range from 1 up to (1 << 30).
    inline unsigned int RandomIntPowerOfTwo(unsigned int n) {
        if (n == GetLastPowerOfTwo()) {
            return RandomBits(GetLastPowerOfTwoShift());
        }
        unsigned int shift;
#ifdef SRE_RANDOM_LOG2_LOOKUP_TABLE
        shift = GetBitsNeededLookupTable(n);
#else
        // Use the calculation method.
        shift = CalculateLog2(n);
#endif
        SetLastPowerOfTwoData(n, shift);
        return RandomBits(shift);
    }
    // Return random integer in any power of two range with specified shift
    // (log2(n)). Different from RandomBits() because it caches the power of
    // two range value.
    inline unsigned int RandomIntPowerOfTwoWithShift(unsigned int shift) {
       SetLastPowerOfTwoData(1 << shift, shift);
       return RandomBits(shift);
    }
    // Repeat random integer function with the previously used power of two range
    // (with any integer function).
    inline unsigned int RandomIntPowerOfTwoRepeat() {
        return RandomBits(GetLastPowerOfTwoShift());
    }
    // Prepare for power of two range so that it is cached, but do not return any
    // random number yet.
    inline void RandomIntPowerOfTwoPrepareForRepeat(unsigned int n) {
        if (n == GetLastPowerOfTwo())
		return;
	int shift = CalculateLog2(n);
        SetLastPowerOfTwoData(n, shift);
    }
    // Repeat random integer function with the general range previously set
    // with RandomIntGeneralPrepareForRepeat().
    inline unsigned int RandomIntGeneralRepeat() {
        for (;;) {
            // Keep trying until the value is within the range.
            unsigned int r = RandomBits(GetLastGeneralRangeShift());
            if (r < GetLastGeneralRange())
                return r;
        }
    }
    // Prepare for general range (either a power of two or not) so that it is cached,
    // but do not return any random number yet.
    inline void RandomIntGeneralPrepareForRepeat(unsigned int n) {
        if (n == GetLastGeneralRange())
            return;
        int shift = CalculateBitsNeeded(n);
        SetLastGeneralRangeData(n, shift);
    }
    // Efficiently calculate log2(n), returns - 1 if n is not a power of two.
    inline int CalculatePowerOfTwoShift(unsigned int n) {
	unsigned int shift = CalculateLog2(n);
	if (((unsigned int)1 << shift) == n)
		return shift;
	return - 1;
    }

    // Floating point functions (all inline).
    // IEEE float has 24 bits of precision for a range that differs by a factor
    // of two. However for larger ranges (such as the one that applies here with
    // a minimum of zero) a larger effective precision can be attained.
    // Return a random float from 0 to range (exclusive).
    float RandomFloat(float range) {
        return (float)Random32() * (1.0f / powf(2.0f, 32)) * range;
    }
    // Return a random float from 0 to range (exclusive) with low precision
    // (16 random bits), which is faster because of lighter use of the RNG.
    float RandomFloatLP(float range) {
        return (float)(unsigned short)RandomBits(16) * (1.0f / powf(2.0f, 16)) * range;
    }
    // Return a random float from 0 to range (exclusive) with high precision. A
    // greater effective precision can be attained by taking more effort. Since
    // the effective precision of a 32-bit integer converted to float is about
    // 29 bits, a higher effective precision can be reached by scaling the
    // integer (using 64-bit integer format). Execution speed depends on whether
    // the hardware has hardware support for 64-bit integer to float conversion.
    float RandomFloatHP(float range) {
        return (float)((uint64_t)Random32() << 32) * (1.0f / powf(2.0f, 64)) * range;
    }
    // Return a random double from 0 to range (exclusive) with high precision.
    // A high precision is (minimum probably around 53 bits) is applied within
    // the range.
    double RandomDouble(double range) {
        // Scaling the 2^32 integers to [0, 1) (which has good precision in the
        // double format) maintains precision when adding up the lower and higher
        // order components.
        return ((double)Random32() * (1.0d / pow(2.0d, 32)) +
            (double)Random32() * (1.0d / pow(2.0d, 64))) * range;
    }
    // Return a random double from 0 to range (exclusive) with very high precision.
    // A higher degree of precision can be achieved when the range is large
    // (much larger than 1.0, so that the return value can have higher effective
    // precision).
    double RandomDoubleHP(double range) {
        if (range <= 1.00001d)
            // When range <= 1.0, the standard function is already
            // optimal.
            return RandomDouble(range);
        const double high_value = DBL_MAX;
        // Use the identity exp(x + y) = exp(x) * exp(y).
        // Scale the 32 bit random integers r0 and r1 so that
        // 0 <= exp(scale * r0 * exp(scale * r1 * 2^32) <= high_value,
        // where high_value is near the greatest representable double value.
        // That is, exp(scale * r0 + scale * r1 * 2^32) <= high_value,
        // <-> scale * r0 + scale * r1 * 2^32 <= log(high_value)
        // so that scale = log(high_value) / pow(2.0d, 32).
        const double scale_factor0 = log(high_value) / pow(2.0d, 32);
        const double scale_factor1 = log(high_value) / pow(2.0d, 64);
        return log(
            exp((double)Random32() * scale_factor0) *
            exp((double)Random32() * scale_factor1)
            ) * (range / log(high_value));
    }
    // Return a random double from 0 to range (exclusive) with low precision.
    // 32 random bits are used.
    double RandomDoubleLP(double range) {
        return (double)Random32() * (1.0f / pow(2.0d, 32)) * range;
    }
    // Return a random double from 0 to range (exclusive) with low precision
    // (16 random bits), which is faster because of lighter use of the RNG.
    float RandomDoubleVLP(float range) {
        return (double)(unsigned short)RandomBits(16) * (1.0d / powf(2.0d, 16)) * range;
    }
    // Return a random float from min_bound to max_bound (exclusive).
    float RandomFloatWithinBounds(float min_bound, float max_bound) {
         return min_bound + RandomFloat(max_bound - min_bound);
    }
    // Return a random float from min_bound to max_bound (exclusive) with high precision.
    float RandomFloatWithinBoundsHP(float min_bound, float max_bound) {
        return min_bound + RandomFloatHP(max_bound - min_bound);
    }
    // Return a random float from min_bound to max_bound (exclusive) with low recision.
    float RandomFloatWithinBoundsLP(float min_bound, float max_bound) {
        return min_bound + RandomFloatLP(max_bound - min_bound);
    }
    // Return a random double from min_bound to max_bound (exclusive).
    double RandomDoubleWithinBounds(double min_bound, double max_bound) {
        return min_bound + RandomDouble(max_bound - min_bound);
    }
    // Return a random double from min_bound to max_bound (exclusive) with high precision.
    double RandomDoubleWithinBoundsHP(double min_bound, double max_bound) {
        return min_bound + RandomDoubleHP(max_bound - min_bound);
    }
    // Return a random double from min_bound to max_bound (exclusive) with low precision.
    double RandomDoubleWithinBoundsLP(double min_bound, double max_bound) {
        return min_bound + RandomDoubleLP(max_bound - min_bound);
    }
    // Return a random double from min_bound to max_bound (exclusive) with very low
    // precision.
    double RandomDoubleWithinBoundsVLP(double min_bound, double max_bound) {
        return min_bound + RandomDoubleVLP(max_bound - min_bound);
    }

    void CalculateRandomOrder(unsigned int *order, unsigned int n);
}  SRE_ALIGNED(64);

// Default RNG implementation (Complementary-multiply-with-carry).

// State size for default CMWC random number generator must be a power of
// two. A small state size does not seem to significantly affect the
// statistical qualities of the random number generator. For scientific
// purposes it it probably better to use a larger state size. The state
// size is configurable at run time.
#define SRE_RANDOM_CMWC_RNG_DEFAULT_STATE_SIZE 8

class SRE_RANDOM_API sreCMWCRNG : public sreRNG {
private :
    unsigned int *Q;
    unsigned int c;
    int _index;
    int state_size;

    void Initialize(unsigned int state_size);

public :
    sreCMWCRNG();
    sreCMWCRNG(unsigned int state_size);
    ~sreCMWCRNG();
    // Virtual functions defined in the base class.
    void Seed(unsigned int);
    unsigned int Random32();
};

#endif

