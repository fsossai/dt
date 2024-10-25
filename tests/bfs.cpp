#include <iostream>
#include <iterator>
#include <set>
#include <queue>
#include <chrono>
#include <stack>
#include <unordered_set>

#include "ScopeTimer.hpp"
#include "Set.hpp"
#include "Skynet.hpp"
#include "arcana/noelle/core/Pragma.h"

using namespace std;

struct Node {
  int value;
  std::set<Node *> inEdges;
  std::set<Node *> outEdges;
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

  std::vector<Node *> nodes;
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

int bfs_frontier(const Graph &g, Node *root) {
  unordered_set<Node *> enqueued;
  auto currentFrontier = new vector<Node *>();
  auto nextFrontier = new vector<Node *>();

  int t = 0;
  currentFrontier->push_back(root);
  enqueued.insert(root);

  while (!currentFrontier->empty()) {
    for (auto n : *currentFrontier) {
      t += n->value;

      for (auto &m : n->outEdges) {
        bool notEnqueued = enqueued.find(m) == enqueued.end();
        if (notEnqueued) {
          nextFrontier->push_back(m);
          enqueued.insert(m);
        }
      }
    }
    currentFrontier->clear();
    swap(currentFrontier, nextFrontier);
  }

  delete currentFrontier;
  delete nextFrontier;

  return t;
}

int bfs_tc_manual(const Graph &g, Node *root) {
  skynet::Set<Node *> enqueued;
  auto currentFrontier = new skynet::Set<Node *>();
  auto nextFrontier = new skynet::Set<Node *>();

  skynet::Scalar<int> result(0);
  currentFrontier->insert(root);
  enqueued.insert(root);

  int f_idx = 0;
  const int T = 1;
  while (!currentFrontier->empty()) {
#ifdef DEBUG
    cout << "--- processing frontier " << f_idx;
    cout << " (size=" << currentFrontier->size() << ")\n";
#endif
    auto _it2 = currentFrontier->begin();
    auto _end2 = currentFrontier->end();
    int L2_plusplus[T];
    int L2_neq[T];
    int L2_star[T];
    int L2_sum_1[T];
    int L2_sum_2[T];
    int L2_insert[T];
    for (int t = 0; t < T; t++) {
      if (t == 0) {
        L2_plusplus[t] = 0; // default
        L2_neq[t] = 0;      // default
        L2_star[t] = 0;     // default
        L2_sum_1[t] = 0;    // default
        L2_sum_2[t] = 0;    // default
        L2_insert[t] = 0;   // default
      } else {
        skynet::clause_set_insert(currentFrontier);
        skynet::clause_scalar_sum(&currentFrontier->storage_size_);
        L2_plusplus[t] = skynet::clause_set_op_plusplus(&_it2);
        L2_sum_1[t] = skynet::clause_scalar_sum(&result);
        L2_sum_2[t] = skynet::clause_scalar_sum(&nextFrontier->storage_size_);
        L2_neq[t] = skynet::clause_set_op_neq(&_it2);
        L2_star[t] = skynet::clause_set_op_star(&_it2);
        L2_insert[t] = skynet::clause_set_insert(nextFrontier);
      }
    }
#pragma omp parallel num_threads(T)
#pragma omp for
    for (int t = 0; t < T; t++) {
      for (; _it2.__op_neq(L2_neq[t], _end2);) {
        auto n = _it2.__op_star(L2_star[t]);
        result.__sum(L2_sum_1[t], n->value);

        for (auto m : n->outEdges) {
          if (!enqueued.contains(m)) {
            nextFrontier->__insert(t, m);
          }
        }

        _it2.__op_plusplus(L2_plusplus[t]);
      }
    }

    auto _it4 = nextFrontier->begin();
    auto _end4 = nextFrontier->end();
    int L4_plusplus[T];
    int L4_neq[T];
    int L4_star[T];
    int L4_insert[T];
    int L4_sum[T];
    for (int t = 0; t < T; t++) {
      if (t == 0) {
        L4_insert[t] = 0;   // default
        L4_plusplus[t] = 0; // default
        L4_neq[t] = 0;      // default
        L4_star[t] = 0;     // default
        L4_sum[t] = 0;      // default
      } else {
        L4_insert[t] = skynet::clause_set_insert(&enqueued);
        L4_plusplus[t] = skynet::clause_set_op_plusplus(&_it4);
        L4_neq[t] = skynet::clause_set_op_neq(&_it4);
        L4_star[t] = skynet::clause_set_op_star(&_it4);
        L4_sum[t] = skynet::clause_scalar_sum(&enqueued.storage_size_);
      }
    }
#pragma omp parallel num_threads(T)
#pragma omp for
    for (int t = 0; t < T; t++) {
      for (; _it4.__op_neq(L4_neq[t], _end4);) {
        auto m = _it4.__op_star(L4_star[t]);

        enqueued.__insert(L4_insert[t], m);

        _it4.__op_plusplus(L4_plusplus[t]);
      }
    }
    currentFrontier->clear();
    swap(currentFrontier, nextFrontier);
    ++f_idx;
  }

  delete currentFrontier;
  delete nextFrontier;

  return result.get();
}

int bfs_tc(const Graph &g, Node *root) {
  skynet::Set<Node *> enqueued;
  auto currentFrontier = new skynet::Set<Node *>();
  auto nextFrontier = new skynet::Set<Node *>();

  skynet::Scalar<int> t(0);
  currentFrontier->insert(root);
  enqueued.insert(root);

  int f_idx = 0;
  auto p1 = noelle_pragma_begin("loop.tag", 1);
  while (!currentFrontier->empty()) {
    cout << "--- processing frontier " << f_idx << "\n";
    auto p2 = noelle_pragma_begin("loop.tag", 2);
    auto p21 = noelle_pragma_begin("loop.doall", "yes");
    for (auto n : *currentFrontier) {
      t.sum(n->value);

      auto p3 = noelle_pragma_begin("loop.tag", 3);
      auto p31 = noelle_pragma_begin("loop.doall", "maybe");
      for (auto m : n->outEdges) {
        if (!enqueued.contains(m)) {
          nextFrontier->insert(m);
        }
      }
      noelle_pragma_end(p31);
      noelle_pragma_end(p3);
    }
    noelle_pragma_end(p21);
    noelle_pragma_end(p2);
    auto p4 = noelle_pragma_begin("loop.tag", 4);
    auto p41 = noelle_pragma_begin("loop.doall", "no");
    for (auto m : *nextFrontier) {
      enqueued.insert(m);
    }
    noelle_pragma_end(p41);
    noelle_pragma_end(p4);
    currentFrontier->clear();
    swap(currentFrontier, nextFrontier);
    ++f_idx;
  }
  noelle_pragma_end(p1);

  delete currentFrontier;
  delete nextFrontier;

  return t.get();
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
  // cout << "res = " << bfs_frontier(*g, g->nodes[0]) << endl;
  auto result = bfs_tc_manual(*g, g->nodes[0]);
  cout << "res = " << result << "\n";
  TIMER_STOP();

  delete g;

  TIMER_STOP();

  return 0;
}
