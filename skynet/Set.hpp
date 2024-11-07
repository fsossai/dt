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

#include "Hasher.hpp"

#ifdef PADDING
constexpr int PAD = 32;
#else
constexpr int PAD = 1;
#endif

namespace skynet {

enum SetCellContainerT { VectorT, SetT };

template <class T, SetCellContainerT C = SetT>
class Set;

template <class T, SetCellContainerT C = SetT>
class SetIterator;

inline void clause_empty(int /*N*/) {}

template <class T, SetCellContainerT C = SetT>
void clause_set_insert(int N, Set<T, C> *set) {
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
void clause_set_op_plusplus(int N, SetIterator<T, C> *it) {
// The following assertion is there simply because I havne't thought about
// this scenario
#ifdef PADDING
  assert(it->row_its_.size() == PAD);
#else
  assert(it->row_its_.size() == 1);
#endif
  it->reshape(N);
}

template <class T, SetCellContainerT C>
class Set {
  friend void clause_set_insert<T, C>(int N, Set<T, C> *set);
  friend class SetIterator<T, C>;

public:
  using cell_container_t = typename std::
      conditional<C == SetT, std::unordered_set<T>, std::vector<T>>::type;
  using container_t = std::vector<cell_container_t>;

  Set() : n_rows_(1), n_cols_(1) {
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
          noelle_pragma_begin("ldtc", &j, 0, clause_set_insert<T, C>, this);
      container_[i][j].insert(value);
      noelle_pragma_end(_p);
    }
    if constexpr (C == VectorT) {
      auto _p =
          noelle_pragma_begin("ldtc", &j, 0, clause_set_insert<T, C>, this);
      container_[i][j].push_back(value);
      noelle_pragma_end(_p);
    }
  }

  template <SetCellContainerT C2>
  void insert(const Set<T, C2> &other) {
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
  }

  size_t storageSize() const {
    size_t s = 0;
    for (auto &row : container_) {
      for (auto &cell : row) {
        s += cell.size();
      }
    }
    return s;
  }

  bool empty() const {
    for (auto &row : container_) {
      for (auto &cell : row) {
        if (!cell.empty()) {
          return false;
        }
      }
    }
    return true;
  }

  SetIterator<T, C> begin() {
    return SetIterator(this);
  }

  SetIterator<T, C> end() {
    return SetIterator(this);
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
      auto it = SetSubIterator(this, i, i + 1);
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
      container_[i][j].insert(value);
    }
    if constexpr (C == VectorT) {
      container_[i][j].push_back(value);
    }
  }

  // private:
  std::vector<std::vector<cell_container_t>> container_;
  size_t n_rows_;
  size_t n_cols_;
};

template <class T, SetCellContainerT C>
class SetIterator {
  friend void clause_set_op_plusplus<T, C>(int N, SetIterator<T, C> *it);

public:
  SetIterator(Set<T, C> *base) : base_(base) {
    reshape(1);
  }

  SetIterator begin() {
    return *this;
  }

  SetIterator end() {
    return *this;
  }

  void reshape(int N) {
#ifdef PADDING
    if (row_its_.size() == N * PAD) {
      return;
    }
#else
    if (row_its_.size() == N) {
      return;
    }
#endif
    row_its_.clear();
    row_ends_.clear();
    cell_its_.clear();
    cell_ends_.clear();
    seens_.clear();
    auto set = base_;

    for (int i = 0; i < N; i++) {
      // find first non-empty cell in the i-th row
      auto &row = set->container_[i];
      auto row_it = row.begin();
      auto row_end = row.end();
      for (; row_it != row_end; ++row_it) {
        if (row_it->size() > 0) {
          break;
        }
      }
      seens_.emplace_back();
      if (row_it == row_end) {
        // i-th row is empty
        cell_ends_.emplace_back();
        cell_its_.push_back(cell_ends_.back());
      } else {
        // there is at least one valid element in this row
        auto cell_it = row_it->begin();
        auto cell_end = row_it->end();
        ;
        if (cell_it != cell_end) {
          seens_.back().insert(*cell_it);
        }
        cell_its_.push_back(std::move(cell_it));
        cell_ends_.push_back(std::move(cell_end));
      }
      row_its_.push_back(std::move(row_it));
      row_ends_.push_back(std::move(row_end));

      for (int j = 0; j < PAD - 1; j++) {
        row_its_.emplace_back();
        row_ends_.emplace_back();
        cell_its_.emplace_back();
        cell_ends_.emplace_back();
        seens_.emplace_back();
      }
    }

    return;
  }

  __attribute__((always_inline)) bool operator!=(
      const SetIterator & /*other*/) const {
    int k = 0;
    auto _p = noelle_pragma_begin("ldtc", &k, 0, clause_empty);
    bool result =
        (row_its_[k] != row_ends_[k]) || (cell_its_[k] != cell_ends_[k]);
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
    k *= PAD;
    return *(cell_its_[k]);
  }

  void __op_plusplus(int k) {
    k *= PAD;
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

  bool __op_neq(int k, const SetIterator & /*other*/) const {
    k *= PAD;
    return (row_its_[k] != row_ends_[k]) || (cell_its_[k] != cell_ends_[k]);
  }

private:
  using row_it_t =
      typename std::vector<typename Set<T, C>::cell_container_t>::iterator;
  using cell_it_t = typename Set<T, C>::cell_container_t::iterator;

  Set<T, C> *base_;
  std::vector<cell_it_t> cell_its_;
  std::vector<cell_it_t> cell_ends_;
  std::vector<row_it_t> row_its_;
  std::vector<row_it_t> row_ends_;
  std::vector<std::unordered_set<T>> seens_;
};

} // namespace skynet
