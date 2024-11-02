#include <iostream>
#include <iterator>
#include <memory>
#include <set>
#include <queue>
#include <chrono>
#include <stack>
#include <unordered_set>
#include <omp.h>

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

int bfs_tc_manual_original(const Graph &g, Node *root) {
  skynet::Set<Node *> enqueued;
  auto currentFrontier = new skynet::Set<Node *>();
  auto nextFrontier = new skynet::Set<Node *>();

  skynet::Scalar<int> result(0);
  currentFrontier->insert(root);
  enqueued.insert(root);

  int f_idx = 0;
  const int T = 7;
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

int bfs_tc_manual_bulk(const Graph &g, Node *root) {
  skynet::Set<Node *> enqueued;
  auto currentFrontier = new skynet::Set<Node *>();
  auto nextFrontier = new skynet::Set<Node *>();

  skynet::Scalar<int> result(0);
  currentFrontier->insert(root);
  enqueued.insert(root);

  int f_idx = 0;
  const int T = 2;
  while (!currentFrontier->empty()) {
#if defined(DEBUG) || defined(BFS_DEBUG)
    cout << "--- processing frontier " << f_idx;
    cout << " (size=" << currentFrontier->size() << ")\n";
    cout << result.get() << "\n";
    result.printInternals();
    cout << "\n";
#endif
    enqueued.printInternals();
    cout << "\n";
    auto _it2 = currentFrontier->begin();
    auto _end2 = currentFrontier->end();
    int L2_plusplus[T];
    int L2_neq[T];
    int L2_star[T];
    int L2_sum[T];
    int L2_insert[T];
    skynet::clause_set_insert_bulk(T, currentFrontier);
    skynet::clause_scalar_sum_bulk(T, &currentFrontier->storage_size_);
    skynet::clause_set_insert_bulk(T, nextFrontier);
    skynet::clause_scalar_sum_bulk(T, &nextFrontier->storage_size_);
    skynet::clause_set_op_plusplus_bulk(T, &_it2);
    skynet::clause_scalar_sum_bulk(T, &result);
    for (int t = 0; t < T; t++) {
      L2_plusplus[t] = t;
      L2_neq[t] = t;
      L2_star[t] = t;
      L2_sum[t] = t;
      L2_insert[t] = t;
    }
#pragma omp parallel num_threads(T)
#pragma omp for
    for (int t = 0; t < T; t++) {
      for (; _it2.__op_neq(L2_neq[t], _end2);) {
        auto n = _it2.__op_star(L2_star[t]);
        result.__sum(L2_sum[t], n->value);

        for (auto m : n->outEdges) {
          if (!enqueued.contains(m)) {
            nextFrontier->__insert(L2_insert[t], m);
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
    skynet::clause_set_insert_bulk(T, &enqueued);
    skynet::clause_scalar_sum_bulk(T, &enqueued.storage_size_);
    skynet::clause_set_op_plusplus_bulk(T, &_it4);
    for (int t = 0; t < T; t++) {
      L4_insert[t] = t;
      L4_plusplus[t] = t;
      L4_neq[t] = t;
      L4_star[t] = t;
      L4_sum[t] = t;
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

#if defined(DEBUG) || defined(BFS_DEBUG)
  result.printInternals();
#endif

  return result.get();
}

int bfs_tc_manual_bulk_merge(const Graph &g, Node *root) {
  skynet::Set<Node *, skynet::SetT> enqueued;
  auto currentFrontier = new skynet::Set<Node *, skynet::VectorT>();
  auto nextFrontier = new skynet::Set<Node *, skynet::VectorT>();

  skynet::Scalar<int> result(0);
  currentFrontier->insert(root);
  enqueued.insert(root);

  int f_idx = 0;
  const int T = omp_get_max_threads();
#if defined(DEBUG) || defined(BFS_DEBUG)
  printf("T: %i\n", T);
#endif
  while (!currentFrontier->empty()) {
#if defined(DEBUG) || defined(BFS_DEBUG)
    cout << "--- processing frontier " << f_idx;
    cout << " (size=" << currentFrontier->size() << ")\n";
    cout << result.get() << "\n";
    result.printInternals();
    cout << "\n";
#endif
    auto _it2 = currentFrontier->begin();
    auto _end2 = currentFrontier->end();
    skynet::clause_set_insert_bulk(T, currentFrontier);
    skynet::clause_scalar_sum_bulk(T, &currentFrontier->storage_size_);
    skynet::clause_set_insert_bulk(T, nextFrontier);
    skynet::clause_scalar_sum_bulk(T, &nextFrontier->storage_size_);
    skynet::clause_set_op_plusplus_bulk(T, &_it2);
    skynet::clause_scalar_sum_bulk(T, &result);
#pragma omp parallel num_threads(T)
#pragma omp for
    for (int t = 0; t < T; t++) {
      for (; _it2.__op_neq(t, _end2);) {
        auto n = _it2.__op_star(t);
        result.__sum(t, n->value);

        for (auto m : n->outEdges) {
          if (!enqueued.contains(m)) {
            nextFrontier->__insert(t, m);
          }
        }

        _it2.__op_plusplus(t);
      }
    }

    enqueued.insert(*nextFrontier);
    currentFrontier->clear();
    swap(currentFrontier, nextFrontier);
    ++f_idx;
  }

  delete currentFrontier;
  delete nextFrontier;

#if defined(DEBUG) || defined(BFS_DEBUG)
  result.printInternals();
#endif

  return result.get();
}

int bfs_tc_manual_bulk_merge_leak(const Graph &g, Node *root) {
  skynet::Set<Node *, skynet::SetT> enqueued;
  auto currentFrontier = new skynet::Set<Node *, skynet::VectorT>();
  auto nextFrontier = new skynet::Set<Node *, skynet::VectorT>();

  skynet::Scalar<int> result(0);
  currentFrontier->insert(root);
  enqueued.insert(root);

  int f_idx = 0;
  const int T = omp_get_max_threads();
#if defined(DEBUG) || defined(BFS_DEBUG)
  printf("T: %i\n", T);
#endif
  skynet::clause_set_insert_bulk(T, currentFrontier);
  skynet::clause_scalar_sum_bulk(T, &currentFrontier->storage_size_);
  skynet::clause_set_insert_bulk(T, nextFrontier);
  skynet::clause_scalar_sum_bulk(T, &nextFrontier->storage_size_);
  skynet::clause_scalar_sum_bulk(T, &result);
  while (!currentFrontier->empty()) {
#if defined(DEBUG) || defined(BFS_DEBUG)
    cout << "--- processing frontier " << f_idx;
    cout << " (size=" << currentFrontier->size() << ")\n";
    cout << result.get() << "\n";
    result.printInternals();
    currentFrontier->printStats();
    cout << "\n";
#endif
    // auto _it2 = currentFrontier->begin();
    // auto _end2 = currentFrontier->end();
    // int L2_plusplus[T];
    // int L2_neq[T];
    // int L2_star[T];
    // int L2_sum[T];
    // int L2_insert[T];
    // skynet::clause_set_op_plusplus_bulk(T, &_it2);
#pragma omp parallel num_threads(T)
#pragma omp for
    for (int t = 0; t < T; t++) {
      auto &row = currentFrontier->container_[t];
      unordered_set<Node *> seen;
      auto seenEnd = seen.end();
      for (auto &cell : row) {
        for (auto &n : cell) {
          if (seen.find(n) == seenEnd) {
            seen.insert(n);
            result.__sum(t, n->value);
            for (auto m : n->outEdges) {
              if (!enqueued.contains(m)) {
                nextFrontier->__insert(t, m);
              }
            }
          }
        }
      }
    }

    enqueued.insert(*nextFrontier);
    currentFrontier->clear();
    swap(currentFrontier, nextFrontier);
    ++f_idx;
  }

  delete currentFrontier;
  delete nextFrontier;

#if defined(DEBUG) || defined(BFS_DEBUG)
  result.printInternals();
#endif

  return result.get();
}

int bfs_tc_manual_bulk_opt(const Graph &g, Node *root) {
  skynet::Set<Node *> enqueued;
  auto currentFrontier = new skynet::Set<Node *>();
  auto nextFrontier = new skynet::Set<Node *>();

  skynet::Scalar<int> result(0);
  currentFrontier->insert(root);
  enqueued.insert(root);

  int f_idx = 0;
  const int T = 16;
  skynet::clause_set_insert_bulk(T, currentFrontier);
  skynet::clause_scalar_sum_bulk(T, &currentFrontier->storage_size_);
  skynet::clause_set_insert_bulk(T, nextFrontier);
  skynet::clause_scalar_sum_bulk(T, &nextFrontier->storage_size_);
  skynet::clause_set_insert_bulk(T, &enqueued);
  skynet::clause_scalar_sum_bulk(T, &enqueued.storage_size_);
  skynet::clause_scalar_sum_bulk(T, &result);
  while (!currentFrontier->empty()) {
#if defined(DEBUG) || defined(BFS_DEBUG)
    cout << "--- processing frontier " << f_idx;
    cout << " (size=" << currentFrontier->size() << ")\n";
    cout << result.get() << "\n";
    result.printInternals();
    cout << "\n";
#endif
    auto _it2 = currentFrontier->begin();
    auto _end2 = currentFrontier->end();
    skynet::clause_set_op_plusplus_bulk(T, &_it2);
#pragma omp parallel num_threads(T)
#pragma omp for
    for (int t = 0; t < T; t++) {
      for (; _it2.__op_neq(t, _end2);) {
        auto n = _it2.__op_star(t);
        result.__sum(t, n->value);

        for (auto m : n->outEdges) {
          if (!enqueued.contains(m)) {
            nextFrontier->__insert(t, m);
          }
        }

        _it2.__op_plusplus(t);
      }
    }

    auto _it4 = nextFrontier->begin();
    auto _end4 = nextFrontier->end();
    skynet::clause_set_op_plusplus_bulk(T, &_it4);
#pragma omp parallel num_threads(T)
#pragma omp for
    for (int t = 0; t < T; t++) {
      for (; _it4.__op_neq(t, _end4);) {
        auto m = _it4.__op_star(t);

        enqueued.__insert(t, m);

        _it4.__op_plusplus(t);
      }
    }
    currentFrontier->clear();
    swap(currentFrontier, nextFrontier);
    ++f_idx;
  }

  delete currentFrontier;
  delete nextFrontier;

#if defined(DEBUG) || defined(BFS_DEBUG)
  result.printInternals();
#endif

  return result.get();
}

int bfs_tc(const Graph &g, Node *root) {
  skynet::Set<Node *> enqueued;
  auto currentFrontier = new skynet::Set<Node *>();
  auto nextFrontier = new skynet::Set<Node *>();

  skynet::Scalar<int> result(0);
  currentFrontier->insert(root);
  enqueued.insert(root);

  int f_idx = 0;
  auto p1 = noelle_pragma_begin("loop.tag", 1);
  while (!currentFrontier->empty()) {
#if defined(DEBUG) || defined(BFS_DEBUG)
    cout << "--- processing frontier " << f_idx;
    cout << " (size=" << currentFrontier->size() << ")\n";
    cout << result.get() << "\n";
    result.printInternals();
    cout << "\n";
#endif
    auto p2 = noelle_pragma_begin("loop.tag", 2);
    auto p21 = noelle_pragma_begin("loop.doall", "yes");
    for (auto n : *currentFrontier) {
      result.sum(n->value);

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
    // for (auto m : *nextFrontier) {
    //   enqueued.insert(m);
    // }
    enqueued.insert(*nextFrontier);
    noelle_pragma_end(p41);
    noelle_pragma_end(p4);
    currentFrontier->clear();
    swap(currentFrontier, nextFrontier);
    ++f_idx;
  }
  noelle_pragma_end(p1);

  delete currentFrontier;
  delete nextFrontier;

  return result.get();
}

int bfs_lockfree_no_omp(const Graph &g, Node *root) {
  using roster_t = unordered_set<Node *>;
  using frontier_t = vector<vector<Node *>>;

  unique_ptr<vector<frontier_t>> current_frontiers;
  unique_ptr<vector<frontier_t>> next_frontiers;
  vector<roster_t> rosters;

  int n_threads = omp_get_max_threads();
  rosters.resize(n_threads);
  current_frontiers = make_unique<vector<frontier_t>>(n_threads);
  next_frontiers = make_unique<vector<frontier_t>>(n_threads);

  for (int tid = 0; tid < n_threads; tid++) {
    (*current_frontiers)[tid].resize(n_threads);
    (*next_frontiers)[tid].resize(n_threads);
  }

  int sum = 0;

#ifdef STATS
  printf("n_threads: %i\n", n_threads);
#endif

  (*current_frontiers)[skynet::hasher(root) % n_threads][0].push_back(root);
  rosters[skynet::hasher(root) % n_threads].insert(root);

  bool has_work = true;
  while (has_work) {
#ifdef STATS
    printf("current_frontiers: ");
    for (int i = 0; i < n_threads; i++) {
      unsigned long int k = 0;
      for (int j = 0; j < n_threads; j++) {
        k += (*current_frontiers)[i][j].size();
      }
      printf("%4lu ", k);
    }
    printf("\n");
#endif
#ifdef STATS
    int frontier_skips = 0;
    int frontier_size = 0;
#endif

    has_work = false;
    for (int tid = 0; tid < n_threads; tid++) {
      auto &tid_current_frontier = (*current_frontiers)[tid];

      roster_t frontier_roster;
      for (auto &subfrontier : tid_current_frontier) {
        for (auto n : subfrontier) {

          if (frontier_roster.find(n) != frontier_roster.end()) {
#ifdef STATS
#  pragma omp atomic
            ++frontier_skips;
#endif
            continue;
          }
          sum += n->value;
          frontier_roster.insert(n);

          for (auto m : n->outEdges) {
            auto owner_tid = skynet::hasher(m) % n_threads;
            auto &owner_roster = rosters[owner_tid];
            auto not_in_roster = owner_roster.find(m) == owner_roster.end();
            if (not_in_roster) {
              (*next_frontiers)[owner_tid][tid].push_back(m);
              has_work = true;
            }
          }
        }
#ifdef STATS
#  pragma omp atomic
        frontier_size += subfrontier.size();
#endif
      }
    }

    for (int tid = 0; tid < n_threads; tid++) {
      auto &tid_current_frontier = (*current_frontiers)[tid];
      for (auto &subfrontier : tid_current_frontier) {
        subfrontier.clear();
      }
      auto &tid_roster = rosters[tid];
      auto &tid_next_frontier = (*next_frontiers)[tid];
      for (auto &subfrontier : tid_next_frontier) {
        for (auto n : subfrontier) {
          tid_roster.insert(n);
        }
      }
    }

#ifdef STATS
#  pragma omp single
    {
      printf("frontier redundancy: %.1lf %%\n",
             100. * (double)frontier_skips / (double)frontier_size);
      printf("next_frontiers:\n");
      for (int i = 0; i < n_threads; i++) {
        for (int j = 0; j < n_threads; j++) {
          printf("%4lu ", (*next_frontiers)[i][j].size());
        }
        printf("\n");
      }
      printf("-------------------------------------------\n");
    }
#endif
    swap(current_frontiers, next_frontiers);
  }

  return sum;
}

bool lockfree_contains(unordered_set<Node *> &roster, Node *m) {
  return roster.find(m) == roster.end();
}

int bfs_lockfree(const Graph &g, Node *root) {
  using roster_t = unordered_set<Node *>;
  using frontier_t = vector<vector<Node *>>;

  unique_ptr<vector<frontier_t>> current_frontiers;
  unique_ptr<vector<frontier_t>> next_frontiers;
  vector<roster_t> rosters;

  int n_threads = omp_get_max_threads();
  rosters.resize(n_threads);
  current_frontiers = make_unique<vector<frontier_t>>(n_threads);
  next_frontiers = make_unique<vector<frontier_t>>(n_threads);

#pragma omp parallel
  {
    int tid = omp_get_thread_num();
    (*current_frontiers)[tid].resize(n_threads);
    (*next_frontiers)[tid].resize(n_threads);
  }

  int sum = 0;

  (*current_frontiers)[skynet::hasher(root) % n_threads][0].push_back(root);
  rosters[skynet::hasher(root) % n_threads].insert(root);

  bool has_work = true;
  while (has_work) {
#ifdef STATS
    printf("current_frontiers: ");
    for (int i = 0; i < n_threads; i++) {
      unsigned long int k = 0;
      for (int j = 0; j < n_threads; j++) {
        k += (*current_frontiers)[i][j].size();
      }
      printf("%4lu ", k);
    }
    printf("\n");
#endif
#ifdef STATS
    int frontier_skips = 0;
    int frontier_size = 0;
#endif

    has_work = false;
#pragma omp parallel reduction(+ : sum) reduction(|| : has_work)
    {
      int tid = omp_get_thread_num();
      auto &tid_current_frontier = (*current_frontiers)[tid];

      roster_t frontier_roster;
      for (auto &subfrontier : tid_current_frontier) {
        for (auto n : subfrontier) {

          if (frontier_roster.find(n) != frontier_roster.end()) {
#ifdef STATS
#  pragma omp atomic
            ++frontier_skips;
#endif
            continue;
          }
          sum += n->value;
          frontier_roster.insert(n);

          for (auto m : n->outEdges) {
            auto owner_tid = skynet::hasher(m) % n_threads;
            auto &owner_roster = rosters[owner_tid];
            // auto not_in_roster = owner_roster.find(m) == owner_roster.end();
            bool not_in_roster = lockfree_contains(owner_roster, m);
            if (not_in_roster) {
              (*next_frontiers)[owner_tid][tid].push_back(m);
              has_work = true;
            }
          }
        }
#ifdef STATS
#  pragma omp atomic
        frontier_size += subfrontier.size();
#endif
      }

      for (auto &subfrontier : tid_current_frontier) {
        subfrontier.clear();
      }
#pragma omp barrier

#ifdef STATS
#  pragma omp single
      {
        printf("frontier redundancy: %.1lf %%\n",
               100. * (double)frontier_skips / (double)frontier_size);
        printf("next_frontiers:\n");
        for (int i = 0; i < n_threads; i++) {
          for (int j = 0; j < n_threads; j++) {
            printf("%4lu ", (*next_frontiers)[i][j].size());
          }
          printf("\n");
        }
        printf("-------------------------------------------\n");
      }
#endif

      auto &tid_roster = rosters[tid];
      auto &tid_next_frontier = (*next_frontiers)[tid];
      for (auto &subfrontier : tid_next_frontier) {
        for (auto n : subfrontier) {
          tid_roster.insert(n);
        }
      }

    } // pragma omp parallel
    swap(current_frontiers, next_frontiers);
  }

  return sum;
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

  // auto result_correct = bfs_tc(*g, g->nodes[0]);
  auto result_correct = 0;

  TIMER_START("Kernel");
  // auto result_obtained = bfs_tc_manual_bulk_opt(*g, g->nodes[0]);
  // auto result_obtained = bfs_tc_manual_bulk(*g, g->nodes[0]);
  // auto result_obtained = bfs_tc_manual_bulk_merge(*g, g->nodes[0]);
  auto result_obtained = bfs_tc_manual_bulk_merge_leak(*g, g->nodes[0]);
  // auto result_obtained = bfs_tc(*g, g->nodes[0]);
  // auto result_obtained = bfs_frontier(*g, g->nodes[0]);
  // auto result_obtained = bfs_lockfree(*g, g->nodes[0]);
  // auto result_obtained = bfs_lockfree_no_omp(*g, g->nodes[0]);
  TIMER_STOP();

  cout << "correct  = " << result_correct << "\n";
  cout << "obtained = " << result_obtained << "\n";

  delete g;

  TIMER_STOP();

  return 0;
}
