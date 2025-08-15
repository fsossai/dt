#pragma once

#include <atomic>
#include <cassert>
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

namespace skynet {

template <typename T>
class Backpack {
public:
  Backpack(size_t N) : N_(N), size_(0) {
    container_ = (T *)malloc(sizeof(T) * N);
  }

  ~Backpack() {
    free(container_);
  }

  template <typename R = void>
  std::enable_if_t<sizeof(T) >= 8, R> push_back(const T &value) {
    container_[__sync_fetch_and_add(&size_, 1)] = value;
  }

  template <typename R = void>
  std::enable_if_t<sizeof(T) < 8, R> push_back(T value) {
    container_[__sync_fetch_and_add(&size_, 1)] = value;
  }

  void clear() {
    size_ = 0;
  }

  bool empty() {
    return size_ == 0;
  }

  size_t capacity() {
    return N_;
  }

  size_t size() {
    return size_;
  }

  T *begin() {
    return container_;
  }

  T *end() {
    return container_ + size_;
  }

  // private:

  T *container_;
  size_t N_;
  size_t size_;
};

} // namespace skynet