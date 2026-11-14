// balatro_rng.hpp — Balatro's RNG stack, ported to C++.
//
// Layers (see project overview doc, "RNG architecture"):
//   1. pseudohash(str)            -> chaotic string->double hash
//   2. per-key LCG node streams   -> pseudoseed(key)
//   3. LuaJIT math.random         -> Tausworthe LFSR (LuaRandom)
//
// LuaRandom port derived from Immolate by SpectralPack
//   https://github.com/SpectralPack/Immolate  (lib/util.cl)
//   License: CC BY-NC-SA 4.0 — this file inherits that license.
//   Non-commercial use only. Attribution retained per license.
// pseudohash / pseudoseed logic transcribed from Balatro's own
// functions (localthunk), via public analyses of misc_functions.lua.
//
// Verified bit-exact against LuaJIT 2.1 running Balatro's verbatim
// Lua functions (see rng_selftest.cpp / vectors.txt).

#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

// ---------- Layer 3: LuaJIT's math.random (Tausworthe LFSR) ----------
struct LuaRandom {
    std::array<uint64_t, 4> state{};

    // math.randomseed(d)
    explicit LuaRandom(double d);

    double random();                      // math.random()        -> [0,1)
    int    randint(int lo, int hi);       // math.random(lo, hi)  -> [lo,hi]

private:
    uint64_t next_uint();                 // one LFSR step, raw 64 bits
};

// ---------- Layer 1 ----------
double pseudohash(const std::string& s);

// ---------- Abstract interface the whole engine talks to ----------
struct RNG {
    virtual double random(const std::string& key) = 0;                 // [0,1)
    virtual int    randint(const std::string& key, int lo, int hi) = 0; // [lo,hi]
    // Fisher–Yates over indices, matching Balatro's pseudoshuffle
    // (seed once from key, then repeated draws from the same generator).
    virtual void   shuffle(const std::string& key, std::vector<int>& v) = 0;
    // Deep copy of the full RNG state, for game cloning (search/rollouts).
    virtual std::unique_ptr<RNG> clone() const = 0;
    virtual ~RNG() = default;
};

// ---------- Layers 1+2+3 assembled: the real thing ----------
class BalatroRNG : public RNG {
public:
    explicit BalatroRNG(std::string seed8);

    double random(const std::string& key) override;
    int    randint(const std::string& key, int lo, int hi) override;
    void   shuffle(const std::string& key, std::vector<int>& v) override;

    // pseudoseed(key): advance the key's node, return value used to seed
    // LuaRandom. Exposed for tests and for multi-draw patterns.
    double pseudoseed(const std::string& key);

    const std::string& seed_string() const { return seed_; }

    std::unique_ptr<RNG> clone() const override {
        return std::make_unique<BalatroRNG>(*this);   // members all copyable
    }

    // Roll a random 8-char seed the way an unseeded run would.
    static std::string random_seed_string(std::mt19937_64& g);

private:
    std::string seed_;
    double hashed_seed_;
    std::unordered_map<std::string, double> nodes_;
};

// ---------- Phase-1 backend: right distributions, PER-KEY STREAMS ----------
// CRITICAL: every key gets its own independent stream, exactly like the real
// game's pseudoseed nodes. A single global stream would mean any stylistic
// choice (which card to play, an extra discard) shifts every later roll —
// so two agents on the "same seed" would face different shops from ante 2
// onward, destroying common-random-numbers comparability. Shop jokers, tags
// and vouchers MUST be isolated from play decisions.
class MersenneRNG : public RNG {
public:
    explicit MersenneRNG(uint64_t seed) : seed_(seed) {}

    double random(const std::string& key) override {
        return std::uniform_real_distribution<double>(0.0, 1.0)(stream(key));
    }
    int randint(const std::string& key, int lo, int hi) override {
        return std::uniform_int_distribution<int>(lo, hi)(stream(key));
    }
    void shuffle(const std::string& key, std::vector<int>& v) override {
        auto& g = stream(key);
        for (int i = (int)v.size() - 1; i >= 1; --i) {
            int j = std::uniform_int_distribution<int>(0, i)(g);
            std::swap(v[i], v[j]);
        }
    }
    std::unique_ptr<RNG> clone() const override {
        return std::make_unique<MersenneRNG>(*this);
    }
private:
    uint64_t seed_;
    std::unordered_map<std::string, std::mt19937_64> streams_;
    std::mt19937_64& stream(const std::string& key) {
        auto it = streams_.find(key);
        if (it == streams_.end()) {
            // splitmix-style mix of the run seed and the key hash
            uint64_t h = std::hash<std::string>{}(key) ^ (seed_ * 0x9E3779B97F4A7C15ull);
            h ^= h >> 30; h *= 0xBF58476D1CE4E5B9ull;
            h ^= h >> 27; h *= 0x94D049BB133111EBull;
            h ^= h >> 31;
            it = streams_.emplace(key, std::mt19937_64(h)).first;
        }
        return it->second;
    }
};
