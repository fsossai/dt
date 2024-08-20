#include <iostream>

#include "Skynet.hpp"
#include "ScopeTimer.hpp"

using namespace std;

using hist_t = skynet::Array<int64_t>;

vector<int64_t> generate(int64_t N, int64_t M) {
  vector<int64_t> values(N);
  for (int i = 0; i < N; i++) {
    values[i] = rand() % M;
  }
  return values;
}

hist_t computeFiniteDomainHistogram(vector<int64_t> &values, int64_t M) {
  hist_t hist(M);
  const int N = values.size();
  for (int i = 0; i < N; i++) {
    auto x = values[i];
    hist.add(x, 1);
  }
  return hist;
}

void printHist(hist_t &hist) {
  for (int i = 0; i < hist.size(); i++) {
    printf("%2i : %li\n", i, hist[i]);
  }
}

int main(int argc, char *argv[]) {
  int64_t N = 0;
  if (argc > 1) {
    N = atoi(argv[1]) * 1'000'000;
  }
  if (N == 0) {
    fprintf(stderr, "ERROR: missing input size\n");
    return 1;
  }

  printf("N = %liM\n", N / 1'000'000);

  int64_t M = 1 << 8;

  TIMER_START("Total");

  TIMER_START("Generation");
  auto values = generate(N, M);
  TIMER_STOP();

  TIMER_START("Kernel");
  auto hist = computeFiniteDomainHistogram(values, M);
  TIMER_STOP();

  TIMER_STOP();

  printHist(hist);

  hist.printInternals();

  return 0;
}
