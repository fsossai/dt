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

template <class Tk, class Tv>
class Multimap;

template <class Tk, class Tv>
void clause_multiset_insert(int N, Multimap<Tk, Tv> *mmap) {
  // TODO
  mmap->N_ = N;

  for (auto &[key, bucket] : mmap->container_) {
    clause_sequence_append(N, &bucket);
  }
}

template <class Tk, class Tv>
class Multimap {
public:
  using BucketT = BaseSequence<Tv, /*Order*/ false>;

  friend void clause_multiset_insert<Tk, Tv>(int N, Multimap<Tk, Tv> *mmap);

  // TODO
  Multimap() = default;

  ~Multimap() {}

  void insert(const Tk &key, const Tv &value) {
    int k = N_ - 1;
    auto p = noelle_pragma_begin("ldtc",
                                 &k,
                                 0,
                                 clause_multiset_insert<Tk, Tv>,
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

  bool hasKey(Tk key) const {
    return container_.contains(key);
  }

  BucketT &operator[](const Tk &key) {
    return container_[key];
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
  oneapi::tbb::concurrent_unordered_map<Tk, BucketT> container_;
  int N_ = 1;
};

} // namespace skynet
