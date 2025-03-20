#pragma once

#include "Common.hpp"

namespace skynet {

template <typename T>
class IV {
public:
  IV(T iv) : container_(iv) {}

  IV(const IV &other) : container_(other.container_) {}

  IV &operator=(T iv) {
    container_ = iv;
    return *this;
  }

  void operator++() {
    container_++;
  }

  void operator--() {
    container_--;
  }

  T operator*() {
    return container_;
  }

  operator T&() {
    return container_;
  }

  operator const T&() const {
    return container_;
  }

  bool operator!=(const IV &other) {
    return container_ != other.container_;
  }

private:
  T container_;
};

} // namespace skynet
