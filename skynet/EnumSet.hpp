#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

#include "arcana/noelle/core/Pragma.h"

namespace skynet {

template <class T, int M = 1, size_t Padding = 1, bool Resizable = false>
class EnumSet;

template <class T, int M, size_t Padding, bool Resizable>
void clause_enumset_insert(int N, EnumSet<T, M, Padding, Resizable> *enumset) {
  int K = (N + M - 1) / M;
  auto currentK = enumset->buf_.size();
  if (currentK == K) {
    return;
  }
  assert(currentK < K);
  enumset->buf_.resize(K);
  auto L = enumset->buf_[0].size();
  for (auto &row : enumset->buf_) {
    row.resize(L);
  }
}

template <class T, int M, size_t Padding, bool Resizable>
class EnumSet {
public:
  EnumSet(size_t size) {
    buf_.emplace_back(((size * Padding) + 7) / 8);
    capacity_ = buf_[0].size() * 8;
  }

  void __insert(int t, T value) {
    auto &row = buf_[t / M];
    if constexpr (Resizable) {
      if ((value * Padding) >= capacity_) {
        row.resize((value * Padding) / 8 + 1);
        capacity_ = row.size() * 8;
      }
    }

    if constexpr (M == 1) {
      row[(value * Padding) / 8] |= 1 << ((value * Padding) % 8);
    } else {
      uint8_t old_value;
      uint8_t new_value;
      uint8_t *addr = &row[(value * Padding) / 8];
      do {
        old_value = *addr;
        new_value = old_value | (1 << ((value * Padding) % 8));
      } while (!__sync_bool_compare_and_swap(addr, old_value, new_value));
    }
  }

  __attribute__((always_inline)) void insert(T value) {
    int t = 0;
    auto p =
        noelle_pragma_begin("ldtc",
                            &t,
                            0,
                            clause_enumset_insert<T, M, Padding, Resizable>,
                            this);
    __insert(t, value);
    noelle_pragma_end(p);
  }

  bool contains(T value) const {
    if constexpr (Resizable) {
      if ((value * Padding) >= capacity_) {
        return false;
      }
    }
    for (auto &row : buf_) {
      if (row[(value * Padding) / 8] & (1 << ((value * Padding) % 8))) {
        return true;
      }
    }
    return false;
  }

  bool find(T value) const {
    return contains(value);
  }

  bool end() const {
    return false;
  }

  void clear() {
    memset(buf_.data(), 0x00, sizeof(uint8_t) * buf_.size());
  }

  // private:
  std::vector<std::vector<uint8_t>> buf_;
  size_t capacity_;
};

} // namespace skynet
