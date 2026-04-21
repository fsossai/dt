#include <iostream>

#include "skynet/Sequence.hpp"
#include "skynet/Skynet.hpp"

using namespace std;

int main() {
  skynet::Sequence<int> a;

  a.append(0);
  a.append(1);
  a.append(2);

  skynet::clause_sequence_append(2, &a);
  for (int i = 3; i < 10; i++) {
    a.__append(i % 2, i);
  }

  skynet::Scalar<int> result;
  for (auto x : a) {
    result.add(x);
  }

  cout << "Sum = " << result.get() << "\n";
  assert(result.get() == 45);

  a.printInternals();

  for (int i = 0; i < a.size(); i++) {
    cout << a[i] << " ";
  }
  cout << "\n";

  return 0;
}
