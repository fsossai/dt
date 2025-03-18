#pragma once

#include <cassert>
#include <iostream>
#include <oneapi/tbb/concurrent_vector.h>
#include <vector>
#include <oneapi/tbb.h>
#include <atomic>

#include "arcana/noelle/core/Pragma.h"

namespace skynet {

template <class T>
class PSequence {
public:
  class Iterator {
  public:
    Iterator(PSequence<T> *base, int idx) : idx(idx), base(base) {}

    T operator*() {
      return base->container_[idx];
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
    PSequence<T> *base;
  };

  PSequence() {}

  PSequence(size_t size) {
    container_.reserve(size);
  }

  PSequence(size_t size, T init) : container_(size, init) {}

  void fill(T value) {
#pragma omp parallel for
    for (auto &x : container_) {
      x = value;
    }
  }

  void append(T value) {
    auto p = noelle_pragma_begin("ldtc");
    std::lock_guard<std::mutex> g(mutex_);
    container_.push_back(value);
    noelle_pragma_end(p);
  }

  __attribute__((always_inline)) void push_back(T value) {
    append(value);
  }

  T operator[](size_t idx) {
    return container_[idx];
  }

  const T &operator[](size_t idx) const {
    return container_[idx];
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
    return container_.size();
  }

  void print() const {
    std::cout << "> ";
    for (auto x : container_) {
      std::cout << x << " ";
    }
    std::cout << "\n";
  }

  void clear() {
    container_.clear();
  }

  // private:
  std::vector<T> container_;
  std::mutex mutex_;
};

} // namespace skynet
