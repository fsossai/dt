#pragma once

// #include <absl/container/btree_map.h>
#include <atomic>
#include <iostream>
#include <map>
#include <mutex>
#include <oneapi/tbb/concurrent_unordered_map.h>
#include <oneapi/tbb/concurrent_vector.h>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "Interface.hpp"
#include "IV.hpp"
#include "Range.hpp"
#include "BaseSequence.hpp"

#include "arcana/noelle/core/Pragma.h"

#include <oneapi/tbb.h>

namespace skynet {

template <typename Tk, typename Tv>
class CMultimap;

template <typename Tk, typename Tv>
void clause_Ccmultimap_insert(int N, CMultimap<Tk, Tv> *mmap) {}

template <typename Tk, typename Tv>
class CMultimap {
public:
  using BucketT = oneapi::tbb::concurrent_vector<Tv>;
  using MapT = oneapi::tbb::concurrent_unordered_map<Tk, BucketT>;
  using MapIteratorT = typename MapT::iterator;

  friend void clause_Ccmultimap_insert<Tk, Tv>(int N, CMultimap<Tk, Tv> *mmap);

  CMultimap() : container_(1) {}

  ~CMultimap() {}

  void insert(Tk key, Tv value) {
    int k = 0;
    auto p = noelle_pragma_begin("ldtc",
                                 &k,
                                 0,
                                 clause_Ccmultimap_insert<Tk, Tv>,
                                 this);
    __insert(k, key, value);
    noelle_pragma_end(p);
  }

  void __insert(int /*t*/, Tk key, Tv value) {
    container_[key].push_back(value);
  }

  Tk minKey() {
    Tk current_min = std::numeric_limits<Tk>::min();
    for (auto &[key, _] : container_) {
      if (key < current_min) {
        current_min = key;
      }
    }
    return current_min;
  }

  Tk maxKey() {
    Tk current_max = std::numeric_limits<Tk>::max();
    for (auto &[key, _] : container_) {
      if (key > current_max) {
        current_max = key;
      }
    }
    return current_max;
  }

  void erase(Tk key) {
    auto it = container_.find(key);
    if (it != container_.end()) {
      it->second.clear();
    }
  }

  void printInternals() {
    std::cout << "{\n";
    for (auto &[key, bucket] : container_) {
      std::cout << "(" << key << ", { ";
      for (auto &x : bucket) {
        std::cout << x << " ";
      }
      std::cout << "})";
    }
    std::cout << " }\n";
  }

  BucketT operator[](Tk key) {
    return container_[key];
  }

  size_t count(Tk key) {
    size_t counter = 0;
    for (auto &[key, bucket] : container_) {
      counter += bucket.size();
    }
    return counter;
  }

  bool empty() {
    for (auto &[key, bucket] : container_) {
      if (bucket.size() != 0) {
        return false;
      }
    }
    return true;
  }

  // private:
  MapT container_;
};

} // namespace skynet
