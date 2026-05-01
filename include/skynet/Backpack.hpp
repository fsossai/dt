#pragma once

#include <algorithm>
#include <atomic>
#include <cassert>
#include <omp.h>
#include <sys/types.h>

#include "Common.hpp"
#include "Range.hpp"
#include "Array.hpp"

#include "arcana/noelle/core/Pragma.h"

namespace skynet {

template <typename T>
class Backpack {
public:
  Backpack(size_t N = 4000) : container_(new T[N]), N_(N), size_(0) {}

  Backpack(const Backpack &other)
    : container_(new T[other.N_]),
      N_(other.N_),
      size_(other.size_) {
    std::copy(other.container_, other.container_ + other.N_, container_);
  }

  Backpack(Backpack &&other) noexcept
    : container_(other.container_),
      N_(other.N_),
      size_(other.size_) {
    other.container_ = nullptr;
    other.N_ = 0;
    other.size_ = 0;
  }

  Backpack &operator=(const Backpack &other) {
    if (this == &other) {
      return *this;
    }

    T *new_container = new T[other.N_];
    std::copy(other.container_, other.container_ + other.N_, new_container);

    delete[] container_;
    container_ = new_container;
    N_ = other.N_;
    size_ = other.size_;
    return *this;
  }

  Backpack &operator=(Backpack &&other) noexcept {
    if (this == &other) {
      return *this;
    }

    delete[] container_;
    container_ = other.container_;
    N_ = other.N_;
    size_ = other.size_;

    other.container_ = nullptr;
    other.N_ = 0;
    other.size_ = 0;
    return *this;
  }

  ~Backpack() {
    delete[] container_;
  }

  void insert(T value) {
    auto idx = __sync_fetch_and_add(&size_, 1);
    skynet_assert(idx < N_);
    container_[idx] = value;
  }

  void insert_ref(T &value) {
    auto idx = __sync_fetch_and_add(&size_, 1);
    skynet_assert(idx < N_);
    container_[idx] = value;
  }

  void __insert_ref(int /*t*/, T &value) {
    auto idx = __sync_fetch_and_add(&size_, 1);
    skynet_assert(idx < N_);
    container_[idx] = value;
  }

  INLINE Range<size_t, /*Order=*/false> getRange() {
    return { 0, size_ };
  }

  void clear() {
    size_ = 0;
  }

  bool empty() const {
    return size_ == 0;
  }

  size_t capacity() const {
    return N_;
  }

  size_t size() const {
    return size_;
  }

  T *begin() {
    return container_;
  }

  T *end() {
    return container_ + size_;
  }

  T operator[](size_t idx) const {
    return container_[idx];
  }

  // private:

  T *container_;
  size_t N_;
  size_t size_;
};

} // namespace skynet
