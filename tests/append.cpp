#include <iostream>

#include "Skynet.hpp"

using namespace std;

int main() {
  skynet::Sequence<int> a;

  a.append(0);
  a.append(1);
  a.append(2);

  for (int i = 3; i < 10; i++) {
    a.append(i);
  }

  int sum = 0;
  for (auto x : a) {
    sum += x;
  }

  cout << "Sum = " << sum << "\n";
  assert(sum == 45);

  a.printInternals();

  return 0;
}
