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

// Random number generator classes, implemented as an independent library.

class SRE_API sreRNG {
public :
    unsigned int storage;
    int storage_size;
    int last_random_n_power_of_2;
    int last_random_n_power_of_2_bit_count;

    sreRNG();
    virtual void Seed(unsigned int seed) = 0;
    virtual unsigned int Random32() = 0;
    void SeedWithTimer();
    int RandomBit();
    int Random8();
    int Random16();
    int RandomBits(int n);
    int RandomInt(int range);
    float RandomFloat(float range);
    double RandomDouble(double range);
    float RandomWithinBounds(float min_bound, float max_bound);
    double RandomWithinBounds(double min_bound, double max_bound);
    void CalculateRandomOrder(int *order, int n);
};

// State size for default random number generator must be a power of two.
#define SRE_DEFAULT_RNG_STATE_SIZE 64

class SRE_API sreCMWCRNG : public sreRNG {
private :
    unsigned int *Q;
    unsigned int c;
    int _index;
    int state_size;
public :
    sreCMWCRNG();
    sreCMWCRNG(int state_size);
    ~sreCMWCRNG();
    void Seed(unsigned int);
    unsigned int Random32();
};

// Get the default RNG allocated used internally by the library.
// The standard default RNG is always initialized and available at program start.
SRE_API sreRNG *sreGetDefaultRNG();
// Set the default RNG to be used internally by the library.
// Can be used to replace the standard default RNG.
// A value of NULL reselects the standard (always initialized) default RNG.
SRE_API void sreSetDefaultRNG(sreRNG *rng);
