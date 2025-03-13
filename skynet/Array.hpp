#pragma once

#include <iostream>
#include <type_traits>
#include <vector>

#include "HSequence.hpp"
#include "Interface.hpp"
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

template <typename T>
int clause_array_add2(int N, Array<T> *array, int M, int offset = 0) {
  auto p =
      noelle_pragma_begin("ldtc", &offset, clause_array_add2<T>, array, N * M);
  if (N * M > array->container_.size()) {
    array->container_.resize(N * M);
    for (auto &row : array->container_) {
      row.resize(array->size_);
    }
  }
  int retVal = (offset + tc_chain_id()) * N;
  noelle_pragma_end(p);
  return retVal;
}

template <class T>
class Array {
public:
  template <typename U>
  using ContainerType = std::vector<U>;

  friend void clause_array_add<T>(int N, Array<T> *base);
  friend int clause_array_add2<T>(int N, Array<T> *base, int M, int offset);

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

  Array(size_t size, bool init = true) : size_(size) {
    container_.emplace_back();
    if (init) {
      container_[0].resize(size);
    } else {
      container_[0].reserve(size);
    }
  }

  void set(size_t idx, T value) {
    int k = 0;
    container_[k][idx] = value;
  }

  void fill(T value) {
#pragma omp parallel for
    for (auto &subc : container_) {
      for (auto &x : subc) {
        x = value;
      }
    }
  }

  __attribute__((always_inline)) void add(size_t idx, T value) {
    int k = 0;
    auto _p = noelle_pragma_begin("ldtc", &k, 0, clause_array_add<T>, this);
    container_[k][idx] += value;
    noelle_pragma_end(_p);
  }

  void __add(int t, size_t idx, T value) {
    container_[t][idx] += value;
  }

  void add2(size_t idx, T value, int offset = 0) {
    auto p =
        noelle_pragma_begin("ldtc", &offset, clause_array_add2<T>, this, 1);
    container_[offset + tc_chain_id()][idx] += value;
    noelle_pragma_end(p);
  }

  void lean_add_bypass(std::vector<T> *container, size_t idx, T value) {
    auto p = noelle_pragma_begin("ldtc");
    __atomic_fetch_add(&(*container)[idx], value, __ATOMIC_RELAXED);
    noelle_pragma_end(p);
  }

  void lean_add(size_t idx, T value) {
    auto p = noelle_pragma_begin("ldtc");
    __atomic_fetch_add(&container_[0][idx], value, __ATOMIC_RELAXED);
    noelle_pragma_end(p);
  }

  // This function should be a method of `ApatheticArray` that, at the moment is
  // not implemented
  __attribute__((always_inline)) bool replace_if_negative(size_t idx,
                                                          T new_val) {
    auto _p = noelle_pragma_begin("ldtc");
    auto *addr = &container_[0][idx];
    auto val = *addr;
    if (val < 0) {
      if (__sync_bool_compare_and_swap(addr, val, new_val)) {
        return true;
      }
    }
    noelle_pragma_end(_p);
    return false;
  }

  T &operator[](size_t idx) {
    T &v = container_[0][idx];
    for (int i = 1; i < container_.size(); i++) {
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

  // private:
  std::vector<std::vector<T>> container_;
  size_t size_;
};

} // namespace skynet
