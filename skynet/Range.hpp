#pragma once

#include <cstdint>

#include "Common.hpp"
#include "IV.hpp"

namespace skynet {

template <typename T, bool Order = true>
class Range {
public:
  using RangeIV = IV<T, Order>;

  Range(T i_start, T i_end) : i_start_(i_start), i_end_(i_end) {}

  RangeIV begin() {
    return { i_start_ };
  }

  RangeIV end() {
    return { i_end_ };
  }

private:
  T i_start_;
  T i_end_;
};

template<typename T>
using URange = Range<T, /*Order=*/false>;

} // namespace skynet
