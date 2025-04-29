#pragma once

#include <type_traits>

#include "Common.hpp"
#include "arcana/noelle/core/Pragma.h"

namespace skynet {

template <typename T, bool Order = true>
class IV {
public:
  IV(T value) : value_(value) {
    auto p = noelle_pragma_begin("ldtc");
    noelle_pragma_end(p);
  }

  IV(const IV &other) : value_(other.value_) {
    auto p = noelle_pragma_begin("ldtc");
    noelle_pragma_end(p);
  }

  IV operator=(T iv) = delete;

  template <typename U>
  typename std::enable_if_t<std::is_arithmetic_v<U>, IV>::type operator+(
      U offset) {
    return { value_ + offset };
  }

  void operator++() {
    auto p = noelle_pragma_begin("ldtc");
    value_++;
    noelle_pragma_end(p);
  }

  void operator--() {
    auto p = noelle_pragma_begin("ldtc");
    value_--;
    noelle_pragma_end(p);
  }

  IV &operator+=(T delta) {
    value_ += delta;
    return *this;
  }

  auto operator*() const {
    auto p = noelle_pragma_begin("ldtc");
    auto &v = *this;
    noelle_pragma_end(p);
    return v;
  }

  operator T() {
    auto p = noelle_pragma_begin("ldtc");
    auto v = value_;
    noelle_pragma_end(p);
    return v;
  }

  operator const T() const {
    auto p = noelle_pragma_begin("ldtc");
    auto v = value_;
    noelle_pragma_end(p);
    return v;
  }

  bool operator!=(const IV other) const {
    auto p = noelle_pragma_begin("ldtc");
    bool v = value_ != other.value_;
    noelle_pragma_end(p);
    return v;
  }

private:
  T value_;
};

template <typename T>
using UIV = IV<T, /*Order=*/false>;

} // namespace skynet
