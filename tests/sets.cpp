#include <iostream>
#include <vector>

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

  cout << "N = " << N << "\n";
  const int M = N;

  // cout << "Generation: ";
  for (int i = 0; i < N; i++) {
    auto e = rand() % M;
    elements.push_back(e);
    // cout << e << " ";
  }
  // cout << "\n";

  // Kernel
  for (auto e : elements) {
    set.insert(e);
  }

  cout << "Set:\n" << set.toString() << "\n\n";

  cout << "Internal representation:\n";
  set.printInternals();
  cout << "\n";

  cout << "Statistics:\n";
  set.printStats();

  return 0;
}
