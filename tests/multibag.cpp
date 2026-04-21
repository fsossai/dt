#include <iostream>

#include "skynet/Skynet.hpp"

using namespace std;
using namespace skynet;

int main() {
  OrderedMultibag<int, int> mmap;

  auto show = [&](auto key) {
    cout << "{ ";
    for (auto x : mmap[key]) {
      cout << x << " ";
    }
    cout << "}\n";
  };

  const int N = 10;
  const int M = 10;
  for (int i = 0; i < N; i++) {
    int key = rand() % M;
    int val = rand() % M;
    printf("insert { %d, %d }\n", key, val);
    mmap.insert(key, val);
  }

  mmap.print();

  cout << "numKeys() = " << mmap.numKeys() << "\n";
  cout << "count(3) = " << mmap.count(3) << "\n";
  cout << "count(9) = " << mmap.count(9) << "\n";
  cout << "count(100) = " << mmap.count(100) << "\n";
}
