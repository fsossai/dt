#include <iostream>
#include <vector>
#include <cassert>

#include "Sequence.hpp"
#include "Skynet.hpp"

using namespace std;

void show(skynet::Sequence<int> &v) {
  for (auto x : v) {
    printf("%d ", x);
  }
  printf("\n");
}
int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Usage: %s <N>\n", argv[0]);
    return 1;
  }

  const int N = atoi(argv[1]);
  assert(N > 0);

  printf("N = %d\n", N);

  skynet::Sequence<int> input;

  for (int i = 0; i < N; i++) {
    input.append(rand() % 90 + 10);
  }

  skynet::Sequence<int> partition1;
  skynet::Sequence<int> partition2;

  auto predicate = [](int x) { return x % 2 == 0; };

  for (auto x : input) {
    if (predicate(x)) {
      partition1.append(x);
    } else {
      partition2.append(x);
    }
  }

  printf("partition1: ");
  show(partition1);
  printf("partition2: ");
  show(partition2);

  return 0;
}
