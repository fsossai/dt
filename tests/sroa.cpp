#include <cmath>
#include <iostream>
#include <omp.h>
#include <vector>

#include "skynet/Scalar.hpp"

using namespace std;

struct Point {
  float *coord;
  int ndim;
  float weight;
};

float dist(Point a, Point b) {
  float sum = 0.0f;
  for (int i = 0; i < a.ndim; ++i) {
    float d = a.coord[i] - b.coord[i];
    sum += d * d;
  }
  return std::sqrt(sum);
}

int main(int argc, char *argv[]) {
  float centerCoord[] = { 1.0f, 2.0f };
  Point center{ centerCoord, 2, 1.0f };

  float p1[] = { 0.0f, 0.0f };
  float p2[] = { 3.0f, 4.0f };
  float p3[] = { 6.0f, 8.0f };
  float p4[] = { 2.0f, 1.0f };

  vector<Point> points = {
    { p1, 2, 1.0f },
    { p2, 2, 1.0f },
    { p3, 2, 1.0f },
    { p4, 2, 1.0f },
  };

  skynet::Scalar<float> sum = 0.0f;

  auto p11 = noelle_pragma_begin("loop.tag", 1);
  auto p12 = noelle_pragma_begin("loop.doall", "yes");
  auto p13 = noelle_pragma_begin("loop.scheduling", "static", 0);
  auto p14 = noelle_pragma_begin("unordered");
  const int N = points.size();
  for (int i = 0; i < N; i++) {
    sum += computeDistance(points.data()[i], center, 2);
  }
  noelle_pragma_end(p14);
  noelle_pragma_end(p13);
  noelle_pragma_end(p12);
  noelle_pragma_end(p11);

  float avg = points.empty() ? 0.0f : (sum / points.size());
  cout << avg << "\n";

  return 0;
}
