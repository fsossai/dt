#pragma once

#include <cassert>
#include <vector>
#include "arcana/noelle/core/Pragma.h"

#include "Common.hpp"

namespace skynet {

template <class T,
          uint32_t PAD = std::max<uint32_t>(L1D_CACHE_LINE_SIZE / sizeof(T), 1)>
class StaleObject;

template <class T, uint32_t PAD>
void clause_stale_object_set(int N, StaleObject<T, PAD> *obj) {
#ifdef DEBUG
  std::printf("%s(%i, %p)\n", __func__, N, obj);
#endif
  if (obj->container_.size() == N * PAD) {
    return;
  }
  obj->container_.resize(N * PAD);
}

template <class T, uint32_t PAD>
class StaleObject {
public:
  friend void clause_stale_object_set<T>(int N, StaleObject<T, PAD> *obj);

  // template <typename... Args>
  // StaleObject(Args &&...args) : container_{ T(std::forward<Args>(args)...) }
  // {}

  StaleObject() : container_(1) {}

  StaleObject<T, PAD> &operator=(StaleObject<T, PAD> &other) {
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
    return container_[k * PAD];
  }

  INLINE void set(T new_value) {
    int k = 0;
    auto p = noelle_pragma_begin("ldtc",
                                 &k,
                                 (int)0,
                                 clause_stale_object_set<T, PAD>,
                                 this);
    container_[k * PAD] = std::move(new_value);
    noelle_pragma_end(p);
  }

  INLINE void __set(int k, T new_value) {
    container_[k * PAD] = std::move(new_value);
  }

private:
  std::vector<T> container_;
};

} // namespace skynet