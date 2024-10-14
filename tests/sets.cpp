#include <iostream>
#include <vector>

#include "Set.hpp"
#include "Skynet.hpp"

using namespace std;

int main(int argc, char *argv[]) {
  skynet::Set<int> set;

  int N = 10;
  if (argc > 1) {
    if (atoi(argv[1]) > 0) {
      N = atoi(argv[1]);
    }
  }

  std::vector<int> elements;

  const int M = N;
  cout << "N = " << N << "\n";
  cout << "M = " << M << "\n";

  for (int i = 0; i < N; i++) {
    auto x = rand() % M;
    // cout << "  e = " << x << "\n";
    elements.push_back(x);
  }

  // Kernel
  skynet::clause_set_insert(&set);
  skynet::clause_set_insert(&set);
  for (int i = 0; i < N; i++) {
    set.insert(elements[i]);
  }

  cout << "Set:\n" << set.toString() << "\n\n";

  int sum = 0;
  for (auto x : set) {
    // cout << "  x = " << x << "\n";
    sum += x;
  }
  cout << "Sum = " << sum << "\n";

  cout << "Internal representation:\n";
  set.printInternals();
  cout << "\n";

  cout << "Statistics:\n";
  set.printStats();

  return 0;
}
