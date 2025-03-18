#pragma once

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

#include "arcana/noelle/core/Pragma.h"

namespace skynet {

const uint32_t L1D_CACHE_LINE_SIZE = 64;

template <class T,
          uint32_t _PAD =
              std::max<uint32_t>(L1D_CACHE_LINE_SIZE / sizeof(T), 1)>
class Scalar;

template <class T, uint32_t _PAD>
void clause_scalar_add(int N, Scalar<T, _PAD> *s) {
  N *= _PAD;
#ifdef DEBUG
  std::printf("%s(%i, %p, %i)\n", __func__, N, s);
#endif
  if (s->container_.size() == N) {
    return;
  }
  assert(s->container_.size() == 1);
  s->container_.resize(N);
}

template <class T, uint32_t _PAD>
class Scalar {
public:
  friend void clause_scalar_add<T, _PAD>(int N, Scalar<T, _PAD> *s);

  Scalar(T x) : container_{ x } {}

  Scalar() : Scalar(T{}) {}

  __attribute__((always_inline)) void add(T x) {
    size_t k = 0;
    auto _p = noelle_pragma_begin("ldtc",
                                  &k,
                                  (size_t)0,
                                  clause_scalar_add<T, _PAD>,
                                  this);
    container_[k * _PAD] += x;
    noelle_pragma_end(_p);
  }

  void set(T x) {
    for (auto &e : container_) {
      e = T{};
    }
    container_[0] = x;
  }

  T get() const {
    T acc = container_[0];
    for (size_t i = 1; i < container_.size(); i++) {
      acc = acc + container_.at(i);
    }
    return acc;
  }

  void __add(size_t k, T x) {
    container_[k * _PAD] += x;
  }

  void printInternals() const {
    std::cout << "{ ";
    for (auto x : container_) {
      std::cout << x << " ";
    }
    std::cout << "}\n";
  }

private:
  std::vector<T> container_;
};

} // namespace skynet
