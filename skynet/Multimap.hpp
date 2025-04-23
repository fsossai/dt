#pragma once

#include <atomic>
#include <iostream>
#include <type_traits>
#include <vector>
#include <oneapi/tbb.h>

#include "Interface.hpp"
#include "IV.hpp"
#include "Range.hpp"
#include "BaseSequence.hpp"

#include "arcana/noelle/core/Pragma.h"

namespace skynet {

template <typename Tk, typename Tv, bool Order>
class Multimap;

template <typename Tk, typename Tv, bool Order>
void clause_multiset_insert(int N, Multimap<Tk, Tv, Order> *mmap) {
  // TODO
  mmap->N_ = N;

  for (auto &[key, bucket] : mmap->container_) {
    clause_sequence_append(N, &bucket);
  }
}

template <typename Tk, typename Tv, bool Order>
class Multimap {
public:
  using BucketT = BaseSequence<Tv, /*Order*/ false>;
  using MapT = typename std::conditional_t<
      Order,
      oneapi::tbb::concurrent_map<Tk, BucketT>,
      oneapi::tbb::concurrent_unordered_map<Tk, BucketT>>;
  using MapIteratorT = typename MapT::iterator;

  friend void clause_multiset_insert<Tk, Tv>(int N,
                                             Multimap<Tk, Tv, Order> *mmap);

  class KeysIterator {
  public:
    KeysIterator(MapIteratorT it) : it_(move(it)) {}

    Tk operator*() {
      return it_->first;
    }

    KeysIterator &operator++() {
      ++it_;
      return *this;
    }

    bool operator!=(const KeysIterator &other) const {
      return it_ != other.it_;
    }

    typename MapT::difference_type operator-(const KeysIterator &other) const {
      return it_ - other.it_;
    }

    KeysIterator operator+(int64_t a) const {
      return { it_ + a };
    }

  private:
    MapIteratorT it_;
  };

  class KeysView {
  public:
    KeysView(Multimap<Tk, Tv, Order> &mmap) : mmap_(mmap) {}

    KeysIterator begin() {
      return { mmap_.container_.begin() };
    }

    KeysIterator end() {
      return { mmap_.container_.end() };
    }

  private:
    Multimap<Tk, Tv, Order> &mmap_;
  };

  // TODO
  Multimap() = default;

  ~Multimap() {}

  void insert(Tk key, const Tv &value) {
    int k = N_ - 1;
    auto p = noelle_pragma_begin("ldtc",
                                 &k,
                                 0,
                                 clause_multiset_insert<Tk, Tv, Order>,
                                 this);
    auto it = container_.find(key);
    if (it != container_.end()) {
      BucketT &bucket = it->second;
      bucket.__insert(k, value);
    } else {
      BucketT bucket;
      clause_sequence_append(N_, &bucket);
      bucket.__insert(k, value);
      container_.insert({ key, std::move(bucket) });
    }

    noelle_pragma_end(p);
  }

  size_t numKeys() const {
    return container_.size();
  }

  KeysView keys() {
    return { *this };
  }

  template <typename R = KeysView>
  typename std::enable_if_t<Order, R> sortedKeys() {
    return { *this };
  }

  void printKeys() {
    std::cout << "{ ";
    for (auto k : keys()) {
      std::cout << k << " ";
    }
    std::cout << "}\n";
  }

  bool hasKey(Tk key) const {
    return container_.contains(key);
  }

  BucketT &operator[](Tk key) {
    return container_[key];
  }

  size_t count(Tk key) {
    auto it = container_.find(key);
    if (it != container_.end()) {
      BucketT &bucket = it->second;
      return bucket.size();
    }
    return 0;
  }

  bool empty() const {
    return container_.empty();
  }

  void print() const {
    for (auto &[key, value] : container_) {
      std::cout << key << " ";
      value.printInternals();
    }
  }

  // private:
  MapT container_;
  int N_ = 1;
};

template <typename Tk, typename Tv>
using OrderedMultimap = Multimap<Tk, Tv, /*Order=*/true>;

template <typename Tk, typename Tv>
using UnorderedMultimap = Multimap<Tk, Tv, /*Order=*/false>;

} // namespace skynet
