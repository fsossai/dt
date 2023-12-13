#ifndef __SKYNET__
#define __SKYNET__

#include <vector>
#include <iostream>

#include "dti.h"

namespace skynet {

int next_k() {
  return 0;
}

template<typename T>
class Sequence {
public:
  template<typename U>
  using ContainerType = std::vector<U>;

  Sequence() = default;


  __attribute__((always_inline))
  void append(const T &value) {
    int k = 0;
    PRAGMA_LDTC(k, next_k);
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
};

}

#endif // __SKYNET__
