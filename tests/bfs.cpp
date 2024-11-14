#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h> // for open
#include <iostream>
#include <iterator>
#include <memory>
#include <omp.h>
#include <pthread.h>
#include <queue>
#include <sched.h>
#include <set>
#include <stack>
#include <sys/mman.h> // for mmap, munmap
#include <sys/stat.h> // for fstat
#include <unistd.h>   // for close
#include <unordered_set>
#include <vector>

#include "ScopeTimer.hpp"
#include "Skynet.hpp"
#include "arcana/noelle/core/Pragma.h"

#include "include/Graph.hpp"

using namespace std;

Graph::Graph(const char *input_name) : edges_buffer_size_(0) {
  // Open the file
  int fd = open(input_name, O_RDONLY);
  if (fd == -1) {
    perror("Error opening file");
    abort();
    return;
  }

  // Get the file size
  struct stat sb;
  if (fstat(fd, &sb) == -1) {
    perror("Error getting file size");
    close(fd);
    return;
  }

  // Memory-map the file
  size_t length = sb.st_size;
  Edge *input_graph =
      (Edge *)mmap(nullptr, length, PROT_READ, MAP_PRIVATE, fd, 0);
  if (input_graph == MAP_FAILED) {
    perror("ERROR: map failed");
    close(fd);
    abort();
  }

  // Read from the mapped memory
  N = input_graph[0].src;
  M = input_graph[0].dst;

  // Test for corruption
  size_t real_M = (length / sizeof(Edge)) - 1;
  if (M != real_M) {
    fprintf(stderr, "ERROR: size mismatch in input graph\n");
    close(fd);
    abort();
  }

  edges = (Edge *)&input_graph[1];
  close(fd);

  NodeId last_src = 0;
  for (size_t i = 0; i < M; i++) {
    if (edges[i].src != last_src) {
      last_src = edges[i].src;
      nIdOffsets[last_src] = i;
    }
  }
  nIdOffsets[N + 1] = M;

  for (NodeId i = 1; i <= N; i++) {
    Node *n = new Node;
    n->value = i;
    nIdToNode[i] = n;
    nodeToNId[n] = i;
  }
}

Node *Graph::getRoot() {
  return nIdToNode.at(edges[0].src);
}

Graph::~Graph() {
  for (auto [_, node] : nIdToNode) {
    free(node);
  }
  if (edges_buffer_size_ == 0) {
    return;
  }

  if (munmap(&edges[-1], edges_buffer_size_) == -1) {
    perror("ERROR: unmap failed");
  }
}

EdgeIterator Graph::outgoingEdges(Node *n) const {
  auto nId = nodeToNId.at(n);
  auto nEdges = getOffset(nId + 1) - getOffset(nId);
  return EdgeIterator(*this, nId, 0, 0, nEdges);
}

size_t Graph::getOffset(NodeId nId) const {
  auto it = nIdOffsets.find(nId);
  if (it == nIdOffsets.end()) {
    return getOffset(nId + 1);
  } else {
    return it->second;
  }
}

void Graph::printStats() {
  float avg = (float)M / (float)N;
  printf("Nodes           = %lu\n", N);
  printf("Edges           = %lu\n", M);
  printf("Average degree  = %.2f\n", avg);
}

