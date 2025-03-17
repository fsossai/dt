#pragma once

#include <cstdint>
#include <cstring>
#include <vector>
#include <cassert>

#include "arcana/noelle/core/Pragma.h"

namespace skynet {

template <class T, size_t Padding = 1, bool Resizable = false>
class MEnumSet;

template <class T, size_t Padding, bool Resizable>
void clause_enumset_insert(int N, MEnumSet<T, Padding, Resizable> *enumset) {
  int K = (N + enumset->M_ - 1) / enumset->M_;
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

template <class T = size_t, size_t Padding, bool Resizable>
class MEnumSet {
public:
  MEnumSet(size_t size) : M_(1) {
    buf_.emplace_back(((size * Padding) + 7) / 8);
    capacity_ = buf_[0].size() * 8;
  }

  void __insert(int t, size_t value) {
    auto &row = buf_[t / M_];
    if constexpr (Resizable) {
      if ((value * Padding) >= capacity_) {
        row.resize((value * Padding) / 8 + 1);
        capacity_ = row.size() * 8;
      }
    }

    __atomic_fetch_or(&row[(value * Padding) / 8],
                      1ULL << ((value * Padding) % 8),
                      __ATOMIC_RELAXED);
  }

  void setSharing(int M) {
    assert(M >= 1);
    M_ = M;
  }

  __attribute__((always_inline)) void insert(size_t value) {
    int t = 0;
    auto p = noelle_pragma_begin("ldtc",
                                 &t,
                                 0,
                                 clause_enumset_insert<T, Padding, Resizable>,
                                 this);
    __insert(t, value);
    noelle_pragma_end(p);
  }

  bool contains(size_t value) const {
    if constexpr (Resizable) {
      if ((value * Padding) >= capacity_) {
        return false;
      }
    }
    for (auto &row : buf_) {
      if (row[(value * Padding) / 8] & (1ULL << ((value * Padding) % 8ULL))) {
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
    for (auto &row : buf_) {
      memset(row.data(), 0x00, sizeof(uint8_t) * row.size());
    }
  }

  // private:
  std::vector<std::vector<uint8_t>> buf_;
  size_t capacity_;
  int M_;
};

} // namespace skynet
