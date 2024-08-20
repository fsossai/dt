#include <iostream>
#include <string>
#include <chrono>
#include <stack>
#include <string>

#define TIMER_START(x)                                                         \
  do {                                                                         \
    __stimers.push(new ScopeTimer(x));                                         \
  } while (false)
#define TIMER_STOP()                                                           \
  do {                                                                         \
    delete __stimers.top();                                                    \
    __stimers.pop();                                                           \
  } while (false)

class ScopeTimer;

static std::stack<ScopeTimer *> __stimers;

class ScopeTimer {
public:
  ScopeTimer(std::string prefix) : prefix(prefix) {
    tstart = std::chrono::steady_clock::now();
  }

  ~ScopeTimer() {
    auto tstop = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed_s = tstop - tstart;
    std::cout << prefix << " time: " << elapsed_s.count() * 1e3 << " ms"
              << std::endl;
  }

private:
  std::chrono::time_point<std::chrono::steady_clock> tstart;
  std::string prefix;
};
