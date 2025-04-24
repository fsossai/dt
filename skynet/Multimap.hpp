#pragma once

#include <atomic>
#include <iostream>
#include <map>
#include <type_traits>
#include <vector>
#include <unordered_map>

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
  if (mmap->N_ == N) {
    return;
  }
  mmap->N_ = N;
  for (auto &[key, bucket] : mmap->container_) {
    clause_sequence_append(N, &bucket);
  }
}

template <typename Tk, typename Tv, bool Order>
class Multimap {
public:
  using BucketT = BaseSequence<Tv, /*Order*/ false>;
  using MapT = typename std::conditional_t<Order,
                                           std::map<Tk, BucketT>,
                                           std::unordered_map<Tk, BucketT>>;
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
    int k = 0;
    auto p = noelle_pragma_begin("ldtc",
                                 &k,
                                 0,
                                 clause_multiset_insert<Tk, Tv, Order>,
                                 this);
    __insert(k, key, value);
    noelle_pragma_end(p);
  }

  void __insert(int t, Tk key, const Tv &value) {
    auto it = container_.find(key);
    if (it != container_.end()) {
      BucketT &bucket = it->second;
      bucket.__insert(t, value);
    } else {
      BucketT bucket;
      clause_sequence_append(N_, &bucket);
      bucket.__insert(t, value);
      {
        // critical section
        std::lock_guard<std::mutex> lock(mutex_);
        auto it_retry = container_.find(key);
        if (it_retry != container_.end()) {
          BucketT &bucket = it_retry->second;
          bucket.__insert(t, value);
        } else {
          container_.insert({ key, std::move(bucket) });
        }
      }
    }
  }

  INLINE size_t numKeys() const {
    return container_.size();
  }

  KeysView keys() {
    return { *this };
  }

  template <typename R = KeysView>
  typename std::enable_if_t<Order, R> INLINE sortedKeys() {
    return { *this };
  }

  template <typename R = Tk>
  typename std::enable_if_t<Order, R> INLINE minKey() {
    return container_.begin()->first;
  }

  template <typename R = Tk>
  typename std::enable_if_t<Order, R> maxKey() {
    auto it = container_.begin();
    auto end = container_.end();
    Tk current_max = it->first;
    for (; it != end; ++it) {
      current_max = std::max<Tk>(current_max, it->first);
    }
    return current_max;
  }

  void compact() {
    std::vector<Tk> toErase;
    toErase.reserve(numKeys());
    for (auto &[key, bucket] : container_) {
      if (bucket.size() == 0) {
        toErase.push_back(key);
      }
    }
    for (auto key : toErase) {
      container_.erase(key);
    }
  }

  INLINE void erase(Tk key) {
    container_.erase(key);
  }

  void printKeys() {
    std::cout << "{ ";
    for (auto k : keys()) {
      std::cout << k << " ";
    }
    std::cout << "}\n";
  }

  INLINE bool hasKey(Tk key) const {
    return container_.contains(key);
  }

  INLINE BucketT &operator[](Tk key) {
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

  INLINE bool empty() const {
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
  std::mutex mutex_;
  int N_ = 1;
};

template <typename Tk, typename Tv>
using OrderedMultimap = Multimap<Tk, Tv, /*Order=*/true>;

template <typename Tk, typename Tv>
using UnorderedMultimap = Multimap<Tk, Tv, /*Order=*/false>;

} // namespace skynet
