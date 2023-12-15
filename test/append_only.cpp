#include <iostream>

#include "skynet.h"

using namespace std;

int main() {
  skynet::Sequence<int> a;

  a.append(0);
  a.append(1);
  a.append(2);

  for (int i = 3; i < 10; i++) {
    a.append(i);
  }

  a.printInternals();

  return 0;
}

