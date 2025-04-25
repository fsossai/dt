#pragma once

#include <absl/container/btree_map.h>
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

template <typename Tk, typename Tv>
class UMultimap;

template <typename Tk, typename Tv>
void clause_umultimap_insert(int N, UMultimap<Tk, Tv> *mmap) {
  if (mmap->container_.size() == N) {
    return;
  }
  mmap->container_.resize(N);
}

template <typename Tk, typename Tv>
class UMultimap {
public:
  using BucketT = std::vector<Tv>;
  using MapT = std::unordered_map<Tk, BucketT>;
  using MapIteratorT = typename MapT::iterator;

  friend void clause_umultimap_insert<Tk, Tv>(int N, UMultimap<Tk, Tv> *mmap);

  // class KeysIterator {
  // public:
  //   KeysIterator(MapIteratorT it) : it_(move(it)) {}
  //
  //   Tk operator*() {
  //     return it_->first;
  //   }
  //
  //   KeysIterator &operator++() {
  //     ++it_;
  //     return *this;
  //   }
  //
  //   bool operator!=(const KeysIterator &other) const {
  //     return it_ != other.it_;
  //   }
  //
  //   typename MapT::difference_type operator-(const KeysIterator &other) const
  //   {
  //     return it_ - other.it_;
  //   }
  //
  //   KeysIterator operator+(int64_t a) const {
  //     return { it_ + a };
  //   }
  //
  // private:
  //   MapIteratorT it_;
  // };

  UMultimap() : container_(1) {}

  ~UMultimap() {}

  void insert(Tk key, const Tv &value) {
    int k = 0;
    auto p = noelle_pragma_begin("ldtc",
                                 &k,
                                 0,
                                 clause_umultimap_insert<Tk, Tv>,
                                 this);
    __insert(k, key, value);
    noelle_pragma_end(p);
  }

  void __insert(int t, Tk key, const Tv &value) {
    auto it = container_[t].find(key);
    if (it != container_[t].end()) {
      it->second.push_back(value);
    } else {
      container_[t].emplace(key, BucketT({ value }));
    }
  }

  Tk minKey() {
    Tk current_min = std::numeric_limits<Tk>::min();
    for (auto &block : container_) {
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
    for (auto &block : container_) {
      for (auto &[key, _] : block) {
        if (key > current_max) {
          current_max = key;
        }
      }
    }
    return current_max;
  }

  INLINE void erase(Tk key) {
    for (auto &block : container_) {
      block.erase(key);
    }
  }

  void printKeys() {
    std::cout << "{ ";
    // TODO
    std::cout << "}\n";
  }

  void printInternals() {
    std::cout << "{\n";
    for (auto &block : container_) {
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
    BucketT full_bucket;
    for (auto &block : container_) {
      auto it = block.find(key);
      if (it != block.end()) {
        auto &current_bucket = it->second;
        full_bucket.insert(full_bucket.end(),
                           current_bucket.begin(),
                           current_bucket.end());
      }
    }
    return full_bucket;
  }

  size_t count(Tk key) {
    size_t count = 0;
    for (auto &block : container_) {
      auto it = block.find(key);
      if (it != block.end()) {
        count += it->second.size();
      }
    }
    return count;
  }

  bool empty() {
    for (auto &block : container_) {
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

  void print() const {
    // TODO
  }

  // private:
  std::vector<MapT> container_;
};

} // namespace skynet
