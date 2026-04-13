#include <iostream>
#include <vector>
#include <omp.h>

#include "Skynet.hpp"
#include "ScopeTimer.hpp"

using namespace std;

int main(int argc, char *argv[]) {
  int64_t N = 0;
  if (argc > 1) {
    N = atoi(argv[1]) * 1'000'000;
  }
  if (N == 0) {
    fprintf(stderr, "ERROR: missing input size\n");
    return 1;
  }

  printf("N = %liM\n", N / 1'000'000);

  skynet::RNG rng;

  double sum = 0;

  const int T = omp_get_max_threads();

  clause_rng_gen(T, &rng);
#pragma omp parallel for reduction(+ : sum)
  for (int i = 0; i < N; i++) {
    sum += rng.__gen(omp_get_thread_num()) % N;
  }
  cout << (sum / N / N) << "\n";

  return 0;
}
