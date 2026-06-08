#pragma once

// #include <absl/container/btree_map.h>
#include <atomic>
#include <iostream>
#include <map>
#include <mutex>
#include <type_traits>
#include <unordered_map>
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

template <typename Tk,
          typename Tv,
          uint32_t PAD = compute_padding<Bucket<Tv>>()>
class MultimapTBB;

template <typename Tk, typename Tv, uint32_t PAD>
class MultimapTBB {
public:
  using BucketT = Bucket<Tv>;
  using MapT = oneapi::tbb::concurrent_hash_map<Tk, BucketT>;
  using MapIteratorT = typename MapT::iterator;

  MultimapTBB() : container_(PAD) {}

  ~MultimapTBB() {}

  void insert(Tk key, Tv value) {
    int k = 0;
    auto p = noelle_pragma_begin("ldtc", &k, 0);
    __insert(k, key, value);
    noelle_pragma_end(p);
  }

  void __insert(int /*t*/, Tk key, Tv value) {
    typename MapT::accessor acc;
    container_[0 * PAD].insert(
        acc,
        key); // atomic find-or-insert with exclusive lock
    acc->second.push_back(value);
  }

  Tk minKey() {
    Tk current_min = std::numeric_limits<Tk>::max();
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
    Tk current_max = std::numeric_limits<Tk>::min();
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

  void reduce_seq() { // untested
    const size_t P = container_.size() / PAD;
    auto &dst = container_[0];
    for (int i = 1; i < P; i++) {
      for (auto &[key, bucket] : container_[i * PAD]) {
        for (const auto &e : bucket) {
          dst[key].push_back(e);
        }
      }
    }
    container_.resize(1 * PAD);
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
      typename MapT::const_accessor acc;
      if (block.find(acc, key)) {
        auto &current_bucket = acc->second;
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
      typename MapT::const_accessor acc;
      if (block.find(acc, key)) {
        counter += acc->second.size();
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
