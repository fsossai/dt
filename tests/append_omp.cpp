#include <iostream>
#include <omp.h>

#include "Skynet.hpp"

using namespace std;

int main() {
  skynet::Sequence<int> a;

  int num_threads = omp_get_max_threads();
  for (int i = 0; i < num_threads - 1; i++) {
    skynet::next_k(&a);
  }

#pragma omp parallel for schedule(dynamic)
  for (int i = 0; i < 1000; i++) {
    int tid = omp_get_thread_num();
#pragma ldtc ispace(tid)
    a.append_as(tid, i);
  }

  a.printInternals();

  return 0;
}
