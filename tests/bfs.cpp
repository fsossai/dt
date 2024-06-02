#include <iostream>
#include <set>
#include <queue>
#include <chrono>
#include <stack>

using namespace std;

#define TIMER_START(x)                                                         \
  do {                                                                         \
    __stimers.push(new STimer(x));                                             \
  } while (false)
#define TIMER_STOP()                                                           \
  do {                                                                         \
    delete __stimers.top();                                                    \
    __stimers.pop();                                                           \
  } while (false)

class STimer;

static stack<STimer *> __stimers;

class STimer {
public:
  STimer(string prefix) : prefix(prefix) {
    tstart = chrono::steady_clock::now();
  }

  ~STimer() {
    auto tstop = chrono::steady_clock::now();
    chrono::duration<double> elapsed_s = tstop - tstart;
    cout << prefix << " time: " << elapsed_s.count() * 1e3 << " ms" << endl;
  }

private:
  chrono::time_point<chrono::steady_clock> tstart;
  string prefix;
};

struct Node {
  int value;
  std::set<Node*> inEdges;
  std::set<Node*> outEdges;
};

struct Graph {
  Graph(int N);
  ~Graph();

  void mixedAttachment(int links, float beta);
  void preferentialAttachment(int links);
  void randomAttachment(int links);
  void incrementalValues();
  void setEdge(int i, int j, bool link);
  void setEdge(int i, int j);

  std::vector<Node*> nodes;
};

Graph::Graph(int N) {
  for (int i = 0; i < N; i++) {
    nodes.push_back(new Node);
  }
}

Graph::~Graph() {
  for (auto &n : nodes) {
    delete n;
  }
}

void Graph::mixedAttachment(int links, float beta) {
  const int N = nodes.size();
  std::vector<int> degree;
  degree.resize(N);

  setEdge(0, 1);
  setEdge(1, 0);
  degree[0] = 1;
  degree[1] = 1;

  for (int i = 2; i < N; i++) {
    for (int l = 0; l < links; l++) {
      int j = 0;
      if ((float)rand() / (float)RAND_MAX < beta) {
        int p = rand() % ((i - 1) * 2);
        int cumulative = degree[0];
        if (cumulative <= p) {
          for (j = 1; j < i; j++) {
            cumulative += degree[j];
            if (cumulative > p) {
              break;
            }
          }
        }
      } else {
        j = rand() % i;
      }
      setEdge(i, j);
      setEdge(j, i);
      degree[i]++;
      degree[j]++;
    }
  }
}

void Graph::randomAttachment(int links) {
  mixedAttachment(links, 0.0);
}

void Graph::incrementalValues() {
  int i = 1;
  for (auto &n : nodes) {
    n->value = i;
    i++;
  }
}

void Graph::setEdge(int i, int j, bool link) {
  auto n = nodes[i];
  auto m = nodes[j];
  if (link) {
    n->outEdges.insert(m);
    m->inEdges.insert(n);
  } else {
    n->outEdges.erase(m);
    m->outEdges.erase(n);
  }
}

void Graph::setEdge(int i, int j) {
  setEdge(i, j, true);
}

int reduce(const Graph &g, Node *first) {
  std::queue<Node*> toExplore;
  std::set<Node*> enqueued;

  int t = 0;
  toExplore.push(first);
  enqueued.insert(first);

  while (!toExplore.empty()) {
    auto n = toExplore.front();
    toExplore.pop();
    t += n->value;

    for (auto &m : n->outEdges) {
      bool toEnqueue = enqueued.find(m) == enqueued.end();
      if (toEnqueue) {
        toExplore.push(m);
        enqueued.insert(m);
      }
    }
  }

  return t;
}

int main(int argc, char *argv[]) {
  int N = 10000;
  if (argc > 1) {
    if (atoi(argv[1]) > 1) {
      N = atoi(argv[1]);
    }
  }
  srand(0);

  TIMER_START("Total");

  Graph *g = new Graph(N);

  TIMER_START("Generation");
  g->randomAttachment(2);
  g->incrementalValues();
  TIMER_STOP();

  TIMER_START("Kernel");
  cout << "res = " << reduce(*g, g->nodes[0]) << endl;
  TIMER_STOP();

  TIMER_START("Destruction");
  delete g;
  TIMER_STOP();

  TIMER_STOP();

  return 0;
}
