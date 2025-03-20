#pragma once

#include <cstdint>
#include <cstring>
#include <vector>
#include <cassert>

#include "Common.hpp"

#include "arcana/noelle/core/Pragma.h"

namespace skynet {

template <class T = size_t, size_t Padding = 1>
class EnumSet {
public:
  EnumSet(size_t size) {
    buf_size_ = ((size * Padding) + 7) / 8;
    buf_ = new uint8_t[buf_size_];
  }

  ~EnumSet() {
    delete buf_;
  }

  INLINE void insert(size_t value) {
    auto p = noelle_pragma_begin("ldtc");
    __atomic_fetch_or(&buf_[(value * Padding) / 8],
                      1ULL << ((value * Padding) % 8),
                      __ATOMIC_RELAXED);
    noelle_pragma_end(p);
  }

  bool contains(size_t value) const {
    return buf_[(value * Padding) / 8] & (1ULL << ((value * Padding) % 8ULL));
  }

  bool find(T value) const {
    return contains(value);
  }

  bool end() const {
    return false;
  }

  void clear() {
    memset(buf_, 0x00, sizeof(uint8_t) * buf_size_);
  }

private:
  uint8_t *buf_;
  size_t buf_size_;
};

} // namespace skynet
