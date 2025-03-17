#pragma once

#include <atomic>
#include <iostream>
#include <type_traits>
#include <vector>

#include "HSequence.hpp"
#include "Interface.hpp"
#include "arcana/noelle/core/Pragma.h"

namespace skynet {

template <class T>
class Array {
public:
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
    container_ = new T[size];
  }

  Array(size_t size, T default_value) : Array(size) {
    fill(default_value);
  }

  ~Array() {
    delete container_;
  }

  __attribute__((always_inline)) void set(size_t idx, T value) {
    container_[idx] = value;
  }

  bool update_if(size_t idx, T old_value, T new_value) {
    auto p = noelle_pragma_begin("ldtc");
    auto *addr = &container_[idx];
    if (*addr == old_value) {
      if (__sync_bool_compare_and_swap(addr, old_value, new_value)) {
        return true;
      }
    }
    noelle_pragma_end(p);
    return false;
  }

  void fill(T value) {
#pragma omp parallel for
    for (size_t i = 0; i < size_; i++) {
      container_[i] = value;
    }
  }

  __attribute__((always_inline)) void add(size_t idx, T value) {
    auto p = noelle_pragma_begin("ldtc");
    __atomic_fetch_add(&container_[idx], value, __ATOMIC_RELAXED);
    noelle_pragma_end(p);
  }

  T operator[](size_t idx) {
    return container_[idx];
  }

  const T &operator[](size_t idx) const {
    return container_[idx];
  }

  T stale_read(size_t idx) {
    auto p = noelle_pragma_begin("ldtc");
    return container_[idx];
    noelle_pragma_end(p);
  }

  void stale_write(size_t idx, T value) {
    auto p = noelle_pragma_begin("ldtc");
    return container_[idx] = value;
    noelle_pragma_end(p);
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

  // private:
  // std::vector<T> container_;
  T *container_;
  size_t size_;
};

} // namespace skynet
