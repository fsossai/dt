#include <iostream>

#include "Skynet.hpp"

using namespace std;

int main() {
  skynet::Sequence<int> a;
  skynet::Sequence<int> b;

  for (int i = 0; i < 10; i++) {
    a.append(i);
  }

  for (auto x : a) {
    b.append(x);
  }

  a.printInternals();
  b.printInternals();

  return 0;
}
