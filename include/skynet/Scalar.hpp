#pragma once

#include <cassert>
#include <cstring>
#include <cstdint>
#include <iostream>
#include <vector>

#include "arcana/noelle/core/Pragma.h"

#include "Common.hpp"

namespace skynet {

template <class T, uint32_t PAD = compute_padding<T>()>
class Scalar;

template <class T, uint32_t PAD>
void clause_scalar_add(int N, Scalar<T, PAD> *s) {
  if (s->container_.size() / PAD == N) {
    return;
  }
  s->container_.resize(N * PAD);
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
    std::memset(container_.data(), 0x0, container_.size() * sizeof(T));
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
    const size_t P = container_.size() / PAD;
    // #pragma omp parallel for reduction(+ : acc) // not worth it
    for (size_t i = 1; i < P; i++) {
      acc += container_[i * PAD];
    }
    return acc;
  }

  INLINE T stale_read() const {
    int k;
    auto p = noelle_pragma_begin("ldtc", &k, (int)0);
    auto &x = container_[k * PAD];
    noelle_pragma_end(p);
    return x;
  }

  INLINE void stale_write(T x) {
    int k;
    auto p = noelle_pragma_begin("ldtc", &k, (int)0);
    container_[k * PAD] = std::move(x);
    noelle_pragma_end(p);
  }

  INLINE T __stale_read(int k) const {
    return container_[k * PAD];
  }

  INLINE void __stale_write(int k, T x) {
    container_[k * PAD] = std::move(x);
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

  void reduce() {
    container_[0] = get();
    container_.resize(1 * PAD);
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
