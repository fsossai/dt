#pragma once

#include <atomic>
#include <iostream>
#include <type_traits>
#include <vector>

#include "Interface.hpp"
#include "IV.hpp"
#include "Range.hpp"

#include "arcana/noelle/core/Pragma.h"

namespace skynet {

template <class T>
class Array {
public:
  class Iterator {
  public:
    Iterator(Array<T> *base, int idx) : idx_(idx), base_(base) {}

    T operator*() {
      return (*base_)[idx_];
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

  class ConstIterator {
  public:
    ConstIterator(const Array<T> *base, int idx) : idx_(idx), base_(base) {}

    T operator*() const {
      return (*base_)[idx_];
    }

    const ConstIterator &operator++() {
      idx_++;
      return *this;
    }

    bool operator!=(const ConstIterator &other) const {
      return idx_ != other.idx_;
    }

  private:
    int idx_;
    const Array<T> *base_;
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

  INLINE void set(size_t idx, T value) {
    container_[idx] = value;
  }

  template <typename U, bool Order>
  INLINE void set(IV<U, Order> idx, T value) {
    auto p = noelle_pragma_begin("ldtc");
    container_[idx] = value;
    noelle_pragma_end(p);
  }

  bool update_if_eq(size_t idx, T old_value, T new_value) {
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

  bool keepMin(size_t idx, T new_value) {
    auto p = noelle_pragma_begin("ldtc");

    auto *addr = &container_[idx];
    auto current_value = *addr;
    while (current_value > new_value) {
      if (__atomic_compare_exchange_n(addr,
                                      &current_value,
                                      new_value,
                                      true,
                                      __ATOMIC_RELAXED,
                                      __ATOMIC_RELAXED)) {
        return true;
      }
    }

    // auto *addr = &container_[idx];
    // auto current_value = *addr;
    // while (current_value > new_value) {
    //   if (__sync_bool_compare_and_swap(addr, current_value, new_value)) {
    //     return true;
    //   }
    //   current_value = *addr;
    // }

    noelle_pragma_end(p);
    return false;
  }

  void fill(T value) {
#pragma omp parallel for
    for (size_t i = 0; i < size_; i++) {
      container_[i] = value;
    }
  }

  INLINE void add(size_t idx, T value) {
    auto p = noelle_pragma_begin("ldtc");
    __atomic_fetch_add(&container_[idx], value, __ATOMIC_RELAXED);
    noelle_pragma_end(p);
  }

  template <typename U, bool IOrder>
  INLINE void add(IV<U, IOrder> idx, T value) {
    auto p = noelle_pragma_begin("ldtc");
    container_[idx] += value;
    noelle_pragma_end(p);
  }

  INLINE T operator[](size_t idx) {
    return container_[idx];
  }

  template <typename U>
  INLINE T operator[](IV<U> idx) {
    auto p = noelle_pragma_begin("ldtc");
    auto x = container_[idx];
    noelle_pragma_end(p);
    return x;
  }

  template <typename U>
  INLINE const T &operator[](IV<U> idx) const {
    auto p = noelle_pragma_begin("ldtc");
    auto x = container_[idx];
    noelle_pragma_end(p);
    return x;
  }

  INLINE const T &operator[](size_t idx) const {
    return container_[idx];
  }

  INLINE T stale_read(size_t idx) {
    auto p = noelle_pragma_begin("ldtc");
    auto x = container_[idx];
    noelle_pragma_end(p);
    return x;
  }

  INLINE void stale_write(size_t idx, T value) {
    auto p = noelle_pragma_begin("ldtc");
    container_[idx] = value;
    noelle_pragma_end(p);
  }

  Range<size_t> getRange() {
    return { 0, size() };
  }

  URange<size_t> getURange() {
    return { 0, size() };
  }

  Iterator begin() {
    return Iterator(this, 0);
  }

  Iterator end() {
    return Iterator(this, size());
  }

  ConstIterator begin() const {
    return ConstIterator(this, 0);
  }

  ConstIterator end() const {
    return ConstIterator(this, size());
  }

  bool empty() const {
    return size() == 0;
  }

  size_t size() const {
    return size_;
  }

  // private:
  T *container_;
  size_t size_;
};

} // namespace skynet
