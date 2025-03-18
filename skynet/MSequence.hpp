#pragma once

#include <cassert>
#include <iostream>
#include <oneapi/tbb/concurrent_vector.h>
#include <vector>
#include <oneapi/tbb.h>
#include "PSequence.hpp"

#include "arcana/noelle/core/Pragma.h"

namespace skynet {

template <typename T>
class MSequence;

template <typename T>
void clause_msequence_append(int N, MSequence<T> *seq) {
  int K = (N + seq->M_ - 1) / seq->M_;
  seq->container_.resize(K);
}

template <class T>
class MSequence {
public:
  // using subcontainer_t = std::vector<T>;
  // using subcontainer_t = oneapi::tbb::concurrent_vector<T>;
  using subcontainer_t = PSequence<T>;

  friend void clause_msequence_append<T>(int N, MSequence<T> *base);

  class Iterator {
  public:
    Iterator(MSequence<T> *base, int idx) : idx(idx), base(base) {}

    T operator*() {
      auto coord = base->getCoordinate(idx);
      return base->container_[coord.first][coord.second];
    }

    Iterator &operator++() {
      idx++;
      return *this;
    }

    bool operator!=(const Iterator &other) const {
      return idx != other.idx;
    }

  private:
    int idx;
    MSequence<T> *base;
  };

  MSequence() : k_(0), M_(1) {
    container_.emplace_back();
  }

  MSequence(size_t size) : k_(0), M_(1) {
    container_.emplace_back((size + M_ - 1) / M_);
  }

  MSequence(size_t size, T init) : k_(0) {
    container_.emplace_back((size + M_ - 1) / M_, init);
  }

  void setSharing(int M) {
    assert(M >= 1);
    M_ = M;
  }

  void fill(T value) {
    for (auto &subc : container_) {
#pragma omp parallel for
      for (auto &x : subc) {
        x = value;
      }
    }
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

  __attribute__((always_inline)) void append(T value) {
    int k = container_.size() - 1;
    auto _p =
        noelle_pragma_begin("ldtc", &k, 0, clause_msequence_append<T>, this);
    container_[k / M_].push_back(value);
    noelle_pragma_end(_p);
  }

  void __append(int t, T value) {
    container_[t / M_].push_back(value);
  }

  T operator[](size_t idx) const {
    int i = -1;
    int64_t j = idx;
    do {
      j -= container_[++i].size();
    } while (j >= 0);
    return container_[i][container_[i].size() + j];
  }

  T &operator[](size_t idx) {
    int i = -1;
    int64_t j = idx;
    do {
      j -= container_[++i].size();
    } while (j >= 0);
    return container_[i][container_[i].size() + j];
  }

  Iterator begin() {
    return Iterator(this, 0);
  }

  Iterator end() {
    return Iterator(this, size());
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
  std::vector<subcontainer_t> container_;

  std::pair<size_t, size_t> getCoordinate(size_t idx) const {
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

  int k_;
  int M_;
};

} // namespace skynet
