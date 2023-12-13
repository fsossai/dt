#ifndef __SKYNET__
#define __SKYNET__

#include <vector>
#include <iostream>

#include "dti.h"

namespace skynet {

template<typename T>
class Sequence {
public:
  template<typename U>
  using ContainerType = std::vector<U>;

  Sequence() = default;

  T plus1(T t) const {
    return t+1;
  }

  __attribute__((always_inline))
  void append(T value) {
    PRAGMA_LDTC(value, Sequence<T>::plus1, this, value);
    container_.push_back(value);
  }

  size_t size() const {
    return container_.size();
  }

  T& operator[](size_t idx) {
    return container_[idx];
  }
  
  const T& operator[](size_t idx) const {
    return container_[idx];
  }

  typename ContainerType<T>::iterator begin() {
    return container_.begin();
  }

  typename ContainerType<T>::iterator end() {
    return container_.end();
  }

private:
  ContainerType<T> container_;
  int k_ = 17;
  char c[10];
  std::string ds = "[%i]\n";
};

}

#endif // __SKYNET__
