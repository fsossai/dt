#include <iostream>

#include "Skynet.hpp"

using namespace std;

int main(int argc, char *argv[]) {
  if (argc < 3) {
    printf("Usage: %s <T1> <T2>\n", argv[0]);
    return 1;
  }

  const int T1 = atoi(argv[1]);
  const int T2 = atoi(argv[2]);
  assert(T1 * T2 > 0);
  assert(T1 * T2 <= omp_get_max_threads());
  omp_set_max_active_levels(2);

  skynet::HSequence<int> a;

  vector<int> a_ref;
  for (int i = 1; i < 8; i++) {
    a_ref.push_back(-i);
    for (int j = 0; j < i; j++) {
      a_ref.push_back(i);
    }
  }

  auto c1 = skynet::clause_split(T1, &a);
#pragma omp parallel for num_threads(T1)
  for (int i = 1; i < 8; i++) {
    a.append(-i, c1);

    auto c0 = skynet::clause_split(T2, c1);
#pragma omp parallel for num_threads(T2)
    for (int j = 0; j < i; j++) {
      a.append(i, c0);
    }
  }

  a.printInternals();
  printf("  a = ");
  a.print();

  // print reference
  printf("ref = ");
  for (int i = 0; i < a_ref.size(); i++) {
    printf("%d ", a_ref[i]);
  }
  printf("\n");

  // correcteness
  assert(a.size() == a_ref.size());
  for (int i = 0; i < a.size(); i++) {
    assert(a.at(i) == a_ref.at(i));
  }

  return 0;
}
