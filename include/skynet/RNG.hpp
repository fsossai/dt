#pragma once

#include <cassert>
#include <cstdint>
#include <random>
#include <vector>

#include "arcana/noelle/core/Pragma.h"

#include "Common.hpp"

namespace skynet {

template <uint32_t PAD = compute_padding<std::mt19937>()>
class RNG;

template <uint32_t PAD>
void clause_rng_gen(int N, RNG<PAD> *rng) {
  if (rng->engine_.size() / PAD == N) {
    return;
  }

  rng->engine_.resize(N * PAD, std::mt19937(std::random_device{}()));
}

template <uint32_t PAD>
class RNG {
public:
  friend void clause_rng_gen<PAD>(int N, RNG<PAD> *s);

  RNG() {
    engine_.resize(PAD, std::mt19937(std::random_device{}()));
  }

  INLINE int64_t gen() {
    size_t k = 0;
    auto _p =
        noelle_pragma_begin("ldtc", &k, (size_t)0, clause_rng_gen<PAD>, this);
    auto num = __gen(k);
    noelle_pragma_end(_p);
    return num;
  }

  INLINE int64_t __gen(size_t k) {
    return std::uniform_int_distribution<int>(0, RAND_MAX)(engine_[k * PAD]);
  }

private:
  std::vector<std::mt19937> engine_;
};

} // namespace skynet
