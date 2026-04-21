#include <iostream>
#include <vector>
#include <omp.h>

#include "skynet/Skynet.hpp"

using namespace std;

using vec = skynet::Array<int>;

void axpy(vec &A, vec &x, vec &y, int T1, int T2) {
  const int M = x.size();
  const int N = A.size() / M;
  assert(A.size() == M * N);
  assert(y.size() == N);

  omp_set_max_active_levels(2);

  auto c1 = skynet::clause_array_add2(T1, &y, T2);
#pragma omp parallel for num_threads(T1)
  for (int i = 0; i < N; i++) {

    auto c0 = skynet::clause_array_add2(T2, &y, 1, c1);
#pragma omp parallel for num_threads(T2)
    for (int j = 0; j < M; j++) {
      y.add2(i, A[i * M + j] * x[j], c0);
    }
  }
}

int main(int argc, char *argv[]) {
  const int M = 5;
  const int N = 5;

  if (argc < 3) {
    printf("Usage: %s <T1> <T2>\n", argv[0]);
    return 1;
  }

  const int T1 = atoi(argv[1]);
  const int T2 = atoi(argv[2]);
  assert(T1 * T2 > 0);
  assert(T1 * T2 <= omp_get_max_threads());

  vec A(M * N);
  vec x(M);
  vec y(N);

  for (int i = 0; i < M * N; i++) {
    A.set(i, i);
  }
  for (int i = 0; i < M; i++) {
    x.set(i, 1);
  }

  axpy(A, x, y, T1, T2);
  y.printInternals();

  printf("y = [ ");
  for (int i = 0; i < y.size(); i++) {
    printf("%d ", y[i]);
  }
  printf("]\n");
}
