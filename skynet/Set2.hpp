#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <omp.h>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

#include "arcana/noelle/core/Pragma.h"

#include "Scalar.hpp"
#include "Set.hpp"

namespace skynet {

template <class T, SetCellContainerT C = SetT>
class Set2;

template <class T, SetCellContainerT C = SetT>
class Set2Iterator;

static inline int clause_empty() {
  return 0;
}

template <class T, SetCellContainerT C = SetT>
int clause_set2_insert(Set2<T, C> *set) {
#ifdef DEBUG
  std::printf("%s(%p)\n", __func__, set);
#endif
  set->n_rows_++;
  set->n_cols_++;
  set->container_.resize(set->n_rows_);
  for (auto &row : set->container_) {
    row.resize(set->n_cols_);
  }
  // TODO handle previously_inserted elements
  assert(false);
  return set->n_rows_ - 1;
}

template <class T, SetCellContainerT C = SetT>
void clause_set2_insert_bulk(int N, Set2<T, C> *set) {
#ifdef DEBUG
  std::printf("%s(%i, %p)\n", __func__, N, set);
#endif
  if (set->n_rows_ == N) {
    return;
  }
  assert(set->n_rows_ == 1);
  set->n_rows_ = N;
  set->n_cols_ = N;
  set->container_.resize(N);
  for (auto &row : set->container_) {
    row.resize(N);
  }
  auto previously_inserted = set->container_[0][0];
  set->container_[0][0].clear();
  for (auto e : previously_inserted) {
    set->insert(e);
  }
}

template <class T, SetCellContainerT C = SetT>
void clause_set2_op_plusplus_bulk(int N, Set2Iterator<T, C> *mit) {
#ifdef DEBUG
  std::printf("%s(%i, %p)\n", __func__, N, mit);
#endif
  // if (mit->limits_.size() == N) {
  //   return;
  // }
  // // The following assertion is there simply because I havne't thought about
  // // this scenario
  // assert(mit->limits_.size() == 1);
  //
  // mit->limits_.clear();
  // auto set = mit->base_;
  //
  // for (int i = 0; i < N; i++) {
  //   mit->limits_.emplace_back(std::make_pair(Set2SubIterator(set, i, i + 1),
  //                                            Set2SubIterator(set, i, i +
  //                                            1)));
  // }

  // TODO

  return;
}

template <class T, SetCellContainerT C>
class Set2 {
  friend int clause_set2_insert<T, C>(Set2<T, C> *set);
  friend void clause_set2_insert_bulk<T, C>(int N, Set2<T, C> *set);
  friend class Set2Iterator<T, C>;

public:
  using cell_container_t = typename std::
      conditional<C == SetT, std::unordered_set<T>, std::vector<T>>::type;
  using container_t = std::vector<cell_container_t>;

  Set2() : n_rows_(1), n_cols_(1), storage_size_(0) {
    container_.resize(n_rows_ * n_cols_);
    for (auto &row : container_) {
      row.resize(n_cols_);
    }
  }

  __attribute__((always_inline)) void insert(T value) {
    int i = hasher(value) % n_rows_;
    int j = 0;
    // j = rand() % n_cols_;

    if constexpr (C == SetT) {
      auto _p =
          noelle_pragma_begin("ldtc", &j, 0, clause_set2_insert<T, C>, this);
      auto pair = container_[i][j].insert(value);

      if (pair.second) { // insertion took place
        storage_size_.sum(1);
      }
      noelle_pragma_end(_p);
    }
    if constexpr (C == VectorT) {
      auto _p =
          noelle_pragma_begin("ldtc", &j, 0, clause_set2_insert<T, C>, this);
      container_[i][j].push_back(value);
      noelle_pragma_end(_p);
      storage_size_.sum(1);
    }
  }

