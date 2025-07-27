#pragma once

#include <cassert>
#include <vector>
#include "Common.hpp"
#include "arcana/noelle/core/Pragma.h"

namespace skynet {

template <typename T>
class StaleObject;

template <typename T>
void clause_stale_object_set(int N, StaleObject<T> *obj) {
#ifdef DEBUG
  std::printf("%s(%i, %p)\n", __func__, N, obj);
#endif
  if (obj->container_.size() == N) {
    return;
  }
  obj->container_.resize(N);
}

template <typename T>
class StaleObject {
public:
  friend void clause_stale_object_set<T>(int N, StaleObject<T> *obj);

  // template <typename... Args>
  // StaleObject(Args &&...args) : container_{ T(std::forward<Args>(args)...) }
  // {}

  StaleObject() : container_(1) {}

  StaleObject<T> &operator=(StaleObject<T> &other) {
    set(other.get());
    return *this;
  }

  // INLINE operator T() {
  //   return get();
  // }

  INLINE T &get() {
    int k = 0;
    auto p = noelle_pragma_begin("ldtc", &k, (int)0);
    auto &result = container_[k];
    noelle_pragma_end(p);
    return result;
  }

  INLINE T &__get(int k) {
    return container_[k];
  }

  INLINE void set(T new_value) {
    int k = 0;
    auto p = noelle_pragma_begin("ldtc",
                                 &k,
                                 (int)0,
                                 clause_stale_object_set<T>,
                                 this);
    container_[k] = std::move(new_value);
    noelle_pragma_end(p);
  }

  INLINE void __set(int k, T new_value) {
    container_[k] = std::move(new_value);
  }

private:
  std::vector<T> container_;
};

} // namespace skynet