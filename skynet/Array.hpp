#pragma once

#include <iostream>
#include <vector>

#include "arcana/noelle/core/Pragma.h"

namespace skynet {

template <typename T>
class Array;

template <typename T>
void clause_array_add(int N, Array<T> *array) {
  if (array->container_.size() == N) {
    return;
  }
  array->container_.resize(N);
  for (auto &row : array->container_) {
    row.resize(array->size_);
  }
}

template <class T>
class Array {
public:
  template <typename U>
  using ContainerType = std::vector<U>;

  friend void clause_array_add<T>(int N, Array<T> *base);

  class Iterator {
  public:
    Iterator(Array<T> *base, int idx) : idx_(idx), base_(base) {}

    T operator*() {
      return base_[idx_];
    }

    Iterator &operator++() {
      idx_++;
      return *this;
    }

    bool operator!=(const Iterator &other) const {
      return idx_ != other.idx_;
    }

  private:
    int idx_;
    Array<T> *base_;
  };

  Array(size_t size) : size_(size) {
    container_.emplace_back();
    container_[0].resize(size);
  }

  void set(size_t idx, T value) {
    int k = 0;
    container_[k][idx] = value;
  }

  __attribute__((always_inline)) void add(size_t idx, T value) {
    int k = 0;
    auto _p = noelle_pragma_begin("ldtc", &k, 0, clause_array_add<T>, this);
    container_[k][idx] += value;
    noelle_pragma_end(_p);
  }

  // This function should be a method of `ApatheticArray` that, at the moment is
  // not implemented
  __attribute__((always_inline)) bool replace_if(size_t idx, T pre, T post) {
    auto _p = noelle_pragma_begin("ldtc");
    auto &addr = &container_[0][idx];
    if (*addr == pre) {
      if (__sync_bool_compare_and_swap(addr, pre, post)) {
        return true;
      }
    }
    noelle_pragma_end(_p);
    return false;
  }

  T operator[](size_t idx) {
    T v = container_[0][idx];
    for (int i = 1; i <= container_.size(); i++) {
      v += container_[i][idx];
    }
    return v;
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
    return size_;
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

private:
  std::vector<std::vector<T>> container_;
  size_t size_;
};

} // namespace skynet
