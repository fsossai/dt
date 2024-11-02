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

namespace skynet {

enum SetCellContainerT { VectorT, SetT };

template <class T, SetCellContainerT C = SetT>
class Set;

template <class T, SetCellContainerT C = SetT>
class SetSubIterator;

template <class T, SetCellContainerT C = SetT>
class SetIterator;

template <class T, SetCellContainerT C = SetT>
int clause_set_insert(Set<T, C> *set) {
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
void clause_set_insert_bulk(int N, Set<T, C> *set) {
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
  return;
}

template <class T, SetCellContainerT C = SetT>
void clause_set_insert_all_bulk(int N, Set<T, C> *set) {
#ifdef DEBUG
  std::printf("%s(%i, %p)\n", __func__, N, set);
#endif
  if (set->n_cols_ == N) {
    return;
  }
  assert(set->n_cols_ == 1);
  assert(set->n_rows_ == 1);
  set->n_rows_ = 1;
  set->n_cols_ = N;
  set->container_.resize(1);
  for (auto &row : set->container_) {
    row.resize(N);
  }
  return;
}

template <class T, SetCellContainerT C = SetT>
int clause_set_op_plusplus(SetIterator<T, C> *mit) {
#ifdef DEBUG
  std::printf("%s(%p)\n", __func__, mit);
#endif
  const int N = mit->base_->n_rows_;
  const int M = mit->limits_.size() + 1;
  mit->limits_.clear();
  auto set = mit->base_;

  for (int i = 0; i < M; i++) {
    int row_begin = i * N / M;
    int row_end = (i + 1) * N / M;
    mit->limits_.emplace_back(
        std::make_pair(SetSubIterator(set, row_begin, row_end),
                       SetSubIterator(set, row_begin, row_end)));
  }

  return M - 1;
}

template <class T, SetCellContainerT C = SetT>
void clause_set_op_plusplus_bulk(int N, SetIterator<T, C> *mit) {
#ifdef DEBUG
  std::printf("%s(%i, %p)\n", __func__, N, mit);
#endif
  if (mit->limits_.size() == N) {
    return;
  }
  // The following assertion is there simply because I havne't thought about
  // this scenario
  assert(mit->limits_.size() == 1);

  mit->limits_.clear();
  auto set = mit->base_;

  for (int i = 0; i < N; i++) {
    mit->limits_.emplace_back(std::make_pair(SetSubIterator(set, i, i + 1),
                                             SetSubIterator(set, i, i + 1)));
  }

  return;
}

template <class T, SetCellContainerT C = SetT>
int clause_set_op_neq(SetIterator<T, C> *mit) {
#ifdef DEBUG
  std::printf("%s(%p)\n", __func__, mit);
#endif
  return mit->limits_.size() - 1;
}

template <class T, SetCellContainerT C = SetT>
int clause_set_op_star(SetIterator<T, C> *mit) {
#ifdef DEBUG
  std::printf("%s(%p)\n", __func__, mit);
#endif
  return mit->limits_.size() - 1;
}

template <typename T>
typename std::enable_if<std::is_pointer<T>::value, int>::type hasher(T val) {
  return (reinterpret_cast<uint64_t>(val) * 14695981039346656037ULL)
         >> (64 - 11);
};

template <typename T>
typename std::enable_if<std::is_arithmetic<T>::value, int>::type hasher(T val) {
  return val;
};

template <class T, SetCellContainerT C>
class Set {
  friend int clause_set_insert<T, C>(Set<T, C> *set);
  friend void clause_set_insert_bulk<T, C>(int N, Set<T, C> *set);
  friend class SetSubIterator<T, C>;
  friend class SetIterator<T, C>;

public:
  using cell_container_t = typename std::
      conditional<C == SetT, std::unordered_set<T>, std::vector<T>>::type;
  using container_t = std::vector<cell_container_t>;

  Set() : n_rows_(1), n_cols_(1), storage_size_(0) {
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
      auto pair = container_[i][j].insert(value);

      if (pair.second) { // insertion took place
        storage_size_.sum(1);
      }
      noelle_pragma_end(_p);
    }
    if constexpr (C == VectorT) {
      auto _p =
          noelle_pragma_begin("ldtc", &j, 0, clause_set_insert<T, C>, this);
      container_[i][j].push_back(value);
      noelle_pragma_end(_p);
      storage_size_.sum(1);
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
    storage_size_.set(0);
  }

  size_t storageSize() const {
    return storage_size_.get();
  }

  bool empty() const {
    return storage_size_.get() == 0;
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
class SetSubIterator {
public:
  SetSubIterator(Set<T, C> *base, int row_begin, int row_end)
    : base_(base),
      row_idx_(row_begin),
      col_idx_(0),
      n_rows_(base->n_rows_),
      n_cols_(base->n_cols_),
      row_begin_(0),
      row_end_(row_end) {
    row_ = &base->container_[0];
    end_ = base->container_[row_end - 1][n_cols_ - 1].end();
    reset_cell_iterators_();
    produce_next_iterator_();
  }

  T operator*() {
    return *it_;
  }

  SetSubIterator &operator++() {
    ++it_;
    produce_next_iterator_();
    return *this;
  }

  bool operator!=(const SetSubIterator &other) const {
    return it_ != other.end_;
  }

private:
  bool reached_last_cell_() const {
    return (row_idx_ == row_end_ - 1) && (col_idx_ == n_cols_ - 1);
  }

  bool reached_cell_end_() const {
    return it_ == cell_end_;
  }

  bool reached_end_() const {
    return it_ == end_;
  }

  bool already_seen_() const {
    return anti_duplicates_.find(*it_) != anti_duplicates_.end();
  }

  void reset_seen_set_() {
    anti_duplicates_.clear();
  }

  void reset_cell_iterators_() {
    row_ = &base_->container_[row_idx_];
    auto &cell = (*row_).at(col_idx_);
    it_ = cell.begin();
    cell_end_ = cell.end();
  }

  void mark_current_element_as_seen_() {
    anti_duplicates_.insert(*it_);
  }

  void move_to_next_cell_() {
    if (col_idx_ == n_cols_ - 1) {
      col_idx_ = 0;
      row_idx_++;
    } else {
      col_idx_++;
    }
    reset_cell_iterators_();
  }

  __attribute__((always_inline)) void produce_next_iterator_() {
    bool moved;
    do {
      moved = false;

      // find the next non-empty cell
      while (reached_cell_end_() && !reached_last_cell_()) {
        move_to_next_cell_();
        if (col_idx_ == 0) {
          reset_seen_set_();
        }
      }

      // find the next unseen element
      if (!reached_cell_end_()) {
        if (already_seen_()) {
          it_++;
          moved = true;
        }
      }
    } while (moved);

    if (!reached_last_cell_() && !reached_end_()) {
      mark_current_element_as_seen_();
    }
  }

  Set<T, C> *base_;
  size_t row_idx_;
  size_t col_idx_;
  size_t n_rows_;
  size_t n_cols_;
  std::vector<typename Set<T, C>::cell_container_t> *row_;
  typename Set<T, C>::cell_container_t::iterator it_;
  typename Set<T, C>::cell_container_t::iterator cell_end_;
  typename Set<T, C>::cell_container_t::iterator end_;
  std::unordered_set<T> anti_duplicates_;
  int row_begin_;
  int row_end_;
};

template <class T, SetCellContainerT C>
class SetIterator {
  friend int clause_set_op_plusplus<T, C>(SetIterator<T, C> *range);
  friend void clause_set_op_plusplus_bulk<T, C>(int N,
                                                SetIterator<T, C> *range);
  friend int clause_set_op_neq<T, C>(SetIterator<T, C> *range);
  friend int clause_set_op_star<T, C>(SetIterator<T, C> *range);

public:
  SetIterator(Set<T, C> *base) : base_(base) {
    limits_.emplace_back(
        std::make_pair(SetSubIterator(base, 0, base->n_rows_),
                       SetSubIterator(base, 0, base->n_rows_)));
  }

  SetIterator begin() {
    return *this;
  }

  SetIterator end() {
    return *this;
  }

  __attribute__((always_inline)) bool operator!=(
      const SetIterator & /*other*/) const {
    int k = 0;
    auto _p = noelle_pragma_begin("ldtc", &k, 0, clause_set_op_neq<T, C>, this);
    auto result = limits_[k].first != limits_[k].second;
    noelle_pragma_end(_p);
    return result;
  }

  __attribute__((always_inline)) T operator*() {
    int k = 0;
    auto _p =
        noelle_pragma_begin("ldtc", &k, 0, clause_set_op_star<T, C>, this);
    auto result = *limits_[k].first;
    noelle_pragma_end(_p);
    return result;
  }

  __attribute__((always_inline)) void operator++() {
    int k = 0;
    auto _p =
        noelle_pragma_begin("ldtc", &k, 0, clause_set_op_plusplus<T, C>, this);
    ++limits_[k].first;
    noelle_pragma_end(_p);
  }

  T __op_star(int k) {
    return *limits_[k].first;
  }

  void __op_plusplus(int k) {
    ++limits_[k].first;
  }

  bool __op_neq(int k, const SetIterator & /*other*/) const {
    return limits_[k].first != limits_[k].second;
  }

private:
  Set<T, C> *base_;
  std::vector<std::pair<SetSubIterator<T, C>, SetSubIterator<T, C>>> limits_;
};

} // namespace skynet
