#include <iostream>
#include <vector>

#include "Skynet.hpp"

using namespace std;

int main(int argc, char *argv[]) {
  skynet::Scalar<int> s = 0;

  int N = 10;
  if (argc > 1) {
    if (atoi(argv[1]) > 0) {
      N = atoi(argv[1]);
    }
  }

  for (int i = 0; i < N; i++) {
    s.sum(i);
  }

  cout << s.get() << "\n";

  return 0;
}
