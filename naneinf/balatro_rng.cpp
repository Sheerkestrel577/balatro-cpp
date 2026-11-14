// balatro_rng.cpp — see header for provenance & license (CC BY-NC-SA 4.0).
#include "balatro_rng.hpp"

#include <cmath>

// MinGW/MSVC don't define M_PI without _USE_MATH_DEFINES; define our own so
// the port stays portable. Value is bit-identical to glibc's M_PI.
#ifndef BALATRO_PI
#define BALATRO_PI 3.14159265358979323846
#endif
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ===================== helpers =====================

// Lua's `x % 1` is a true mathematical modulo (result in [0,1) for the
// values that occur here). For finite x: x - floor(x).
static inline double fract(double x) { return x - std::floor(x); }

// Lua: tonumber(string.format("%.13f", x))
// Reproduced literally — round-tripping through the same printf-style
// decimal formatting guarantees identical rounding to the game.
static inline double round13(double x) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.13f", x);
    return std::strtod(buf, nullptr);
}

static inline uint64_t double_bits(double d) {
    uint64_t u;
    std::memcpy(&u, &d, sizeof u);
    return u;
}
static inline double bits_double(uint64_t u) {
    double d;
    std::memcpy(&d, &u, sizeof d);
    return d;
}

// ===================== Layer 1: pseudohash =====================

double pseudohash(const std::string& s) {
    double num = 1.0;
    for (int i = (int)s.size() - 1; i >= 0; --i) {
        num = fract(1.1239285023 / num * (unsigned char)s[i] * BALATRO_PI
                    + BALATRO_PI * (double)(i + 1));
    }
    return num;
}

// ===================== Layer 3: LuaRandom =====================
// Tausworthe LFSR, 4 x 64-bit registers. Port of Immolate lib/util.cl,
// which matches LuaJIT's math.random implementation.

uint64_t LuaRandom::next_uint() {
    uint64_t r = 0, z;
    z = state[0];
    z = (((z << 31) ^ z) >> 45) ^ ((z & (UINT64_MAX << 1)) << 18);
    r ^= z; state[0] = z;
    z = state[1];
    z = (((z << 19) ^ z) >> 30) ^ ((z & (UINT64_MAX << 6)) << 28);
    r ^= z; state[1] = z;
    z = state[2];
    z = (((z << 24) ^ z) >> 48) ^ ((z & (UINT64_MAX << 9)) << 7);
    r ^= z; state[2] = z;
    z = state[3];
    z = (((z << 21) ^ z) >> 39) ^ ((z & (UINT64_MAX << 17)) << 8);
    r ^= z; state[3] = z;
    return r;
}

LuaRandom::LuaRandom(double d) {
    uint32_t r = 0x11090601u;
    for (int i = 0; i < 4; ++i) {
        uint64_t m = 1ull << (r & 255u);
        r >>= 8;
        // Two separate statements on purpose — matches the Lua/Immolate
        // rounding behavior (do NOT merge into d = d*PI + E).
        d = d * 3.14159265358979323846;
        d = d + 2.7182818284590452354;
        uint64_t u = double_bits(d);
        if (u < m) u += m;
        state[i] = u;
    }
    for (int i = 0; i < 10; ++i) next_uint();
}

double LuaRandom::random() {
    // Stuff 52 random mantissa bits into a double in [1,2), subtract 1.
    uint64_t u = (next_uint() & 0xFFFFFFFFFFFFFull) | 0x3FF0000000000000ull;
    return bits_double(u) - 1.0;
}

int LuaRandom::randint(int lo, int hi) {
    return (int)(random() * (double)(hi - lo + 1)) + lo;
}

// ===================== Layer 2 + assembly: BalatroRNG =====================

BalatroRNG::BalatroRNG(std::string seed8)
    : seed_(std::move(seed8)), hashed_seed_(pseudohash(seed_)) {}

double BalatroRNG::pseudoseed(const std::string& key) {
    auto it = nodes_.find(key);
    if (it == nodes_.end())
        it = nodes_.emplace(key, pseudohash(key + seed_)).first;
    it->second = std::fabs(
        round13(fract(2.134453429141 + it->second * 1.72431234)));
    return (it->second + hashed_seed_) / 2.0;
}

double BalatroRNG::random(const std::string& key) {
    return LuaRandom(pseudoseed(key)).random();
}

int BalatroRNG::randint(const std::string& key, int lo, int hi) {
    return LuaRandom(pseudoseed(key)).randint(lo, hi);
}

void BalatroRNG::shuffle(const std::string& key, std::vector<int>& v) {
    // pseudoshuffle: seed ONCE, then repeated math.random(i) draws.
    LuaRandom lr(pseudoseed(key));
    for (int i = (int)v.size() - 1; i >= 1; --i) {
        int j = lr.randint(1, i + 1) - 1;   // Lua's 1-based math.random(i)
        std::swap(v[i], v[j]);
    }
}

static const char SEEDCHARS[] = "123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"; // 35

std::string BalatroRNG::random_seed_string(std::mt19937_64& g) {
    std::string s(8, '1');
    std::uniform_int_distribution<int> d(0, 34);
    for (auto& c : s) c = SEEDCHARS[d(g)];
    return s;
}
