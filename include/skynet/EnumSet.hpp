#pragma once

#include <cstdint>
#include <cstring>
#include <vector>
#include <cassert>

#include "Common.hpp"

#include "arcana/noelle/core/Pragma.h"

namespace skynet {

template <size_t BITSIZE = 1>
class EnumSet {
public:
  EnumSet(size_t size) {
    buf_size_ = ((size * BITSIZE) + 7) / 8;
    buf_ = static_cast<uint8_t *>(aligned_alloc(64, buf_size_));
  }

  ~EnumSet() {
    free(buf_);
  }

  INLINE void insert(size_t value) {
    auto p = noelle_pragma_begin("ldtc");
    if constexpr (BITSIZE % 8 == 0) {
      buf_[value * (BITSIZE / 8)] = 1;
    } else {
      __atomic_fetch_or(&buf_[(value * BITSIZE) / 8],
                        1ULL << ((value * BITSIZE) % 8),
                        __ATOMIC_RELAXED);
    }
    noelle_pragma_end(p);
  }

  INLINE bool contains(size_t value) const {
    if constexpr (BITSIZE % 8 == 0) {
      return buf_[value * (BITSIZE / 8)];
    } else {
      return buf_[(value * BITSIZE) / 8] & (1ULL << ((value * BITSIZE) % 8ULL));
    }
  }

  bool find(size_t value) const {
    return contains(value);
  }

  bool end() const {
    return false;
  }

  void clear() {
    memset(buf_, 0x00, sizeof(uint8_t) * buf_size_);
  }

  // private:
  alignas(64) uint8_t *buf_;
  size_t buf_size_;
};

} // namespace skynet
