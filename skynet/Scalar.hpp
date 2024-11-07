#pragma once

#include <cassert>
#include <iostream>
#include <vector>

#include "arcana/noelle/core/Pragma.h"

namespace skynet {

template <class T>
class Scalar;

template <class T>
size_t clause_scalar_sum(Scalar<T> *s) {
#ifdef DEBUG
  std::printf("%s(%p)\n", __func__, s);
#endif
  s->container_.resize(s->container_.size() + 1);
  return s->container_.size() - 1;
}

template <class T>
void clause_scalar_sum_bulk(int N, Scalar<T> *s) {
#ifdef DEBUG
  std::printf("%s(%i, %p, %i)\n", __func__, N, s);
#endif
  if (s->container_.size() == N) {
    return;
  }
  assert(s->container_.size() == 1);
  s->container_.resize(N);
}

template <class T>
class Scalar {
public:
  friend size_t clause_scalar_sum<T>(Scalar<T> *s);
  friend void clause_scalar_sum_bulk<T>(int N, Scalar<T> *s);

  Scalar(T x) : container_{ x } {}

  Scalar() : Scalar(T{}) {}

  __attribute__((always_inline)) void sum(T x) {
    size_t k = 0;
    auto _p =
        noelle_pragma_begin("ldtc", &k, (size_t)0, clause_scalar_sum_bulk<T>, this);
    container_[k] += x;
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

  void __sum(size_t k, T x) {
    container_[k] += x;
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
