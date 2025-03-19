#pragma once

#include <cstdint>
#include "IV.hpp"

namespace skynet {

template <typename T>
class Range {
public:
  Range(T i_start, T i_end) : i_start_(i_start), i_end_(i_end) {}

  IV<T> begin() {
    return IV<T>(i_start_);
  }

  IV<T> end() {
    return IV<T>(i_end_);
  }

private:
  T i_start_;
  T i_end_;
};

} // namespace skynet
