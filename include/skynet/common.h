#pragma once

#include <stddef.h>
#include <stdint.h>

#ifndef INLINE
#define INLINE __attribute__((always_inline))
#endif

enum { SKYNET_L1D_CACHE_LINE_SIZE = 64u };

static inline uint32_t skynet_gcd_u32(uint32_t a, uint32_t b) {
  while (b != 0u) {
    uint32_t t = a % b;
    a = b;
    b = t;
  }
  return a;
}

static inline uint32_t skynet_lcm_u32(uint32_t a, uint32_t b) {
  if (a == 0u || b == 0u) {
    return 0u;
  }
  return (a / skynet_gcd_u32(a, b)) * b;
}

static inline uint32_t skynet_compute_padding_for_size(size_t elem_size) {
  uint32_t sz;
  if (elem_size == 0u) {
    return 0u;
  }
  sz = (uint32_t)elem_size;
  return skynet_lcm_u32(SKYNET_L1D_CACHE_LINE_SIZE, sz) / sz;
}
