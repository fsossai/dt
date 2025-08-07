#pragma once

#include <atomic>
#include <cassert>
#include <cstdint>
#include <functional>
#include <iostream>
#include <omp.h>
#include <sys/types.h>
#include <vector>

#include "Common.hpp"
#include "Range.hpp"
#include "Array.hpp"

#include "arcana/noelle/core/Pragma.h"

namespace skynet {

template <typename T>
using Vector = std::vector<T>;

template <typename T, bool Order, uint32_t PAD = compute_padding<Vector<T>>()>
class BaseSequence;

template <typename T, bool Order, uint32_t PAD>
void clause_sequence_append(int N, BaseSequence<T, Order, PAD> *seq) {
  if ((seq->container_.size() / PAD) >= N) {
    return;
  }

  auto M = seq->container_[0].capacity();
  int K = seq->container_.size() / PAD;
  for (int i = 0; i < (N - K); i++) {
    typename BaseSequence<T, Order, PAD>::VectorT new_container;
    new_container.reserve(M);
    seq->container_.push_back(std::move(new_container));
    for (int i = 0; i < PAD - 1; i++) {
      seq->container_.emplace_back();
    }
  }
}

template <typename T, bool Order, uint32_t PAD>
class BaseSequence {
public:
  friend void clause_sequence_append<T, Order, PAD>(
      int N,
      BaseSequence<T, Order, PAD> *base);

  using VectorT = Vector<T>;
  class Iterator {
  public:
    Iterator(BaseSequence<T, Order, PAD> *base, size_t idx)
      : idx_(idx),
        base_(base) {}

    T operator*() {
      auto c = base_->getCoordinates(idx_);
      return base_->container_[c.first * PAD][c.second];
    }

    Iterator &operator++() {
      idx_++;
      return *this;
    }

    Iterator &operator+=(size_t delta) {
      idx_ += delta;
      return *this;
    }

    bool operator!=(const Iterator &other) const {
      return idx_ != other.idx_;
    }

    int64_t operator-(const Iterator &other) const {
      return (int64_t)idx_ - (int64_t)other.idx_;
    }

    Iterator operator+(int64_t a) const {
      return { base_, idx_ + a };
    }

  private:
    size_t idx_;
    BaseSequence<T, Order, PAD> *base_;
  };

  BaseSequence(size_t size) {
    VectorT tmp;
    tmp.reserve(size);
    container_.push_back(std::move(tmp));
    for (int i = 0; i < PAD - 1; i++) {
      container_.emplace_back();
    }
  }

  BaseSequence() {
    for (int i = 0; i < PAD; i++) {
      container_.emplace_back();
    }
  }

  BaseSequence(size_t size, T init) {
    container_.emplace_back(size, init);
    for (int i = 0; i < PAD - 1; i++) {
      container_.emplace_back();
    }
  }

  void fill(T value) {
    for (auto &subc : container_) {
#pragma omp parallel for
      for (auto &x : subc) {
        x = value;
      }
    }
  }

  Range<size_t, Order> getRange() {
    return { 0, size() };
  }

  void rebalance() {
    const int P = container_.size() / PAD;
    size_t avg = 0;

    // computing ideal average
    for (int i = 0; i < container_.size() / PAD; i++) {
      avg += container_[i * PAD].size();
    }
    avg = (avg + P - 1) / P;

    for (int t = 0; t < P - 1; t++) {
      size_t smaller_i = 0;
      size_t bigger_i = 0;
      size_t smaller = container_[0].size();
      size_t bigger = smaller;
      for (int i = 1; i < P; i++) {
        size_t v = container_[i * PAD].size();
        if (v < smaller) {
          smaller = v;
          smaller_i = i;
        }
        if (v > bigger) {
          bigger = v;
          bigger_i = i;
        }
      }

      // copy
      auto delta = std::min<size_t>(avg - smaller, bigger - avg);
      for (size_t i = 0; i < delta; i++) {
        container_[smaller_i * PAD].push_back(
            container_[bigger_i * PAD][bigger - delta + i]);
        container_[bigger_i * PAD].resize(bigger - delta);
      }
    }
  }

  void reduce_seq() {
    const size_t P = container_.size() / PAD;

    // prefix sum
    std::vector<size_t> offset(P + 1, 0);
    offset[0] = 0;
    for (size_t i = 0; i < P; ++i) {
      offset[i + 1] = offset[i] + container_[i * PAD].size();
    }

    auto &dst = container_[0];
    dst.resize(offset[P]);
    for (size_t i = 1; i < P; i++) {
      auto &src = container_[i * PAD];
      std::copy(src.begin(), src.end(), dst.begin() + offset[i]);
      src.clear();
    }
  }

  void reduce_par() {
    const size_t P = container_.size() / PAD;

    // prefix sum
    std::vector<size_t> offset(P + 1, 0);
    offset[0] = 0;
    for (size_t i = 0; i < P; ++i) {
      offset[i + 1] = offset[i] + container_[i * PAD].size();
    }

    auto &dst = container_[0];
    dst.resize(offset[P]);
#pragma omp parallel for
    for (size_t i = 1; i < P; i++) {
      auto &src = container_[i * PAD];
      std::copy(src.begin(), src.end(), dst.begin() + offset[i]);
      src.clear();
    }
  }

  template <typename R = void>
  typename std::enable_if_t<Order, R> __append(int t, T value) {
    container_[t * PAD].push_back(value);
  }

