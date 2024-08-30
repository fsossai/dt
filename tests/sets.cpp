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

  const int M = N;
  cout << "N = " << N << "\n";
  cout << "M = " << M << "\n";

  for (int i = 0; i < N; i++) {
    elements.push_back(rand() % M);
  }

  // Kernel
  for (int i = 0; i < N; i++) {
    set.insert(elements[i]);
  }

  cout << "Set:\n" << set.toString() << "\n\n";

  cout << "Internal representation:\n";
  set.printInternals();
  cout << "\n";

  cout << "Statistics:\n";
  set.printStats();

  return 0;
}
