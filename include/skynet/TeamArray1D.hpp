#pragma once

#include <atomic>
#include <cstddef>
#include <vector>
#include <omp.h>

#include "Common.hpp"
#include "arcana/noelle/core/Pragma.h"

namespace skynet {

template <typename T, size_t K>
class TeamArray1D;

template <typename T, size_t K>
void clause_team_array1d_add(int N, TeamArray1D<T, K> *array) {
  skynet_assert(N >= 1);
  const size_t teams = ceil_div(static_cast<size_t>(N), K);
  const size_t old_teams = array->container_.size();
  array->container_.resize(teams);

  for (size_t t = old_teams; t < teams; ++t) {
    array->container_[t].resize(array->size_, array->default_value_);
  }
}

template <class T, size_t K>
class TeamArray1D {
  static_assert(K >= 1, "team size K must be >= 1");

public:
  friend void clause_team_array1d_add<T, K>(int N, TeamArray1D<T, K> *array);

  explicit TeamArray1D(size_t size) : TeamArray1D(size, T{}) {}

  TeamArray1D(size_t size, T default_value)
    : size_(size),
      default_value_(std::move(default_value)) {
    container_.resize(1);
    container_[0].resize(size_, default_value_);
  }

  INLINE void __add(size_t t, size_t idx, const T &value) {
    const size_t team = t / K;
    skynet_assert(team < lanes());
    skynet_assert(idx < size_);
    using IntT =
        std::conditional_t<sizeof(T) == 4,
                           uint32_t,
                           std::conditional_t<sizeof(T) == 8, uint64_t, void>>;

    auto *addr = &container_[team][idx];
    T current_value = *addr;
    T new_value;
    do {
      new_value = current_value + value;
    } while (
        !__atomic_compare_exchange(reinterpret_cast<IntT *>(addr),
                                   reinterpret_cast<IntT *>(&current_value),
                                   reinterpret_cast<IntT *>(&new_value),
                                   true,
                                   __ATOMIC_RELAXED,
                                   __ATOMIC_RELAXED));
  }

  INLINE void add(size_t idx, const T &value) {
    int t = 0;
    auto p =
        noelle_pragma_begin("ldtc", &t, 0, clause_team_array1d_add<T, K>, this);
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

  static constexpr size_t team_size() {
    return K;
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
