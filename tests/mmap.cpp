#include <iostream>
#include "Skynet.hpp"

using namespace std;
using namespace skynet;

int main() {
  Multimap<int, int> mmap;

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
    printf("insert {%2d, %2d}\n", key, val);
    mmap.insert(key, val);
  }

  mmap.print();

  cout << "numKeys() = " << mmap.numKeys() << "\n";

}
