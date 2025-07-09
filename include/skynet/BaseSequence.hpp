#pragma once

#include <atomic>
#include <cassert>
#include <functional>
#include <iostream>
#include <omp.h>
#include <vector>

#include "Common.hpp"
#include "Range.hpp"
#include "Array.hpp"

#include "arcana/noelle/core/Pragma.h"

namespace skynet {

template <typename T, bool Order>
class BaseSequence;

template <typename T, bool Order>
void clause_sequence_append(int N, BaseSequence<T, Order> *seq) {
  if (seq->container_.size() >= N) {
    return;
  }

  auto M = seq->container_[0].capacity();
  int K = seq->container_.size();
  for (int i = 0; i < (N - K); i++) {
    typename BaseSequence<T, Order>::VectorT new_container;
    new_container.reserve(M);
    seq->container_.push_back(std::move(new_container));
  }
}

template <typename T, bool Order>
class BaseSequence {
public:
  friend void clause_sequence_append<T>(int N, BaseSequence<T, Order> *base);

  using VectorT = std::vector<T>;

  class Iterator {
  public:
    Iterator(BaseSequence<T, Order> *base, size_t idx)
      : idx_(idx),
        base_(base) {}

    T operator*() {
      auto c = base_->getCoordinates(idx_);
      return base_->container_[c.first][c.second];
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
    BaseSequence<T, Order> *base_;
  };

  BaseSequence(size_t size) {
    VectorT tmp;
    tmp.reserve(size);
    container_.push_back(std::move(tmp));
  }

  BaseSequence() {
    container_.emplace_back();
  }

  BaseSequence(size_t size, T init) {
    container_.emplace_back(size, init);
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
    const int P = container_.size();
    size_t avg = 0;

    // computing ideal average
    for (auto &row : container_) {
      avg += row.size();
    }
    avg = (avg + P - 1) / P;

    for (int t = 0; t < P - 1; t++) {
      size_t smaller_i = 0;
      size_t bigger_i = 0;
      size_t smaller = container_[0].size();
      size_t bigger = smaller;
      for (int i = 1; i < P; i++) {
        size_t v = container_[i].size();
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
        container_[smaller_i].push_back(
            container_[bigger_i][bigger - delta + i]);
        container_[bigger_i].resize(bigger - delta);
      }
    }
  }

  void reduce() {
    auto &dst = container_[0];
    std::atomic<size_t> offset = dst.size();
    dst.resize(size());

#pragma omp parallel for ordered
    for (size_t i = 1; i < container_.size(); i++) {
      auto &src = container_[i];
      size_t i_start;
      if constexpr (Order) {
#pragma omp ordered
        i_start = offset.fetch_add(src.size());
      } else {
        i_start = offset.fetch_add(src.size());
      }
      std::copy(src.begin(), src.end(), dst.begin() + i_start);
      src.resize(0);
    }
  }

  template <typename R = void>
  typename std::enable_if_t<Order, R> __append(int t, T value) {
    container_[t].push_back(value);
  }

  template <typename R = void>
  typename std::enable_if_t<Order, R> __append(int t,
                                               skynet::Array<T> &values,
                                               size_t N) {
    const int offset = container_[t].size();
    container_[t].resize(offset + N);
    std::copy(values.container_,
              values.container_ + N,
              container_[t].begin() + offset);
  }

  template <typename R = void>
  typename std::enable_if_t<Order, R> INLINE append(T value) {
    int k = container_.size() - 1;
    auto _p = noelle_pragma_begin("ldtc",
                                  &k,
                                  0,
                                  clause_sequence_append<T, Order>,
                                  this);
    __append(k, value);
    noelle_pragma_end(_p);
  }

  template <typename R = void>
  typename std::enable_if_t<Order, R> INLINE append(skynet::Array<T> &values) {
    int k = container_.size() - 1;
    auto _p = noelle_pragma_begin("ldtc",
                                  &k,
                                  0,
                                  clause_sequence_append<T, Order>,
                                  this);
    append(values, values.size());
    noelle_pragma_end(_p);
  }

  template <typename R = void>
  typename std::enable_if_t<Order, R> INLINE append(skynet::Array<T> &values,
                                                    size_t N) {
    int k = container_.size() - 1;
    auto _p = noelle_pragma_begin("ldtc",
                                  &k,
                                  0,
                                  clause_sequence_append<T, Order>,
                                  this);
    __append(k, values, N);
    noelle_pragma_end(_p);
  }

  template <typename R = void>
  typename std::enable_if_t<!Order, R> __insert(int t, T value) {
    container_[t].push_back(value);
  }

  template <typename R = void>
  typename std::enable_if_t<!Order, R> INLINE insert(T value) {
    int k = container_.size() - 1;
    auto _p = noelle_pragma_begin("ldtc",
                                  &k,
                                  0,
                                  clause_sequence_append<T, Order>,
                                  this);
    __insert(k, value);
    noelle_pragma_end(_p);
  }

  template <typename R = void>
  typename std::enable_if_t<Order, R> resize(size_t size) {
    assert(container_.size() == 1); // TODO: this is here for simplicity
    for (auto &row : container_) {
      row.resize(size);
    }
  }

  template <typename R = const T &>
  typename std::enable_if_t<Order, R> operator[](size_t idx) const {
    auto c = getCoordinates(idx);
    return container_[c.first][c.second];
  }

  template <typename R = T &>
  typename std::enable_if_t<Order, R> operator[](size_t idx) {
    auto c = getCoordinates(idx);
    return container_[c.first][c.second];
  }

  template <typename R = const T &, typename U>
  typename std::enable_if_t<!Order, R> operator[](
      const IV<U, /*Order=*/false> idx) const {
    auto c = getCoordinates(idx);
    return container_[c.first][c.second];
  }

  template <typename R = T &, typename U>
  typename std::enable_if_t<!Order, R> operator[](
      const IV<U, /*Order=*/false> idx) {
    auto c = getCoordinates(idx);
    return container_[c.first][c.second];
  }

  void serialize(const std::function<void(T *, size_t)> &writer) {
    for (auto &row : container_) {
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
    return size() == 0;
  }

  size_t size() const {
    size_t sum = 0;
    for (size_t i = 0; i < container_.size(); i++) {
      sum += container_[i].size();
    }
    return sum;
  }

  void printInternals() const {
    for (auto c : container_) {
      std::cout << "> ";
      for (auto x : c) {
        std::cout << x << " ";
      }
      std::cout << "\n";
    }
  }

  void printStats() const {
    size_t s = 0;
    std::printf("[ ");
    for (auto &cell : container_) {
      s += cell.size();
      std::printf("[%zu] ", cell.size());
    }
    std::printf("] (%zu)\n", s);
  }

  void clear() {
#pragma omp parallel for
    for (auto &row : container_) {
      row.clear();
    }
  }

  // private:
  std::vector<VectorT> container_;

  INLINE std::pair<size_t, size_t> getCoordinates(size_t idx) const {
    int i = -1;
    int64_t j = idx;
    do {
      j -= container_[++i].size();
    } while (j >= 0);
    std::pair<size_t, size_t> coord;
    coord.first = i;
    coord.second = container_[i].size() + j;
    return coord;
  }
};

} // namespace skynet