  template <SetCellContainerT C2>
  void insert(const Set2<T, C2> &other) {
    if (n_rows_ != other.n_rows_) {
      if (other.n_rows_ > n_rows_) {
        __reshape(other.n_rows_, 1);
      }
    }
    if (n_rows_ == other.n_rows_) {
#pragma omp parallel for
      for (int i = 0; i < n_rows_; i++) {
        const auto &other_row = other.container_[i];
        auto &this_cell = container_[i][0];
        for (const auto &other_cell : other_row) {
          for (const auto &x : other_cell) {
            this_cell.insert(x);
          }
        }
      }
    } else {
      for (const auto &other_row : other.container_) {
        for (const auto &other_cell : other_row) {
          for (const auto &x : other_cell) {
            int i = hasher(x) % n_rows_;
            container_[i][0].insert(x);
          }
        }
      }
    }
  }

  void __reshape(int N, int M) {
    assert(N >= n_rows_ && M >= n_cols_ && "Not implemented");
    assert(n_rows_ == 1 && n_cols_ == 1 && "Not implemented");
    auto cell_copy = container_[0][0];
    container_.resize(N);
    for (auto &row : container_) {
      row.resize(M);
    }
    n_rows_ = N;
    n_cols_ = M;
    for (auto &x : cell_copy) {
      int i = hasher(x) % n_rows_;
      container_[i][0].insert(x);
    }
  }

  bool contains(T value) {
    int i = hasher(value) % n_rows_;
    auto &row = container_[i];
    for (auto &cell : row) {
      if constexpr (C == SetT) {
        if (cell.find(value) != cell.end()) {
          return true;
        }
      }
      if constexpr (C == VectorT) {
        if (std::find(cell.begin(), cell.end(), value) != cell.end()) {
          return true;
        }
      }
    }
    return false;
  }

  void clear() {
#pragma omp parallel for
    for (auto &row : container_) {
      for (auto &cell : row) {
        cell.clear();
      }
    }
    storage_size_.set(0);
  }

  size_t storageSize() const {
    return storage_size_.get();
  }

  bool empty() const {
    return storage_size_.get() == 0;
  }

  Set2Iterator<T, C> begin() {
    return Set2Iterator(this);
  }

  Set2Iterator<T, C> end() {
    return Set2Iterator(this);
  }

  std::string toString() {
    std::stringstream ss;
    ss << "{ ";
    for (auto e : *this) {
      ss << e << " ";
    }
    ss << "}";
    return ss.str();
  }

  size_t size() {
    size_t rs = 0;
    for (const auto &e : *this) {
      rs++;
    }
    return rs;
  }

  double redundancyFactor() {
    return (double)storageSize() / size();
  }

  void printInternals() const {
    for (auto &row : container_) {
      for (auto &cell : row) {
        std::cout << "{ ";
        for (auto &x : cell) {
          std::cout << x << " ";
        }
        std::cout << "}\t";
      }
      std::cout << "\n";
    }
  }

  void printStats() {
    auto size_local = size();
    auto redundancy = (double)storageSize() / size_local;

    std::cout << "storageSize = " << storageSize() << "\n";
    std::cout << "size = " << size_local << "\n";
    std::cout << "redundancyFactor = " << redundancy << "\n";

    for (int i = 0; i < n_rows_; i++) {
      int count = 0;
      auto it = Set2SubIterator(this, i, i + 1);
      auto end = it;
      for (; it != end; ++it) {
        count++;
      }
      std::cout << "row." << i << ".size = " << count << " ";
      std::printf("(%.1f %%)\n", 100. * count / size_local);
    }
    for (int i = 0; i < n_rows_; i++) {
      size_t count = 0;
      for (int j = 0; j < n_cols_; j++) {
        count += container_[i][j].size();
      }
      std::cout << "row." << i << ".storageSize = " << count << " ";
      std::printf("(%.1f %%)\n", 100. * count / storageSize());
    }
  }

  void __insert(int j, T value) {
    int i = hasher(value) % n_rows_;

    if constexpr (C == SetT) {
      auto pair = container_[i][j].insert(value);

      if (pair.second) { // insertion took place
        storage_size_.__sum(j, 1);
      }
    }
    if constexpr (C == VectorT) {
      container_[i][j].push_back(value);
      storage_size_.__sum(j, 1);
    }
  }

