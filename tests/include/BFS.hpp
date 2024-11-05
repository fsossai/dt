#pragma once

#include <queue>
#include <set>

#include "Graph.hpp"

// sequential
int bfs_frontier_post(const Graph &g, Node root);
int bfs_frontier_pre(const Graph &g, Node root);
int bfs_queue_post(const Graph &g, Node root);
int bfs_queue_pre(const Graph &g, Node root);
int bfs_regq_post(const Graph &g, Node root);
int bfs_regq_pre(const Graph &g, Node root);
int bfs_ring_post(const Graph &g, Node root);
int bfs_ring_pre(const Graph &g, Node root);
int bfs_unordered_pre(const Graph &g, Node root);
int bfs_vector_post(const Graph &g, Node root);
int bfs_vector_pre(const Graph &g, Node root);
int bfs_window_post(const Graph &g, Node root);
int bfs_window_pre(const Graph &g, Node root);


// parallel
int bfs_critical(const Graph &g, Node root);
int bfs_lockfree(const Graph &g, Node root);
int bfs_owner(const Graph &g, Node root);
