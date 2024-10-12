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

  skynet::Scalar<int> result;
  for (auto x : a) {
    result.sum(x);
  }

  cout << "Sum = " << result.get() << "\n";
  assert(result.get() == 45);

  a.printInternals();

  return 0;
}
