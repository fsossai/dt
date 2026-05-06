#include <cstdio>
#include <cstdlib>
#include <vector>

#include "arcana/noelle/core/Pragma.h"

int main(int argc, char *argv[]) {
  int N = (argc > 1) ? atoi(argv[1]) : 1000000;
  int M = (argc > 2) ? atoi(argv[2]) : 100;

  std::vector<double> a(N, 1.0);
  std::vector<double> b(N, 2.0);
  std::vector<double> c(N, 0.0);

  // Loop 1: vector addition, repeated M times
  for (int iter = 0; iter < M; iter++) {
    auto p1 = noelle_pragma_begin("loop.tag", 1);
    for (int i = 0; i < N; i++) {
      c[i] = a[i] + b[i];
    }
    noelle_pragma_end(p1);
  }

  // Loop 2: dot product accumulation, repeated M/2 times
  double sum = 0.0;
  for (int iter = 0; iter < M / 2; iter++) {
    auto p2 = noelle_pragma_begin("loop.tag", 2);
    for (int i = 0; i < N; i++) {
      sum += a[i] * b[i];
    }
    noelle_pragma_end(p2);
  }

  // Loop 3: scale, run once
  {
    auto p3 = noelle_pragma_begin("loop.tag", 3);
    for (int i = 0; i < N; i++) {
      c[i] *= 0.5;
    }
    noelle_pragma_end(p3);
  }

  printf("c[0]=%.1f sum=%.1f\n", c[0], sum);
  return 0;
}
