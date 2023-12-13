#ifndef __SKYNET__
#define __SKYNET__

#include <vector>

#include "dti.h"

namespace skynet {

template<typename T>
class Sequence {
public:
  using ContainerType = std::vector<T>;

  Sequence() = default;

  void append(const T &value) {
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

  typename ContainerType::iterator begin() {
    return container_.begin();
  }

  typename ContainerType::iterator end() {
    return container_.end();
  }

private:
  ContainerType container_;
};

}

#endif // __SKYNET__
