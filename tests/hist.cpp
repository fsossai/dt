#include <iostream>

#include "skynet.h"

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

int main() {
  int N = 100'000;
  int M = 10;
  auto values = generate(N, M);

  auto hist = computeFiniteDomainHistogram(values, M);

  printHist(hist);

  hist.printInternals();

  return 0;
}