  // private:
  std::vector<std::vector<cell_container_t>> container_;
  size_t n_rows_;
  size_t n_cols_;
  Scalar<size_t> storage_size_;
};

template <class T, SetCellContainerT C>
class Set2Iterator {
  friend void clause_set2_op_plusplus_bulk<T, C>(int N,
                                                 Set2Iterator<T, C> *range);

public:
  Set2Iterator(Set2<T, C> *base) : base_(base) {
    row_its_.push_back(base_->container_[0].begin());
    row_ends_.push_back(base_->container_[0].end());
    cell_its_.push_back(row_its_[0]->begin());
    cell_ends_.push_back(row_its_[0]->end());
    if (cell_its_[0] != cell_ends_[0]) {
      seens_.emplace_back();
      seens_[0].insert(*cell_its_[0]);
    }
  }

  Set2Iterator begin() {
    return *this;
  }

  Set2Iterator end() {
    return *this;
  }

  __attribute__((always_inline)) bool operator!=(
      const Set2Iterator & /*other*/) const {
    int k = 0;
    auto _p = noelle_pragma_begin("ldtc", &k, 0, clause_empty);
    bool result = (row_its_[k] != row_ends_[k]) || (cell_its_[k] != cell_ends_[k]);
    noelle_pragma_end(_p);
    return result;
  }

  __attribute__((always_inline)) T operator*() {
    int k = 0;
    auto _p = noelle_pragma_begin("ldtc", &k, 0, clause_empty);
    auto result = *(cell_its_[k]);
    noelle_pragma_end(_p);
    return result;
  }

  __attribute__((always_inline)) void operator++() {
    int k = 0;
    auto _p = noelle_pragma_begin("ldtc", &k, 0, clause_empty);
    ++(cell_its_[k]);
    while (true) {
      if (cell_its_[k] == cell_ends_[k]) {
        ++(row_its_[k]);
        if (row_its_[k] == row_ends_[k]) {
          break;
        } else {
          cell_its_[k] = row_its_[k]->begin();
          cell_ends_[k] = row_its_[k]->end();
        }
      } else {
        const auto &e = *(cell_its_[k]);
        if (seens_[k].find(e) == seens_[k].end()) {
          seens_[k].insert(e);
          break;
        } else {
          ++(cell_its_[k]);
        }
      }
    }
    noelle_pragma_end(_p);
  }

  T __op_star(int k) {
    return *(cell_its_[k]);
  }

  void __op_plusplus(int k) {
    ++(cell_its_[k]);
    while (true) {
      if (cell_its_[k] == cell_ends_[k]) {
        ++(row_its_[k]);
        if (row_its_[k] == row_ends_[k]) {
          break;
        } else {
          cell_its_[k] = row_its_[k]->begin();
          cell_ends_[k] = row_its_[k]->end();
        }
      } else {
        const auto &e = *(cell_its_[k]);
        if (seens_[k].find(e) == seens_[k].end()) {
          seens_[k].insert(e);
          break;
        } else {
          ++(cell_its_[k]);
        }
      }
    }
  }

  bool __op_neq(int k, const Set2Iterator & /*other*/) const {
    return !(row_its_[k] != row_ends_[k]) && !(cell_its_[k] != cell_ends_[k]);
  }

private:
  using row_it_t =
      typename std::vector<typename Set2<T, C>::cell_container_t>::iterator;
  using cell_it_t = typename Set2<T, C>::cell_container_t::iterator;

  Set2<T, C> *base_;
  std::vector<cell_it_t> cell_its_;
  std::vector<cell_it_t> cell_ends_;
  std::vector<row_it_t> row_its_;
  std::vector<row_it_t> row_ends_;
  std::vector<std::unordered_set<T>> seens_;
};

} // namespace skynet
