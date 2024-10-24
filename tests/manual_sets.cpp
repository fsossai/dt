#include <iostream>
#include <vector>
#include <omp.h>

#include "Scalar.hpp"
#include "Set.hpp"
#include "Skynet.hpp"

using namespace std;

int main(int argc, char *argv[]) {
  skynet::Set<int> set;

  int N = 10;
  int T = 2;
  if (argc > 1) {
    if (atoi(argv[1]) > 0) {
      N = atoi(argv[1]);
    }
  }
  if (argc > 2) {
    if (atoi(argv[2]) > 0) {
      T = atoi(argv[2]);
    }
  }

  std::vector<int> elements;

  const int M = N;
  cout << "N = " << N << "\n";
  cout << "M = " << M << "\n";

  for (int i = 0; i < N; i++) {
    elements.push_back(rand() % M);
  }

  int V1_insert[T];
  for (int t = 0; t < T; t++) {
    if (t == 0) {
      V1_insert[t] = 0; // default
    } else {
      V1_insert[t] = skynet::clause_set_insert(&set);
    }
  }
#pragma omp parallel num_threads(T)
#pragma omp for
  for (int t = 0; t < T; t++) {
    for (int i = t * N / T; i < (t + 1) * N / T; i++) {
      set.__insert(V1_insert[t], elements[i]);
    }
  }

  cout << "Set:\n" << set.toString() << "\n\n";

  skynet::Scalar<int> result(0);
  auto _it = set.begin();
  auto _end = set.end();

  int V2_plusplus[T];
  int V2_neq[T];
  int V2_star[T];
  int V2_sum[T];
  for (int t = 0; t < T; t++) {
    if (t == 0) {
      V2_plusplus[t] = 0; // default
      V2_neq[t] = 0;      // default
      V2_star[t] = 0;     // default
      V2_sum[t] = 0;      // default
    } else {
      V2_plusplus[t] = skynet::clause_set_op_plusplus<int>(&_it);
      V2_sum[t] = skynet::clause_scalar_sum<int>(&result);
      V2_neq[t] = skynet::clause_set_op_neq<int>(&_it);
      V2_star[t] = skynet::clause_set_op_star<int>(&_it);
    }
  }
#pragma omp parallel num_threads(T)
#pragma omp for
  for (int t = 0; t < T; t++) {
    for (; _it.__op_neq(V2_neq[t], _end);) {
      result.__sum(V2_sum[t], _it.__op_star(V2_star[t]));

      _it.__op_plusplus(V2_plusplus[t]);
    }
  }
  cout << "Sum = " << result.get() << "\n";

  cout << "Internal representation:\n";
  set.printInternals();
  cout << "\n";

  cout << "Statistics:\n";
  set.printStats();

  return 0;
}
