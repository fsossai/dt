#pragma once

#include <cassert>
#include <iostream>
#include <vector>

#include "Common.hpp"
#include "Range.hpp"

#include "arcana/noelle/core/Pragma.h"

namespace skynet {

template <typename T, bool Order>
class BaseSequence;

template <typename T, bool Order>
void clause_sequence_append(int N, BaseSequence<T, Order> *seq) {
  seq->container_.resize(N);
}

template <typename T, bool Order>
class BaseSequence {
public:
  friend void clause_sequence_append<T>(int N, BaseSequence<T, Order> *base);

  template <bool IOrder>
  class Iterator {
  public:
    Iterator(BaseSequence<T, Order> *base, int idx) : idx(idx), base(base) {}

    T operator*() {
      auto c = base->getCoordinates(idx);
      return base->container_[c.first][c.second];
    }

    Iterator<IOrder> &operator++() {
      idx++;
      return *this;
    }

    bool operator!=(const Iterator<IOrder> &other) const {
      return idx != other.idx;
    }

  private:
    int idx;
    BaseSequence<T, Order> *base;
  };

  BaseSequence() {
    container_.emplace_back();
  }

  BaseSequence(size_t size) {
    container_.emplace_back(size);
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

  Range<size_t> getRange() {
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

  template <typename = typename std::enable_if_t<Order>>
  INLINE void __append(int t, T value) {
    container_[t].push_back(value);
  }

  template <typename = typename std::enable_if_t<Order>>
  INLINE void append(T value) {
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
  typename std::enable_if_t<!Order, R> INLINE __insert(T value) {
    int k = container_.size() - 1;
    auto _p = noelle_pragma_begin("ldtc",
                                  &k,
                                  0,
                                  clause_sequence_append<T, Order>,
                                  this);
    container_[k].push_back(value);
    noelle_pragma_end(_p);
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

  Iterator<Order> begin() {
    return { this, 0 };
  }

  Iterator<Order> end() {
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
    for (auto &block : container_) {
      block.clear();
    }
  }

  // private:
  std::vector<std::vector<T>> container_;

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
