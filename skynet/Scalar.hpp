#pragma once

#include <vector>

#include "TCI.hpp"

namespace skynet {

template <class T>
class Scalar;

template <class T>
size_t scalar_clause_sum(Scalar<T> *s) {
  s->container_.resize(s->container_.size() + 1);
  return s->container_.size() - 1;
}

template <class T>
class Scalar {
public:
  friend size_t scalar_clause_sum<T>(Scalar<T> *s);

  Scalar() : container_(1) {}

  Scalar(T x) : Scalar() {
    container_[0] = x;
  }

  __attribute__((always_inline)) void sum(T x) {
    size_t k = 0;
    PRAGMA_LDTC_BEGIN(k, (size_t)0, scalar_clause_sum<T>, this);
    container_[k] += x;
    PRAGMA_LDTC_END();
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
