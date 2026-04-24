#pragma once

#include <cassert>
#include <cstdint>
#include <random>
#include <vector>

#include "arcana/noelle/core/Pragma.h"

#include "Common.hpp"

namespace skynet {

static constexpr uint32_t RNG_PAD = compute_padding<std::mt19937>();

class RNG;

void clause_rng_gen(int N, RNG *rng);

class RNG {
public:
  friend void clause_rng_gen(int N, RNG *s);

  RNG(int seed = 0) : start_seed_(seed) {
    engine_.resize(RNG_PAD, std::mt19937(seed));
  }

  INLINE int64_t gen() {
    size_t k = 0;
    auto _p = noelle_pragma_begin("ldtc", &k, (size_t)0, clause_rng_gen, this);
    auto num = __gen(k);
    noelle_pragma_end(_p);
    return num;
  }

  INLINE int64_t __gen(size_t k) {
    return std::uniform_int_distribution<int>(0,
                                              RAND_MAX)(engine_[k * RNG_PAD]);
  }

  INLINE double genN() {
    size_t k = 0;
    auto _p = noelle_pragma_begin("ldtc", &k, (size_t)0, clause_rng_gen, this);
    auto num = __genN(k);
    noelle_pragma_end(_p);
    return num;
  }

  INLINE double __genN(size_t k) {
    return std::normal_distribution<double>(0.0, 1.0)(engine_[k * RNG_PAD]);
  }

private:
  std::vector<std::mt19937> engine_;
  int start_seed_;
};

inline void clause_rng_gen(int N, RNG *rng) {
  if (rng->engine_.size() / RNG_PAD == N) {
    return;
  }

  const int oldN = rng->engine_.size() / RNG_PAD;
  rng->engine_.resize(N * RNG_PAD);
  for (int i = oldN; i < N; i++) {
    rng->engine_[i * RNG_PAD].seed(rng->start_seed_++);
  }
}

} // namespace skynet
