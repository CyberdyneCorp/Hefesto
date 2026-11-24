// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Correctly rounded binary64 arctangent (cr_atan.hpp). Owner: EXTCAP.
// Build: -ffp-contract=off (hefesto_core); the only fused operation is the
// explicit std::fma of two_prod, which IEEE 754 defines exactly.

#include "cr_atan.hpp"

#include <array>
#include <cmath>
#include <cstddef>

namespace hefesto::util {

namespace {

/// An unevaluated sum hi + lo with |lo| <= ulp(hi) / 2.
struct DD {
  double hi = 0.0;
  double lo = 0.0;
};

constexpr DD two_sum(double a, double b) noexcept {
  const double s = a + b;
  const double bb = s - a;
  return {s, (a - (s - bb)) + (b - bb)};
}

constexpr DD fast_two_sum(double a, double b) noexcept {
  const double s = a + b;
  return {s, b - (s - a)};
}

DD two_prod(double a, double b) noexcept {
  const double p = a * b;
  return {p, std::fma(a, b, -p)};
}

DD add(DD a, DD b) noexcept {
  DD s = two_sum(a.hi, b.hi);
  const DD t = two_sum(a.lo, b.lo);
  s.lo += t.hi;
  s = fast_two_sum(s.hi, s.lo);
  s.lo += t.lo;
  return fast_two_sum(s.hi, s.lo);
}

DD neg(DD a) noexcept { return {-a.hi, -a.lo}; }

DD mul(DD a, DD b) noexcept {
  DD p = two_prod(a.hi, b.hi);
  p.lo += a.hi * b.lo + a.lo * b.hi;
  return fast_two_sum(p.hi, p.lo);
}

DD mul(DD a, double b) noexcept {
  DD p = two_prod(a.hi, b);
  p.lo += a.lo * b;
  return fast_two_sum(p.hi, p.lo);
}

DD div(DD a, DD b) noexcept {
  const double q1 = a.hi / b.hi;
  DD r = add(a, neg(mul(b, q1)));
  const double q2 = r.hi / b.hi;
  r = add(r, neg(mul(b, q2)));
  const double q3 = r.hi / b.hi;
  return add(fast_two_sum(q1, q2), DD{q3, 0.0});
}

/// 1 / a for a > 1, to about 104 bits.
DD reciprocal(double a) noexcept {
  const double q1 = 1.0 / a;
  const double r = std::fma(-q1, a, 1.0);  // exact remainder
  return fast_two_sum(q1, r / a);
}

// atan(k/64), k = 0..64, as double-double (300-bit evaluation, rounded).
constexpr std::array<DD, 65> kAtanTable{{
    {0x0.0p+0, 0x0.0p+0},
    {0x1.fff555bbb729bp-7, -0x1.220c39d4dff50p-61},
    {0x1.ffd55bba97625p-6, -0x1.5ec431444912cp-60},
    {0x1.7fb818430da2ap-5, -0x1.86ef8f794f105p-63},
    {0x1.ff55bb72cfdeap-5, -0x1.c934d86d23f1dp-60},
    {0x1.3f59f0e7c559dp-4, 0x1.ac4ce285df847p-58},
    {0x1.7ee182602f10fp-4, -0x1.cfb654c0c3d98p-58},
    {0x1.be39ebe6f07c3p-4, 0x1.f7b8f29a05987p-58},
    {0x1.fd5ba9aac2f6ep-4, -0x1.cd37686760c17p-59},
    {0x1.1e1fafb043727p-3, -0x1.b485914dacf8cp-59},
    {0x1.3d6eee8c6626cp-3, 0x1.61a3b0ce9281bp-57},
    {0x1.5c9811e3ec26ap-3, -0x1.054ab2c010f3dp-58},
    {0x1.7b97b4bce5b02p-3, 0x1.347b0b4f881cap-58},
    {0x1.9a6a8e96c8626p-3, 0x1.cf601e7b4348ep-59},
    {0x1.b90d7529260a2p-3, 0x1.17b10d2e0e5abp-61},
    {0x1.d77d5df205736p-3, 0x1.c648d1534597ep-57},
    {0x1.f5b75f92c80ddp-3, 0x1.8ab6e3cf7afbdp-57},
    {0x1.09dc597d86362p-2, 0x1.62e47390cb865p-56},
    {0x1.18bf5a30bf178p-2, 0x1.30ca4748b1bf9p-57},
    {0x1.278372057ef46p-2, -0x1.077cdd36dfc81p-56},
    {0x1.362773707ebccp-2, -0x1.963a544b672d8p-57},
    {0x1.44aa436c2af0ap-2, -0x1.5d5e43c55b3bap-56},
    {0x1.530ad9951cd4ap-2, -0x1.2566480884082p-57},
    {0x1.614840309cfe2p-2, -0x1.a725715711f00p-56},
    {0x1.6f61941e4def1p-2, -0x1.c63aae6f6e918p-56},
    {0x1.7d5604b63b3f7p-2, 0x1.69c885c2b249ap-56},
    {0x1.8b24d394a1b25p-2, 0x1.b6d0ba3748fa8p-56},
    {0x1.98cd5454d6b18p-2, 0x1.9e6c988fd0a77p-56},
    {0x1.a64eec3cc23fdp-2, -0x1.24dec1b50b7ffp-56},
    {0x1.b3a911da65c6cp-2, 0x1.ae187b1ca5040p-56},
    {0x1.c0db4c94ec9f0p-2, -0x1.cc1ce70934c34p-56},
    {0x1.cde53432c1351p-2, -0x1.a2cfa4418f1adp-56},
    {0x1.dac670561bb4fp-2, 0x1.a2b7f222f65e2p-56},
    {0x1.e77eb7f175a34p-2, 0x1.0e53dc1bf3435p-56},
    {0x1.f40dd0b541418p-2, -0x1.a3992dc382a23p-57},
    {0x1.0039c73c1a40cp-1, -0x1.b32c949c9d593p-55},
    {0x1.0657e94db30d0p-1, -0x1.d5b495f6349e6p-56},
    {0x1.0c6145b5b43dap-1, 0x1.974fa13b5404fp-58},
    {0x1.1255d9bfbd2a9p-1, -0x1.2bdaee1c0ee35p-58},
    {0x1.1835a88be7c13p-1, 0x1.c621cec00c301p-55},
    {0x1.1e00babdefeb4p-1, -0x1.928df287a668fp-58},
    {0x1.23b71e2cc9e6ap-1, 0x1.c421c9f38224ep-57},
    {0x1.2958e59308e31p-1, -0x1.09e73b0c6c087p-56},
    {0x1.2ee628406cbcap-1, 0x1.c5d5e9ff0cf8dp-55},
    {0x1.345f01cce37bbp-1, 0x1.1021137c71102p-55},
    {0x1.39c391cd4171ap-1, -0x1.2304331d8bf46p-55},
    {0x1.3f13fb89e96f4p-1, 0x1.ecf8b492644f0p-56},
    {0x1.445065b795b56p-1, -0x1.f76d0163f79c8p-56},
    {0x1.4978fa3269ee1p-1, 0x1.2419a87f2a458p-56},
    {0x1.4e8de5bb6ec04p-1, 0x1.4a33dbeb3796cp-55},
    {0x1.538f57b89061fp-1, -0x1.1bb74abda520cp-55},
    {0x1.587d81f732fbbp-1, -0x1.5e5c9d8c5a950p-56},
    {0x1.5d58987169b18p-1, 0x1.0028e4bc5e7cap-57},
    {0x1.6220d115d7b8ep-1, -0x1.2b785350ee8c1p-57},
    {0x1.66d663923e087p-1, -0x1.6ea6febe8bbbap-56},
    {0x1.6b798920b3d99p-1, -0x1.a80386188c50ep-55},
    {0x1.700a7c5784634p-1, -0x1.8c34d25aadef6p-56},
    {0x1.748978fba8e0fp-1, 0x1.7b2a6165884a1p-59},
    {0x1.78f6bbd5d315ep-1, 0x1.406a089803740p-55},
    {0x1.7d528289fa093p-1, 0x1.560821e2f3aa9p-55},
    {0x1.819d0b7158a4dp-1, -0x1.bf76229d3b917p-56},
    {0x1.85d69576cc2c5p-1, 0x1.6b66e7fc8b8c3p-57},
    {0x1.89ff5ff57f1f8p-1, -0x1.55b9a5e177a1bp-55},
    {0x1.8e17aa99cc05ep-1, -0x1.ec182ab042f61p-56},
    {0x1.921fb54442d18p-1, 0x1.1a62633145c07p-55},
}};

// (-1)^j / (2j + 1), j = 0..9, as double-double: atan(u)/u as a polynomial in u^2.
constexpr std::array<DD, 10> kSeries{{
    {0x1.0000000000000p+0, 0x0.0p+0},
    {-0x1.5555555555555p-2, -0x1.5555555555555p-56},
    {0x1.999999999999ap-3, -0x1.999999999999ap-57},
    {-0x1.2492492492492p-3, -0x1.2492492492492p-57},
    {0x1.c71c71c71c71cp-4, 0x1.c71c71c71c71cp-58},
    {-0x1.745d1745d1746p-4, 0x1.745d1745d1746p-59},
    {0x1.3b13b13b13b14p-4, -0x1.3b13b13b13b14p-58},
    {-0x1.1111111111111p-4, -0x1.1111111111111p-60},
    {0x1.e1e1e1e1e1e1ep-5, 0x1.e1e1e1e1e1e1ep-61},
    {-0x1.af286bca1af28p-5, -0x1.af286bca1af28p-59},
}};

constexpr DD kHalfPi{0x1.921fb54442d18p+0, 0x1.1a62633145c07p-54};

/// atan(t) for t in [0, 1] given as a double-double.
DD atan_unit(DD t) noexcept {
  const auto k = static_cast<std::size_t>(t.hi * 64.0 + 0.5);  // t.hi in [0, 1]: k in 0..64
  const double c = static_cast<double>(k) / 64.0;
  const DD num = add(t, DD{-c, 0.0});
  const DD den = add(DD{1.0, 0.0}, mul(t, c));
  const DD u = div(num, den);
  const DD z = mul(u, u);
  DD s = kSeries.back();
  for (std::size_t j = kSeries.size() - 1; j-- > 0;) {
    s = add(kSeries[j], mul(s, z));
  }
  return add(kAtanTable[k], mul(u, s));
}

}  // namespace

double cr_atan(double x) noexcept {
  if (std::isnan(x)) {
    return x + x;
  }
  const double a = std::fabs(x);
  // atan(a) = a - a^3/3 + ...: below 2^-27 the correction is under half an
  // ulp of a, so a (or +-0) is the rounded value.
  if (a < 0x1p-27) {
    return x;
  }
  // atan(a) = pi/2 - 1/a + ...: above 2^60, 1/a is far below half an ulp.
  if (a > 0x1p+60) {
    return std::copysign(kHalfPi.hi, x);
  }
  DD r;
  if (a <= 1.0) {
    r = atan_unit(DD{a, 0.0});
  } else {
    r = add(kHalfPi, neg(atan_unit(reciprocal(a))));
  }
  return std::copysign(r.hi + r.lo, x);
}

}  // namespace hefesto::util