  template <typename R = void>
  typename std::enable_if_t<Order, R> __append(int t, T *values, size_t N) {
    const int offset = container_[t * PAD].size();
    container_[t * PAD].resize(offset + N);
    std::copy(values, values + N, container_[t * PAD].begin() + offset);
  }

  template <typename R = void>
  typename std::enable_if_t<Order, R> __append(int t,
                                               skynet::Array<T> &values,
                                               size_t N) {
    __append(t, values.container_, N);
  }

  template <typename R = void>
  typename std::enable_if_t<Order, R> INLINE append(T value) {
    int k = (container_.size() / PAD) - 1;
    auto _p = noelle_pragma_begin("ldtc",
                                  &k,
                                  0,
                                  clause_sequence_append<T, Order, PAD>,
                                  this);
    __append(k, value);
    noelle_pragma_end(_p);
  }

  template <typename R = void>
  typename std::enable_if_t<Order, R> INLINE append(skynet::Array<T> &values) {
    int k = (container_.size() / PAD) - 1;
    auto _p = noelle_pragma_begin("ldtc",
                                  &k,
                                  0,
                                  clause_sequence_append<T, Order, PAD>,
                                  this);
    append(values, values.size());
    noelle_pragma_end(_p);
  }

  template <typename R = void>
  typename std::enable_if_t<Order, R> INLINE append(skynet::Array<T> &values,
                                                    size_t N) {
    int k = (container_.size() / PAD) - 1;
    auto _p = noelle_pragma_begin("ldtc",
                                  &k,
                                  0,
                                  clause_sequence_append<T, Order, PAD>,
                                  this);
    __append(k, values, N);
    noelle_pragma_end(_p);
  }

  template <typename R = void>
  typename std::enable_if_t<!Order, R> __insert(int t, T value) {
    container_[t * PAD].push_back(value);
  }

  template <typename R = void>
  typename std::enable_if_t<!Order, R> INLINE insert(T value) {
    int k = (container_.size() / PAD) - 1;
    auto _p = noelle_pragma_begin("ldtc",
                                  &k,
                                  0,
                                  clause_sequence_append<T, Order, PAD>,
                                  this);
    __insert(k, value);
    noelle_pragma_end(_p);
  }

  template <typename R = void>
  typename std::enable_if_t<Order, R> resize(size_t size) {
    for (size_t i = 0; i < container_.size() / PAD; i++) {
      container_[i * PAD].resize(size);
    }
  }

  template <typename R = const T &>
  typename std::enable_if_t<Order, R> operator[](size_t idx) const {
    auto c = getCoordinates(idx);
    return container_[c.first * PAD][c.second];
  }

  template <typename R = T &>
  typename std::enable_if_t<Order, R> operator[](size_t idx) {
    auto c = getCoordinates(idx);
    return container_[c.first * PAD][c.second];
  }

  template <typename R = const T &, typename U>
  typename std::enable_if_t<!Order, R> operator[](
      const IV<U, /*Order=*/false> idx) const {
    auto c = getCoordinates(idx);
    return container_[c.first * PAD][c.second];
  }

  template <typename R = T &, typename U>
  typename std::enable_if_t<!Order, R> operator[](
      const IV<U, /*Order=*/false> idx) {
    auto c = getCoordinates(idx);
    return container_[c.first * PAD][c.second];
  }

  void serialize(const std::function<void(T *, size_t)> &writer) {
    const size_t P = container_.size() / PAD;
    for (size_t i = 0; i < P; i++) {
      auto &row = container_[i * PAD];
      writer(row.data(), row.size());
    }
  }

  Iterator begin() {
    return { this, 0 };
  }

  Iterator end() {
    return { this, size() };
  }

  bool empty() const {
    const size_t P = container_.size() / PAD;
    for (size_t i = 0; i < P; i++) {
      if (!container_[i * PAD].empty()) {
        return false;
      }
    }
    return true;
  }

  size_t size() const {
    size_t sum = 0;
    const size_t P = container_.size() / PAD;
    for (size_t i = 0; i < P; i++) {
      sum += container_[i * PAD].size();
    }
    return sum;
  }

  void printInternals() const {
    for (int i = 0; i < container_.size() / PAD; i++) {
      std::cout << "> ";
      for (auto x : container_[i * PAD]) {
        std::cout << x << " ";
      }
      std::cout << "\n";
    }
  }

  void printStats() const {
    size_t s = 0;
    std::printf("[ ");
    for (int i = 0; i < container_.size() / PAD; i++) {
      auto &row = container_[i * PAD];
      s += row.size();
      std::printf("[%zu] ", row.size());
    }
    std::printf("] (%zu)\n", s);
  }

  void clear() {
    // #pragma omp parallel for
    const size_t P = container_.size() / PAD;
    for (int i = 0; i < P; i++) {
      container_[i * PAD].clear();
    }
  }

  // private:
  std::vector<VectorT> container_;
  uint32_t pad_ = PAD;

  INLINE std::pair<size_t, size_t> getCoordinates(size_t idx) const {
    int i = -1;
    int64_t j = idx;
    do {
      ++i;
      j -= container_[i * PAD].size();
    } while (j >= 0);
    std::pair<size_t, size_t> coord;
    coord.first = i;
    coord.second = container_[i * PAD].size() + j;
    return coord;
  }
};

} // namespace skynet
