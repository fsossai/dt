#include <iostream>

#include "skynet.h"

using namespace std;

int main() {
  skynet::Sequence<int> a;

  for (int i = 1; i < 5; i++) {
    a.append(-i);
    for (int j = 0; j < i; j++) {
      a.append(i);
    }
    //skynet::next_k(&a);
  }

  a.printInternals();

  return 0;
}

