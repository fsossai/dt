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

  RNG() {
    engine_.resize(RNG_PAD, std::mt19937(std::random_device{}()));
    gset_.resize(RNG_PAD, 0.0);
    iset_.resize(RNG_PAD, 0);
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

  double __genN(size_t k) {
    const size_t idx = k * RNG_PAD;
    if (iset_[idx] == 0) {
      // Box-Muller transform
      double fac, rsq, v1, v2;
      do {
        v1 = 2.0 * (__gen(k) / (double)RAND_MAX) - 1.0;
        v2 = 2.0 * (__gen(k) / (double)RAND_MAX) - 1.0;
        rsq = v1 * v1 + v2 * v2;
      } while (rsq >= 1.0 || rsq == 0.0);

      fac = sqrt(-2.0 * log(rsq) / rsq);
      gset_[idx] = v1 * fac;
      iset_[idx] = 1;
      return v2 * fac;
    }

    iset_[idx] = 0;
    return gset_[idx];
  }

private:
  std::vector<std::mt19937> engine_;
  std::vector<double> gset_;
  std::vector<int> iset_;
};

inline void clause_rng_gen(int N, RNG *rng) {
  if (rng->engine_.size() / RNG_PAD == N) {
    return;
  }

  std::random_device rd;
  rng->engine_.resize(N * RNG_PAD);
  rng->gset_.resize(N * RNG_PAD, 0.0);
  rng->iset_.resize(N * RNG_PAD, 0);
  for (auto &eng : rng->engine_) {
    eng.seed(rd());
  }
}

} // namespace skynet
