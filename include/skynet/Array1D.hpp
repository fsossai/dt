#pragma once

#include <cstddef>
#include <vector>
#include <omp.h>

#include "Common.hpp"
#include "arcana/noelle/core/Pragma.h"

namespace skynet {

template <typename T>
std::vector<T> &operator+=(std::vector<T> &lhs, const std::vector<T> &rhs) {
  const auto N = lhs.size();
  for (size_t i = 0; i < N; ++i) {
    lhs[i] += rhs[i];
  }
  return lhs;
}

template <typename T>
class Array1D;

template <typename T>
void clause_array1d_add(int N, Array1D<T> *array) {
  skynet_assert(N >= 1);
  const size_t old_N = array->container_.size();
  array->container_.resize(N);

  // constexpr int PAD = ceil_div(1024, sizeof(T));

  for (size_t t = old_N; t < N; ++t) {
    // array->container_[t].reserve(array->size_ + PAD);
    array->container_[t].resize(array->size_, array->default_value_);
  }
}

template <class T>
class Array1D {
public:
  friend void clause_array1d_add<T>(int N, Array1D<T> *array);

  explicit Array1D(size_t size) : Array1D(size, T{}) {}

  Array1D(size_t size, T default_value)
    : size_(size),
      default_value_(std::move(default_value)) {
    container_.resize(1);
    container_[0].resize(size_, default_value_);
  }

  INLINE void __add(size_t t, size_t idx, const T &value) {
    skynet_assert(t < lanes());
    skynet_assert(idx < size_);
    container_[t][idx] += value;
  }

  INLINE void add(size_t idx, const T &value) {
    int t = 0;
    auto p = noelle_pragma_begin("ldtc", &t, 0, clause_array1d_add<T>, this);
    __add(t, idx, value);
    noelle_pragma_end(p);
  }

  INLINE T operator[](size_t idx) const {
    skynet_assert(idx < size_);
    T v = default_value_;
    for (const auto &lane : container_) {
      v += lane[idx];
    }
    return v;
  }

  void fill(const T &value) {
    for (auto &lane : container_) {
      for (auto &x : lane) {
        x = value;
      }
    }
  }

  void reduce() {
    for (size_t i = 0; i < size_; i++) {
      container_[0][i] = operator[](i);
    }
    container_.resize(1);
  }

  void reduce_par() {
#pragma omp parallel for
    for (size_t i = 0; i < size_; i++) {
      container_[0][i] = operator[](i);
    }
    container_.resize(1);
  }

  void reduce_rd(bool parallel = true) {
    const int nthreads = omp_get_max_threads() ? parallel : 1;
    int P = container_.size();
    int N = container_[0].size();

    // Recursive doubling
    for (int step = 1; step < P; step <<= 1) {
#pragma omp parallel for num_threads(nthreads)
      for (int i = 0; i < P; i += 2 * step) {
        int j = i + step;
        auto &lhs = container_[i];
        auto &rhs = container_[j];
        if (j < P) {
          for (int k = 0; k < N; ++k)
            lhs[k] += rhs[k];
        }
      }
    }
    container_.resize(1);
  }

  size_t size() const {
    return size_;
  }

  std::vector<T> &lane(size_t t) {
    skynet_assert(t < lanes());
    return container_[t];
  }

  const std::vector<T> &lane(size_t t) const {
    skynet_assert(t < lanes());
    return container_[t];
  }

private:
  size_t lanes() const {
    return container_.size();
  }

  std::vector<std::vector<T>> container_;
  size_t size_;
  T default_value_;
};

} // namespace skynet
