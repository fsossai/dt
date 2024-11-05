#pragma once

#include <cstddef>
#include <vector>
#include <unordered_map>

using NodeId = size_t;

struct Node {
  int value;
  int level;
};

struct Edge {
  NodeId src;
  NodeId dst;
};

class EdgeIterator;

struct Graph {
  Graph(const char *input_name);
  ~Graph();
  Graph(const Graph &other) = delete;
  Graph(Graph &&other) = default;

  void printStats();

  EdgeIterator outgoingEdges(Node *n) const;

  Node *getRoot();

  Edge *edges;
  std::unordered_map<NodeId, size_t> nIdOffsets;
  std::unordered_map<NodeId, Node *> nIdToNode;
  std::unordered_map<Node *, NodeId> nodeToNId;

  size_t N;
  size_t M;

  size_t getOffset(NodeId nId) const;

private:
  size_t edges_buffer_size_;
};

class EdgeIterator {
public:
  EdgeIterator(const Graph &graph,
               NodeId nId,
               size_t idx,
               size_t startIdx,
               size_t endIdx)
    : graph(graph),
      nId(nId),
      idx(idx),
      startIdx(startIdx),
      endIdx(endIdx),
      offset(graph.getOffset(nId)) {}

  bool operator!=(const EdgeIterator &other) {
    return idx != other.endIdx;
  }

  EdgeIterator begin() const {
    return EdgeIterator(graph, nId, startIdx, startIdx, endIdx);
  }

  EdgeIterator end() const {
    return EdgeIterator(graph, nId, endIdx, startIdx, endIdx);
  }

  Node *operator*() const {
    return graph.nIdToNode.at(graph.edges[offset + idx].dst);
  }

  EdgeIterator &operator++() {
    idx++;
    return *this;
  }

  size_t size() const {
    return endIdx - idx;
  }

private:
  const Graph &graph;
  NodeId nId;
  size_t idx;
  size_t startIdx;
  size_t endIdx;
  size_t offset;
};
