#pragma once

#include <iostream>
#include <vector>

#include "arcana/noelle/core/Pragmas.hpp"

namespace skynet {

template <class T>
class Scalar;

template <class T>
size_t clause_scalar_sum(Scalar<T> *s) {
  s->container_.resize(s->container_.size() + 1);
  return s->container_.size() - 1;
}

template <class T>
class Scalar {
public:
  friend size_t clause_scalar_sum<T>(Scalar<T> *s);

  Scalar() : container_(1) {}

  Scalar(T x) : Scalar() {
    container_[0] = x;
  }

  __attribute__((always_inline)) void sum(T x) {
    size_t k = 0;
    noelle_pragma_begin("ldtc", &k, (size_t)0, clause_scalar_sum<T>, this);
    container_[k] += x;
    noelle_pragma_end("ldtc");
  }

  T get() const {
    T acc = container_[0];
    for (size_t i = 1; i < container_.size(); i++) {
      acc = acc + container_.at(i);
    }
    return acc;
  }

private:
  std::vector<T> container_;
};

} // namespace skynet
