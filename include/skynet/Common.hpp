#pragma once

#include <algorithm>
#include <cstdint>

#if defined(SKYNET_ENABLE_ASSERT)
#include <cassert>
#define skynet_assert(expr) assert(expr)
#else
#define skynet_assert(expr) ((void)0)
#endif

#define INLINE __attribute__((always_inline))

const uint32_t L1D_CACHE_LINE_SIZE = 64;

constexpr uint32_t gcd(uint32_t a, uint32_t b) {
  return b == 0 ? a : gcd(b, a % b);
}

constexpr uint32_t lcm(uint32_t a, uint32_t b) {
  return (a / gcd(a, b)) * b;
}

template <typename T>
constexpr uint32_t compute_padding() {
  return lcm(L1D_CACHE_LINE_SIZE, sizeof(T)) / sizeof(T);
}
