#pragma once

#include <type_traits>

#include "Common.hpp"

namespace skynet {

template <typename T, bool Order = true>
class IV {
public:
  IV(T value) : value_(value) {}

  IV(const IV &other) : value_(other.value_) {}

  IV operator=(T iv) = delete;

  template <typename U>
  typename std::enable_if_t<std::is_arithmetic_v<U>, IV>::type operator+(
      U offset) {
    return { value_ + offset };
  }

  void operator++() {
    value_++;
  }

  void operator--() {
    value_--;
  }

  auto operator*() const {
    return *this;
  }

  operator T() {
    return value_;
  }

  operator const T() const {
    return value_;
  }

  bool operator!=(const IV other) const {
    return value_ != other.value_;
  }

private:
  T value_;
};

template <typename T>
using UIV = IV<T, /*Order=*/false>;

} // namespace skynet
