#pragma once

#include <cstdint>
#include <type_traits>

// #include "arcana/noelle/core/Pragma.h"

namespace skynet {

template <typename T>
typename std::enable_if<std::is_pointer<T>::value, int>::type hasher(T val) {
  return (reinterpret_cast<uint64_t>(val) * 14695981039346656037ULL)
         >> (64 - 11);
};

template <typename T>
typename std::enable_if<std::is_arithmetic<T>::value, int>::type hasher(T val) {
  return val;
};


} // namespace skynet
