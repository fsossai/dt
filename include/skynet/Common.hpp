#pragma once

#include <algorithm>
#include <cstdint>

#define INLINE __attribute__((always_inline))

const uint32_t L1D_CACHE_LINE_SIZE = 64;

template <typename T>
constexpr uint32_t compute_padding() {
  return std::max<uint32_t>((L1D_CACHE_LINE_SIZE + sizeof(T) - 1) / sizeof(T),
                            1U);
}
