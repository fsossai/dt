#pragma once

// #include <absl/container/btree_map.h>
#include <atomic>
#include <iostream>
#include <iterator>
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
class CCMultimap;

template <typename Tk, typename Tv>
void clause_ccmultimap_insert(int N, CCMultimap<Tk, Tv> *mmap) {}

template <typename Tk, typename Tv>
class CCMultimap {
public:
  using MapT = oneapi::tbb::concurrent_multimap<Tk, Tv>;
  using MapIteratorT = typename MapT::iterator;

  friend void clause_ccmultimap_insert<Tk, Tv>(int N, CCMultimap<Tk, Tv> *mmap);

  CCMultimap() {}

  ~CCMultimap() {}

  void insert(Tk key, Tv value) {
    int k = 0;
    auto p = noelle_pragma_begin("ldtc",
                                 &k,
                                 0,
                                 clause_ccmultimap_insert<Tk, Tv>,
                                 this);
    __insert(k, key, value);
    noelle_pragma_end(p);
  }

  void __insert(int /*t*/, Tk key, Tv value) {
    container_.emplace(key, value);
  }

  Tk minKey() {
    container_.begin()->first;
  }

  void erase(Tk key) {
    container_.unsafe_erase(key);
  }

  std::vector<Tv> operator[](Tk key) {
    std::vector<Tv> values;
    auto range = container_.equal_range(key);
    for (auto it = range.first; it != range.second; ++it) {
      values.push_back(it->second);
    }
    return values;
  }

  size_t count(Tk key) {
    auto range = container_.equal_range(key);
    return std::distance(range.first, range.second);
  }

  bool empty() {
    return container_.empty();
  }

  // private:
  MapT container_;
};

} // namespace skynet
