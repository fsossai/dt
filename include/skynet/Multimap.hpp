#pragma once

// #include <absl/container/btree_map.h>
#include <atomic>
#include <iostream>
#include <map>
#include <mutex>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "Interface.hpp"
#include "IV.hpp"
#include "Range.hpp"
#include "BaseSequence.hpp"

#include "arcana/noelle/core/Pragma.h"

namespace skynet {

template <typename T>
using Bucket = std::vector<T>;

template <typename Tk, typename Tv, uint32_t PAD = compute_padding<Bucket<Tv>>()>
class Multimap;

template <typename Tk, typename Tv, uint32_t PAD>
void clause_multimap_insert(int N, Multimap<Tk, Tv, PAD> *mmap) {
  N *= PAD;
  if (mmap->container_.size() == N) {
    return;
  }
  mmap->container_.resize(N);
}

template <typename Tk, typename Tv, uint32_t PAD>
class Multimap {
public:
  using BucketT = Bucket<Tv>;
  using MapT = std::unordered_map<Tk, BucketT>;
  using MapIteratorT = typename MapT::iterator;

  friend void clause_multimap_insert<Tk, Tv, PAD>(int N,
                                                  Multimap<Tk, Tv, PAD> *mmap);

  Multimap() : container_(PAD) {}

  ~Multimap() {}

  void insert(Tk key, Tv value) {
    int k = 0;
    auto p = noelle_pragma_begin("ldtc",
                                 &k,
                                 0,
                                 clause_multimap_insert<Tk, Tv, PAD>,
                                 this);
    __insert(k, key, value);
    noelle_pragma_end(p);
  }

  void __insert(int t, Tk key, Tv value) {
    auto it = container_[t * PAD].find(key);
    if (it != container_[t * PAD].end()) {
      it->second.push_back(value);
    } else {
      container_[t * PAD].emplace(key, BucketT({ value }));
    }
  }

  Tk minKey() {
    Tk current_min = std::numeric_limits<Tk>::min();
#pragma omp parallel for reduction(min : current_min)
    for (int i = 0; i < container_.size() / PAD; i++) {
      auto &block = container_[i * PAD];
      for (auto &[key, _] : block) {
        if (key < current_min) {
          current_min = key;
        }
      }
    }
    return current_min;
  }

  Tk maxKey() {
    Tk current_max = std::numeric_limits<Tk>::max();
#pragma omp parallel for reduction(max : current_max)
    for (int i = 0; i < container_.size() / PAD; i++) {
      auto &block = container_[i * PAD];
      for (auto &[key, _] : block) {
        if (key > current_max) {
          current_max = key;
        }
      }
    }
    return current_max;
  }

  void erase(Tk key) {
#pragma omp parallel for
    for (int i = 0; i < container_.size() / PAD; i++) {
      container_[i * PAD].erase(key);
    }
  }

  void printInternals() {
    std::cout << "{\n";
    for (int i = 0; i < container_.size() / PAD; i++) {
      auto &block = container_[i * PAD];
      std::cout << "  { ";
      for (auto &[key, bucket] : block) {
        std::cout << "(" << key << ", { ";
        for (auto &x : bucket) {
          std::cout << x << " ";
        }
        std::cout << "})";
      }
      std::cout << " }\n";
    }
    std::cout << "}\n";
  }

  BucketT operator[](Tk key) {
    BucketT full_bucket(count(key));
    std::atomic<size_t> offset(0);
#pragma omp parallel for
    for (int i = 0; i < container_.size() / PAD; i++) {
      auto &block = container_[i * PAD];
      auto it = block.find(key);
      if (it != block.end()) {
        auto &current_bucket = it->second;
        auto start =
            offset.fetch_add(current_bucket.size(), std::memory_order_relaxed);

        std::copy(current_bucket.begin(),
                  current_bucket.end(),
                  full_bucket.data() + start);
      }
    }
    return full_bucket;
  }

  size_t count(Tk key) {
    size_t counter = 0;
#pragma omp parallel for reduction(+ : counter)
    for (int i = 0; i < container_.size() / PAD; i++) {
      auto &block = container_[i * PAD];
      auto it = block.find(key);
      if (it != block.end()) {
        counter += it->second.size();
      }
    }
    return counter;
  }

  bool empty() {
    for (int i = 0; i < container_.size() / PAD; i++) {
      auto &block = container_[i * PAD];
      if (block.size() != 0) {
        for (auto &[_, bucket] : block) {
          if (bucket.size() != 0) {
            return false;
          }
        }
      }
    }
    return true;
  }

  // private:
  std::vector<MapT> container_;
};

} // namespace skynet
