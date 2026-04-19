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
void clause_scalar_min(int N, Scalar<T, PAD> *s) {
  if (s->container_.size() / PAD == N) {
    return;
  }
  s->container_.resize(N * PAD);
}

template <class T, uint32_t PAD>
void clause_scalar_max(int N, Scalar<T, PAD> *s) {
  if (s->container_.size() / PAD == N) {
    return;
  }
  s->container_.resize(N * PAD);
}

template <class T, uint32_t PAD>
void clause_scalar_keepMin(int N, Scalar<T, PAD> *s) {
  auto value = s->get();
  s->container_.resize(1);
  auto &lane0 = s->container_[0];
  lane0 = value;
}

template <class T, uint32_t PAD>
void clause_scalar_keepMax(int N, Scalar<T, PAD> *s) {
  auto value = s->get();
  s->container_.resize(1);
  auto &lane0 = s->container_[0];
  lane0 = value;
}

template <class T, uint32_t PAD>
class Scalar {
public:
  friend void clause_scalar_add<T, PAD>(int N, Scalar<T, PAD> *s);
  friend void clause_scalar_min<T, PAD>(int N, Scalar<T, PAD> *s);
  friend void clause_scalar_max<T, PAD>(int N, Scalar<T, PAD> *s);
  friend void clause_scalar_keepMin<T, PAD>(int N, Scalar<T, PAD> *s);
  friend void clause_scalar_keepMax<T, PAD>(int N, Scalar<T, PAD> *s);

  Scalar(T x) : container_{ x } {}

  Scalar() : Scalar(T{}) {}

  INLINE void add(T x) {
    size_t k = 0;
    auto _p = noelle_pragma_begin("ldtc",
                                  &k,
                                  (size_t)0,
                                  clause_scalar_add<T, PAD>,
                                  this);
    auto &lane = container_[k * PAD];
    lane += x;
    noelle_pragma_end(_p);
  }

  INLINE void min(T x) {
    size_t k = 0;
    auto _p = noelle_pragma_begin("ldtc",
                                  &k,
                                  (size_t)0,
                                  clause_scalar_min<T, PAD>,
                                  this);
    auto &lane = container_[k * PAD];
    lane = (lane < x) ? lane : x;
    noelle_pragma_end(_p);
  }

  INLINE void max(T x) {
    size_t k = 0;
    auto _p = noelle_pragma_begin("ldtc",
                                  &k,
                                  (size_t)0,
                                  clause_scalar_max<T, PAD>,
                                  this);
    auto &lane = container_[k * PAD];
    lane = (lane < x) ? x : lane;
    noelle_pragma_end(_p);
  }

  void set(T x) {
    std::memset(container_.data(), 0x0, container_.size() * sizeof(T));
    auto &lane0 = container_[0];
    lane0 = x;
  }

  bool keepMin(T new_value) {
    int k;
    auto p = noelle_pragma_begin("ldtc",
                                 &k,
                                 (int)0,
                                 clause_scalar_keepMin<T, PAD>,
                                 this);
    auto *lane_addr = &container_[0];
    auto current_lane = *lane_addr;
    while (current_lane > new_value) {
      if (__sync_bool_compare_and_swap(lane_addr, current_lane, new_value)) {
        return true;
      }
      current_lane = *lane_addr;
    }
    noelle_pragma_end(p);
    return false;
  }

  bool keepMax(T new_value) {
    int k;
    auto p = noelle_pragma_begin("ldtc",
                                 &k,
                                 (int)0,
                                 clause_scalar_keepMax<T, PAD>,
                                 this);
    auto *lane_addr = &container_[0];
    auto current_lane = *lane_addr;
    while (current_lane < new_value) {
      if (__sync_bool_compare_and_swap(lane_addr, current_lane, new_value)) {
        return true;
      }
      current_lane = *lane_addr;
    }
    noelle_pragma_end(p);
    return false;
  }

  INLINE operator T() {
    return get();
  }

  T get() const {
    const auto lane0 = container_[0];
    T acc = lane0;
    const size_t P = container_.size() / PAD;
    // #pragma omp parallel for reduction(+ : acc) // not worth it
    for (size_t i = 1; i < P; i++) {
      const auto lane = container_[i * PAD];
      acc += lane;
    }
    return acc;
  }

  INLINE T stale_read() const {
    int k;
    auto p = noelle_pragma_begin("ldtc", &k, (int)0);
    auto &lane = container_[k * PAD];
    noelle_pragma_end(p);
    return lane;
  }

  INLINE void stale_write(T x) {
    int k;
    auto p = noelle_pragma_begin("ldtc", &k, (int)0);
    auto &lane = container_[k * PAD];
    lane = std::move(x);
    noelle_pragma_end(p);
  }

  INLINE T __stale_read(int k) const {
    const auto &lane = container_[k * PAD];
    return lane;
  }

  INLINE void __stale_write(int k, T x) {
    auto &lane = container_[k * PAD];
    lane = std::move(x);
  }

  INLINE void operator++() {
    add(1);
  }

  INLINE void operator+=(T x) {
    add(x);
  }

  INLINE void __add(size_t k, T x) {
    auto &lane = container_[k * PAD];
    lane += x;
  }

  INLINE void __min(size_t k, T x) {
    auto &lane = container_[k * PAD];
    lane = (lane < x) ? lane : x;
  }

  INLINE void __max(size_t k, T x) {
    auto &lane = container_[k * PAD];
    lane = (lane < x) ? x : lane;
  }

  void reduce() {
    auto &lane0 = container_[0];
    lane0 = get();
    container_.resize(1 * PAD);
  }

  void reduce_min() {
    auto &lane0 = container_[0];
    const size_t P = container_.size() / PAD;
    for (size_t i = 1; i < P; i++) {
      const auto lane = container_[i * PAD];
      lane0 = (lane0 < lane) ? lane0 : lane;
    }
    container_.resize(1 * PAD);
  }

  void reduce_max() {
    auto &lane0 = container_[0];
    const size_t P = container_.size() / PAD;
    for (size_t i = 1; i < P; i++) {
      const auto lane = container_[i * PAD];
      lane0 = (lane0 < lane) ? lane : lane0;
    }
    container_.resize(1 * PAD);
  }

  void printInternals() const {
    std::cout << "{ ";
    for (auto lane : container_) {
      std::cout << lane << " ";
    }
    std::cout << "}\n";
  }

  // private:
  std::vector<T> container_;
};

} // namespace skynet
