// bignum.hpp — mantissa+exponent big number (overview doc §4).
// value = m * 2^e with m in [1,2) (or m==0). 53-bit double mantissa =>
// arithmetic below the naneinf threshold rounds identically to vanilla
// doubles => functional equivalence preserved.
// Win condition: score >= 2^1024, i.e. normalized e >= 1024. Never isinf.
#pragma once
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>

struct BigNum {
    double  m = 0.0;   // mantissa, [1,2) when nonzero
    int64_t e = 0;     // binary exponent

    BigNum() = default;
    BigNum(double v) { *this = fromDouble(v); }   // implicit on purpose

    static BigNum fromDouble(double v) {
        assert(!std::isnan(v) && !std::isinf(v) && v >= 0.0);
        BigNum b;
        if (v == 0.0) return b;
        int ex;
        b.m = std::frexp(v, &ex) * 2.0;   // frexp gives [0.5,1); shift to [1,2)
        b.e = ex - 1;
        return b;
    }

    bool isZero() const { return m == 0.0; }

    void normalize() {
        if (m == 0.0) { e = 0; return; }
        int ex;
        m = std::frexp(m, &ex) * 2.0;
        e += ex - 1;
    }

    BigNum operator+(const BigNum& o) const {
        if (isZero()) return o;
        if (o.isZero()) return *this;
        const BigNum *hi = this, *lo = &o;
        if (o.e > e) { hi = &o; lo = this; }
        int64_t d = hi->e - lo->e;
        BigNum r = *hi;
        if (d < 64)                       // beyond 64 bits the small term
            r.m += std::ldexp(lo->m, (int)-d);   // vanishes entirely
        r.normalize();
        return r;
    }

    BigNum operator*(const BigNum& o) const {
        if (isZero() || o.isZero()) return BigNum{};
        BigNum r;
        r.m = m * o.m;                    // [1,4): safely finite
        r.e = e + o.e;
        r.normalize();
        return r;
    }

    BigNum half() const { BigNum r = *this; if (!r.isZero()) r.e -= 1; return r; }

    // Largest integer <= this value. Values with e >= 52 are already
    // integral (the 53-bit mantissa has no fractional bits left), so floor
    // is the identity there — which matters in deep endless where the
    // value is far beyond what a double could represent.
    BigNum floorVal() const {
        if (isZero() || e >= 52) return *this;
        double v = std::ldexp(m, (int)e);
        return fromDouble(std::floor(v));
    }

    bool operator>=(const BigNum& o) const {
        if (isZero()) return o.isZero();
        if (o.isZero()) return true;
        if (e != o.e) return e > o.e;
        return m >= o.m;
    }
    bool operator<(const BigNum& o) const { return !(*this >= o); }

    // The win condition (2^1024): a plain inequality, never an overflow.
    bool naneinf() const { return !isZero() && e >= 1024; }

    // For display/observation only; saturates instead of overflowing.
    double toDouble() const {
        if (isZero()) return 0.0;
        if (e >= 1024) return 1.7976931348623157e308;
        return std::ldexp(m, (int)e);
    }
    void print(const char* label) const {
        if (e < 1000) std::printf("%s%.6g", label, toDouble());
        else std::printf("%s%.6f * 2^%lld", label, m, (long long)e);
    }
};
