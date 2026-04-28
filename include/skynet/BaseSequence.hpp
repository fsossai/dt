#pragma once

#include <atomic>
#include <cassert>
#include <cstddef>
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
#include "arcana/noelle/core/PragmaDecl.h"

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

template <typename T, uint32_t PAD>
void clause_bag_iterator_erase(
    int N,
    typename BaseSequence<T, /*Order=*/false, PAD>::Iterator *it) {
  // nothing to do
}

template <typename T, uint32_t PAD>
void clause_bag_iterator_op_plusplus(
    int N,
    typename BaseSequence<T, /*Order=*/false, PAD>::Iterator *it) {
  it->idxs_.resize(N * PAD);
  assert(it->base_->container_.size() == (N * PAD));
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
      : idxs_({ idx }),
        base_(base) {
      idxs_.resize(PAD);
    }

    T &__op_star(int t) {
      assert(idxs_[t * PAD] < base_->container_[t * PAD].size());
      return base_->container_[t * PAD][idxs_[t * PAD]];
    }

    T &operator*() {
      int k = 0;
      auto p = noelle_pragma_begin("ldtc", &k, 0);
      auto &val = __op_star(k);
      noelle_pragma_end(p);
      return val;
    }

    Iterator __op_plusplus(int t) {
      ++idxs_[t * PAD];
      assert(idxs_[t * PAD] <= base_->container_[t * PAD].size());
      return *this;
    }

    Iterator &operator++() {
      int k = 0;
      auto p = noelle_pragma_begin("ldtc",
                                   &k,
                                   0,
                                   clause_bag_iterator_op_plusplus<T, PAD>,
                                   this);
      __op_plusplus(k);
      noelle_pragma_end(p);
      return *this;
    }

    Iterator &operator+=(size_t delta) {
      idxs_[0] += delta;
      return *this;
    }

    bool __op_neq(int t, const Iterator & /*other*/) const {
      assert(idxs_[t * PAD] <= base_->container_[t * PAD].size());
      return idxs_[t * PAD] != base_->container_[t * PAD].size();
    }

    bool operator!=(const Iterator &other) const {
      int k = 0;
      auto p = noelle_pragma_begin("ldtc", &k, 0);
      bool val = __op_neq(k, other);
      noelle_pragma_end(p);
      return val;
    }

    int64_t operator-(const Iterator &other) const {
      return (int64_t)idxs_[0] - (int64_t)other.idxs_[0];
    }

    Iterator operator+(int64_t a) const {
      return { base_, idxs_[0] + a };
    }

    template <typename R = void>
    typename std::enable_if_t<!Order, R> __erase(int t) {
      auto &subc = base_->container_[t * PAD];
      assert(idxs_[t * PAD] < subc.size());

      // swap-and-pop idiom
      // not sure about the following std::move
      subc[idxs_[t * PAD]] = std::move(subc[subc.size() - 1]);
      subc.pop_back();
    }

    template <typename R = void>
    typename std::enable_if_t<!Order, R> erase() {
      int k = 0;
      auto p = noelle_pragma_begin("ldtc",
                                   &k,
                                   0,
                                   clause_bag_iterator_erase<T, PAD>,
                                   this);
      __erase(k);
      noelle_pragma_end(p);
    }

    // private:
    std::vector<size_t> idxs_;
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

  BaseSequence &operator=(const BaseSequence &other) {
    if (this == &other) {
      return *this;
    }

    const auto M = other.container_.size();
    container_.resize(M);
    const size_t P = M / PAD;
#pragma omp parallel for
    for (size_t i = 0; i < P; i++) {
      container_[i * PAD] = other.container_[i * PAD];
    }
    return *this;
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
  typename std::enable_if_t<Order, R> __append_ref(int t, T &value) {
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
  typename std::enable_if_t<Order, R> INLINE append_ref(T &value) {
    int k = (container_.size() / PAD) - 1;
    auto _p = noelle_pragma_begin("ldtc",
                                  &k,
                                  0,
                                  clause_sequence_append<T, Order, PAD>,
                                  this);
    __append_ref(k, value);
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
  typename std::enable_if_t<!Order, R> __insert_ref(int t, T &value) {
    container_[t * PAD].push_back(value);
  }

  template <typename R = void>
  typename std::enable_if_t<!Order, R> INLINE insert_ref(T &value) {
    int k = (container_.size() / PAD) - 1;
    auto _p = noelle_pragma_begin("ldtc",
                                  &k,
                                  0,
                                  clause_sequence_append<T, Order, PAD>,
                                  this);
    __insert_ref(k, value);
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

  const T &operator[](size_t idx) const {
    auto c = getCoordinates(idx);
    return container_[c.first * PAD][c.second];
  }

  T &operator[](size_t idx) {
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
  static constexpr uint32_t pad_ = PAD;

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
