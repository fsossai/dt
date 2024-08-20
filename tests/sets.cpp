#include <iostream>
#include <vector>

#include "Skynet.hpp"

using namespace std;

int main(int argc, char *argv[]) {
  skynet::Set<int> set;

  int N = 0;
  if (argc > 1) {
    N = atoi(argv[1]);
  }
  N = std::max<int>(10, N);

  std::vector<int> elements;

  cout << "N = " << N << "\n";
  cout << "Generation: ";

  for (int i = 0; i < N; i++) {
    auto e = rand() % N;
    elements.push_back(e);
    cout << e << " ";
  }
  cout << "\n";

  // Kernel
  skynet::set_clause_insert(&set);
  skynet::set_clause_insert(&set);
  skynet::set_clause_insert(&set);
  for (auto e : elements) {
    set.insert(e);
  }

  // Printing
  cout << "Set: ";
  for (auto e : set) {
    cout << e << " ";
  }
  cout << "\n";

  set.printInternals();

  return 0;
}