int bfs_frontier(const Graph &g, Node *root) {
  unordered_set<Node *> enqueued;
  auto currentFrontier = new vector<Node *>();
  auto nextFrontier = new vector<Node *>();

  int t = 0;
  currentFrontier->push_back(root);
  enqueued.insert(root);

  while (!currentFrontier->empty()) {
    for (auto *n : *currentFrontier) {
      t += n->value;

      for (auto *m : g.outgoingEdges(n)) {
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

int bfs_manual(const Graph &g, Node *root) {
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
#ifdef PADDING
  constexpr int PAD = 16;
#else
  constexpr int PAD = 1;
#endif
  while (!currentFrontier->empty()) {
#if defined(DEBUG) || defined(BFS_DEBUG)
    cout << "--- processing frontier " << f_idx;
    cout << " (size=" << currentFrontier->size() << ")\n";
    cout << result.get() << "\n";
    result.printInternals();
    currentFrontier->printStats();
    cout << "\n";
#endif
#ifdef BFS_TIMING
    TIMER_START("Frontier");
#endif
    auto _it2 = currentFrontier->begin();
    auto _end2 = currentFrontier->end();
    skynet::clause_set_insert(T, nextFrontier);
    skynet::clause_set_op_plusplus(T, &_it2);
    skynet::clause_scalar_sum(T * PAD, &result);
#pragma omp parallel num_threads(T)
#pragma omp for
    for (int t = 0; t < T; t++) {
      for (; _it2.__op_neq(t, _end2);) {
        auto *n = _it2.__op_star(t);
        result.__sum(t * PAD, n->value);

        for (auto *m : g.outgoingEdges(n)) {
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
#ifdef BFS_TIMING
    TIMER_STOP();
#endif
  }

  delete currentFrontier;
  delete nextFrontier;

#if defined(DEBUG) || defined(BFS_DEBUG)
  result.printInternals();
#endif

  return result.get();
}

typedef struct {
  int t;
  const Graph *g;
  skynet::Set<Node *, skynet::VectorT> *currentFrontier;
  skynet::Set<Node *, skynet::VectorT> *nextFrontier;
  skynet::Set<Node *, skynet::SetT> *enqueued;
  skynet::Scalar<int> *result;
  skynet::SetIterator<Node *, skynet::VectorT> *_it2;
  skynet::SetIterator<Node *, skynet::VectorT> *_end2;

} bfs_pthreads_kernel_args;

void *bfs_pthreads_kernel(void *p) {
  auto args = (bfs_pthreads_kernel_args *)p;

  for (; args->_it2->__op_neq(args->t, *args->_end2);) {
    auto *n = args->_it2->__op_star(args->t);
    args->result->__sum(args->t * PAD, n->value);

    for (auto *m : args->g->outgoingEdges(n)) {
      if (!args->enqueued->contains(m)) {
        args->nextFrontier->__insert(args->t, m);
      }
    }

    args->_it2->__op_plusplus(args->t);
  }
  return nullptr;
}

int bfs_pthreads(const Graph &g, Node *root) {
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
#ifdef PADDING
  constexpr int PAD = 16;
#else
  constexpr int PAD = 1;
#endif
  while (!currentFrontier->empty()) {
#if defined(DEBUG) || defined(BFS_DEBUG)
    cout << "--- processing frontier " << f_idx;
    cout << " (size=" << currentFrontier->size() << ")\n";
    cout << result.get() << "\n";
    result.printInternals();
    currentFrontier->printStats();
    cout << "\n";
#endif
    auto _it2 = currentFrontier->begin();
    auto _end2 = currentFrontier->end();
    skynet::clause_set_insert(T, nextFrontier);
    skynet::clause_set_op_plusplus(T, &_it2);
    skynet::clause_scalar_sum(T * PAD, &result);

    pthread_t threads[T];
    bfs_pthreads_kernel_args args[T];

#ifdef BFS_TIMING
    TIMER_START("Frontier");
#endif

    int rc;
    for (int t = 0; t < T; t++) {
      args[t].t = t;
      args[t].g = &g;
      args[t].currentFrontier = currentFrontier;
      args[t].nextFrontier = nextFrontier;
      args[t].result = &result;
      args[t]._it2 = &_it2;
      args[t]._end2 = &_end2;
      args[t].enqueued = &enqueued;
      rc = pthread_create(&threads[t],
                          NULL,
                          bfs_pthreads_kernel,
                          (void *)&args[t]);
      if (rc != 0) {
        cout << "ERROR: pthread_create()\n";
        return 0;
      }
      cpu_set_t cpuset;
      CPU_ZERO(&cpuset);
      CPU_SET(t * 2, &cpuset); // balanced single-socket pinning

      rc = pthread_setaffinity_np(threads[t], sizeof(cpu_set_t), &cpuset);
      if (rc != 0) {
        cout << "ERROR: pthread_setaffinity_np()\n";
        return 0;
      }
    }

    for (int t = 0; t < T; t++) {
      pthread_join(threads[t], NULL);
    }

    enqueued.insert(*nextFrontier);
    currentFrontier->clear();
    swap(currentFrontier, nextFrontier);
    ++f_idx;

#ifdef BFS_TIMING
    TIMER_STOP();
#endif
  }

  delete currentFrontier;
  delete nextFrontier;

#if defined(DEBUG) || defined(BFS_DEBUG)
  result.printInternals();
#endif

  return result.get();
}

int bfs_manual_opt(const Graph &g, Node *root) {
  skynet::Set<Node *, skynet::SetT> enqueued;
  auto currentFrontier = new skynet::Set<Node *, skynet::VectorT>();
  auto nextFrontier = new skynet::Set<Node *, skynet::VectorT>();

  skynet::Scalar<int> result(0);
  currentFrontier->insert(root);
  enqueued.insert(root);

  int f_idx = 0;
  const int T = omp_get_max_threads();
#ifdef PADDING
  constexpr int PAD = 16;
#else
  constexpr int PAD = 1;
#endif
#if defined(DEBUG) || defined(BFS_DEBUG)
  printf("T: %i\n", T);
#endif
  skynet::clause_set_insert(T, currentFrontier);
  skynet::clause_set_insert(T, nextFrontier);
  skynet::clause_scalar_sum(T * PAD, &result);
  while (!currentFrontier->empty()) {
#if defined(DEBUG) || defined(BFS_DEBUG)
    cout << "--- processing frontier " << f_idx;
    cout << " (size=" << currentFrontier->size() << ")\n";
    cout << result.get() << "\n";
    result.printInternals();
    currentFrontier->printStats();
    cout << "\n";
#endif
#pragma omp parallel num_threads(T)
#pragma omp for
    for (int t = 0; t < T; t++) {
      auto &row = currentFrontier->container_[t];
      unordered_set<Node *> seen;
      auto seenEnd = seen.end();
      for (auto &cell : row) {
        for (auto *n : cell) {
          if (seen.find(n) == seenEnd) {
            seen.insert(n);
            result.__sum(t * PAD, n->value);
            for (auto *m : g.outgoingEdges(n)) {
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

int bfs_tc(const Graph &g, Node *root) {
  skynet::Set<Node *, skynet::SetT> enqueued;
  auto currentFrontier = new skynet::Set<Node *, skynet::VectorT>();
  auto nextFrontier = new skynet::Set<Node *, skynet::VectorT>();

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
#ifdef BFS_TIMING
    TIMER_START("Frontier");
#endif
    auto p2 = noelle_pragma_begin("loop.tag", 2);
    auto p21 = noelle_pragma_begin("loop.doall", "yes");
    for (auto *n : *currentFrontier) {
      result.sum(n->value);

      auto p3 = noelle_pragma_begin("loop.tag", 3);
      auto p31 = noelle_pragma_begin("loop.doall", "maybe");
      for (auto *m : g.outgoingEdges(n)) {
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

    enqueued.insert(*nextFrontier);
    noelle_pragma_end(p41);
    noelle_pragma_end(p4);
    currentFrontier->clear();
    swap(currentFrontier, nextFrontier);
    ++f_idx;
#ifdef BFS_TIMING
    TIMER_STOP();
#endif
  }
  noelle_pragma_end(p1);

  delete currentFrontier;
  delete nextFrontier;

  return result.get();
}

bool lockfree_contains(unordered_set<Node *> &roster, Node *m) {
  return roster.find(m) == roster.end();
}

int bfs_omp(const Graph &g, Node *root) {
  using roster_t = unordered_set<Node *>;
  using frontier_t = vector<vector<Node *>>;

  unique_ptr<vector<frontier_t>> current_frontiers;
  unique_ptr<vector<frontier_t>> next_frontiers;
  vector<roster_t> rosters;

  int n_threads = omp_get_max_threads();
  rosters.resize(n_threads);
  current_frontiers = make_unique<vector<frontier_t>>(n_threads);
  next_frontiers = make_unique<vector<frontier_t>>(n_threads);

#ifdef PADDING
  const int PAD = 16;
#else
  const int PAD = 1;
#endif

  int sum[n_threads * PAD];

#pragma omp parallel
  {
    int tid = omp_get_thread_num();
    (*current_frontiers)[tid].resize(n_threads);
    (*next_frontiers)[tid].resize(n_threads);
    sum[tid * PAD] = 0;
  }

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
#ifdef BFS_TIMING
    TIMER_START("Frontier");
#endif

    has_work = false;
#pragma omp parallel reduction(|| : has_work)
    {
      int tid = omp_get_thread_num();
      auto &tid_current_frontier = (*current_frontiers)[tid];

      roster_t frontier_roster;
      for (auto &subfrontier : tid_current_frontier) {
        for (auto *n : subfrontier) {

          if (frontier_roster.find(n) != frontier_roster.end()) {
#ifdef STATS
#  pragma omp atomic
            ++frontier_skips;
#endif
            continue;
          }
          sum[tid * PAD] += n->value;
          frontier_roster.insert(n);

          for (auto *m : g.outgoingEdges(n)) {
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
#ifdef BFS_TIMING
    TIMER_STOP();
#endif
  }

  int reduced_sum = sum[0];
  for (int i = 1; i < n_threads; i++) {
    reduced_sum += sum[i * PAD];
  }

  return reduced_sum;
}

int main(int argc, char *argv[]) {
  const char *input_file = nullptr;
  if (argc > 1) {
    input_file = argv[1];
  } else {
    fprintf(stderr, "usage: %s <INPUT_GRAPH>\n", argv[0]);
    return 1;
  }
  srand(0);

  TIMER_START("Total");

  TIMER_START("Build");
  auto g = Graph(input_file);
  TIMER_STOP();

  g.printStats();

  auto root = g.getRoot();

  TIMER_START("Kernel");
#ifdef BFS_FRONTIER
  int result = bfs_frontier(g, g.getRoot());
#elif defined BFS_MANUAL
  int result = bfs_manual(g, g.getRoot());
#elif defined BFS_MANUAL_OPT
  int result = bfs_manual_opt(g, g.getRoot());
#elif defined BFS_OMP
  int result = bfs_omp(g, g.getRoot());
#elif defined BFS_PTHREADS
  int result = bfs_pthreads(g, g.getRoot());
#else
  int result = bfs_tc(g, g.getRoot());
#endif
  cout << "res = " << result << endl;
  TIMER_STOP();

  TIMER_STOP();

  return 0;
}
