#include <iostream>
#include <vector>

#include "Set2.hpp"
#include "Skynet.hpp"

using namespace std;
using namespace skynet;

int main(int argc, char *argv[]) {
  int N = 10;
  if (argc > 1) {
    if (atoi(argv[1]) > 0) {
      N = atoi(argv[1]);
    }
  }

  const int T = 2;

  Set2<int> set;

  clause_set2_insert_bulk(T, &set);

  for (int t = 0; t < T; t++) {
    // for (int i = t * N / T; i < (t + 1) * N / T; i++) {
    for (int i = 0; i < N; i++) {
      set.__insert(1, i);
    }
  }

  set.printInternals();

  cout << "Set (p view): | ";
  auto _it = set.begin();
  auto _end = set.end();
  clause_set2_op_plusplus_bulk(T, &_it);
  for (int t = 0; t < T; t++) {
    for (; _it.__op_neq(t, _end); _it.__op_plusplus(t)) {
      auto v = _it.__op_star(t);
      cout << v << " " << flush;
    }
    cout << "| ";
  }
  cout << "\n";

  cout << "Set (s view): ";
  for (auto x : set) {
    cout << x << " " << flush;
  }
  cout << "\n";

  return 0;
}
