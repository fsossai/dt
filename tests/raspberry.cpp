#include <iostream>

#include "skynet.h"

using namespace std;

int main() {
  skynet::Sequence<char> a;

  for (auto c : "raspberry") {
    a.append(c);
  }

  a.printInternals();

  return 0;
}
