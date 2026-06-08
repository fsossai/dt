#pragma once

// #include <absl/container/btree_map.h>
#include <iostream>
#include <limits>
#include <vector>
#include "oneapi/tbb.h"

#include "Interface.hpp"
#include "IV.hpp"
#include "Range.hpp"
#include "BaseSequence.hpp"

#include "arcana/noelle/core/Pragma.h"

namespace skynet {

template <typename T>
using Bucket = std::vector<T>;

template <typename Tk, typename Tv>
class MultimapTBB {
public:
  using BucketT = Bucket<Tv>;
  using MapT = oneapi::tbb::concurrent_hash_map<Tk, BucketT>;
  using MapIteratorT = typename MapT::iterator;

  MultimapTBB() {}

  ~MultimapTBB() {}

  void insert(Tk key, Tv value) {
    int k = 0;
    auto p = noelle_pragma_begin("ldtc", &k, 0);
    __insert(k, key, value);
    noelle_pragma_end(p);
  }

  void __insert(int /*t*/, Tk key, Tv value) {
    typename MapT::accessor acc;
    container_.insert(acc, key); // atomic find-or-insert with exclusive lock
    acc->second.push_back(value);
  }

  Tk minKey() {
    Tk current_min = std::numeric_limits<Tk>::max();
    for (auto &[key, _] : container_) {
      if (key < current_min) {
        current_min = key;
      }
    }
    return current_min;
  }

  Tk maxKey() {
    Tk current_max = std::numeric_limits<Tk>::min();
    for (auto &[key, _] : container_) {
      if (key > current_max) {
        current_max = key;
      }
    }
    return current_max;
  }

  void erase(Tk key) {
    container_.erase(key);
  }

  void reduce_seq() {}

  void printInternals() {
    std::cout << "{\n  { ";
    for (auto &[key, bucket] : container_) {
      std::cout << "(" << key << ", { ";
      for (auto &x : bucket) {
        std::cout << x << " ";
      }
      std::cout << "})";
    }
    std::cout << " }\n}\n";
  }

  BucketT operator[](Tk key) {
    typename MapT::const_accessor acc;
    if (!container_.find(acc, key)) {
      return {};
    }
    return acc->second;
  }

  size_t count(Tk key) {
    typename MapT::const_accessor acc;
    if (!container_.find(acc, key)) {
      return 0;
    }
    return acc->second.size();
  }

  bool empty() {
    for (auto &[_, bucket] : container_) {
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
