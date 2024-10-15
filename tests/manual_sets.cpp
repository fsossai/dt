#include <iostream>
#include <vector>

#include "Set.hpp"
#include "Skynet.hpp"

using namespace std;

int main(int argc, char *argv[]) {
  skynet::Set<int> set;

  int N = 10;
  int T = 1;
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

  int TCV_1_insert[T];
  for (int t = 0; t < T; t++) {
    if (t == 0) {
      TCV_1_insert[t] = 0; // default
    } else {
      TCV_1_insert[t] = skynet::clause_set_insert(&set);
    }
  }
  for (int t = 0; t < T; t++) {
    for (int i = t * N / T; i < (t + 1) * N / T; i++) {
      set.__insert(t, elements[i]);
    }
  }

  cout << "Set:\n" << set.toString() << "\n\n";

  int sum = 0;
  auto _it = set.begin();
  auto _end = set.end();

  int TCV_2_plusplus[T];
  int TCV_2_neq[T];
  int TCV_2_star[T];
  for (int t = 0; t < T; t++) {
    if (t == 0) {
      TCV_2_plusplus[t] = 0; // default
      TCV_2_neq[t] = 0;      // default
      TCV_2_star[t] = 0;     // default
    } else {
      TCV_2_plusplus[t] = skynet::clause_set_op_plusplus<int>(&_it);
      TCV_2_neq[t] = skynet::clause_set_op_neq<int>(&_it);
      TCV_2_star[t] = skynet::clause_set_op_star<int>(&_it);
    }
  }
  for (int t = 0; t < T; t++) {
    for (; _it.__op_neq(t, _end); _it.__op_plusplus(t)) {
      sum += _it.__op_star(t);
    }
  }
  cout << "Sum = " << sum << "\n";

  cout << "Internal representation:\n";
  set.printInternals();
  cout << "\n";

  cout << "Statistics:\n";
  set.printStats();

  return 0;
}
