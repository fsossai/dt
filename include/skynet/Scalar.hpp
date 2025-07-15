#pragma once

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

#include "arcana/noelle/core/Pragma.h"

#include "Common.hpp"

namespace skynet {

const uint32_t L1D_CACHE_LINE_SIZE = 64;

template <class T,
          uint32_t PAD = std::max<uint32_t>(L1D_CACHE_LINE_SIZE / sizeof(T), 1)>
class Scalar;

template <class T, uint32_t PAD>
void clause_scalar_add(int N, Scalar<T, PAD> *s) {
  N *= PAD;
#ifdef DEBUG
  std::printf("%s(%i, %p, %i)\n", __func__, N, s);
#endif
  if (s->container_.size() == N) {
    return;
  }
  assert(s->container_.size() == 1);
  s->container_.resize(N);
}

template <class T, uint32_t PAD>
void clause_scalar_keepMin(int N, Scalar<T, PAD> *s) {
  auto value = s->get();
  s->container_.resize(1);
  s->container_[0] = value;
}

template <class T, uint32_t PAD>
class Scalar {
public:
  friend void clause_scalar_add<T, PAD>(int N, Scalar<T, PAD> *s);
  friend void clause_scalar_keepMin<T, PAD>(int N, Scalar<T, PAD> *s);

  Scalar(T x) : container_{ x } {}

  Scalar() : Scalar(T{}) {}

  INLINE void add(T x) {
    size_t k = 0;
    auto _p = noelle_pragma_begin("ldtc",
                                  &k,
                                  (size_t)0,
                                  clause_scalar_add<T, PAD>,
                                  this);
    container_[k * PAD] += x;
    noelle_pragma_end(_p);
  }

  void set(T x) {
    for (auto &e : container_) {
      e = T{};
    }
    container_[0] = x;
  }

  bool keepMin(T new_value) {
    int k;
    auto p = noelle_pragma_begin("ldtc",
                                 &k,
                                 (int)0,
                                 clause_scalar_keepMin<T, PAD>,
                                 this);
    auto *addr = &container_[0];
    auto current_value = *addr;
    while (current_value > new_value) {
      if (__sync_bool_compare_and_swap(addr, current_value, new_value)) {
        return true;
      }
      current_value = *addr;
    }
    noelle_pragma_end(p);
    return false;
  }

  INLINE operator T() {
    return get();
  }

  T get() const {
    T acc = container_[0];
#pragma omp parallel for reduction(+ : acc)
    for (size_t i = 1; i < container_.size(); i++) {
      acc += container_[i];
    }
    return acc;
  }

  INLINE void operator++() {
    add(1);
  }

  INLINE void operator+=(T x) {
    add(x);
  }

  INLINE void __add(size_t k, T x) {
    container_[k * PAD] += x;
  }

  void printInternals() const {
    std::cout << "{ ";
    for (auto x : container_) {
      std::cout << x << " ";
    }
    std::cout << "}\n";
  }

  // private:
  std::vector<T> container_;
};

} // namespace skynet
