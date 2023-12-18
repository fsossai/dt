#include <iostream>

#include "skynet.h"

using namespace std;

int myfunc(float a) { return 9; }

int main() {
  skynet::Sequence<int> a;

  //a.append(0);
  //a.append(1);
  //a.append(2);

  int k = 0;

  for (int i = 3; i < 10; i++) {
    a.append(i);
    PRAGMA_LDTC_BEGIN(i, myfunc, 9.8f);
    PRAGMA_LDTC_END();
  }

  a.printInternals();

  return 0;
}

