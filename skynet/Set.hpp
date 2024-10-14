#pragma once

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

#include "arcana/noelle/core/Pragma.h"

#include "Scalar.hpp"

namespace skynet {

template <class T>
class Set;

template <class T>
class SetSubIterator;

template <class T>
class SetIterator;

template <class T>
int clause_set_insert(Set<T> *set) {
  set->n_rows_++;
  set->n_cols_++;
  set->container_.resize(set->n_rows_);
  for (auto &row : set->container_) {
    row.resize(set->n_cols_);
  }
  return set->n_rows_ - 1;
}

template <class T>
int clause_set_op_plusplus(SetIterator<T> *mit) {
  const int N = mit->base_->n_rows_;
  const int M = mit->limits_.size() + 1;
  mit->limits_.clear();
  auto set = mit->base_;

  for (int i = 0; i < M; i++) {
    int row_begin = i * N / M;
    int row_end = (i + 1) * N / M;
    // std::printf("clause_set_op_plusplus: (%i, %i)\n", i, row_begin, row_end);
    mit->limits_.emplace_back(
        std::make_pair(SetSubIterator(set, row_begin, row_end),
                       SetSubIterator(set, row_begin, row_end)));
  }
  // std::cout << "\n";

  return 0;
}

template <class T>
int clause_set_op_neq(SetIterator<T> *mit) {
  return mit->limits_.size() - 1;
}

template <class T>
int clause_set_op_star(SetIterator<T> *mit) {
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

template <class T>
class Set {
  friend int clause_set_insert<T>(Set<T> *set);
  friend class SetSubIterator<T>;
  friend class SetIterator<T>;

public:
  using cell_container_t = std::unordered_set<T>;
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

    auto _p = noelle_pragma_begin("ldtc", &j, 0, clause_set_insert<T>, this);
    auto pair = container_[i][j].insert(value);
    noelle_pragma_end(_p);

    if (pair.second) { // insertion took place
      storage_size_.sum(1);
    }
  }

  bool contains(T value) {
    int i = hasher(value) % n_rows_;
    for (int j = 0; j < n_cols_; j++) {
      auto &s = container_[i][j];
      if (s.find(value) != s.end()) {
        return true;
      }
    }
    return false;
  }

  void clear() {
    for (auto &row : container_) {
      for (auto &cell : row) {
        cell.clear();
      }
    }
    storage_size_ = 0;
  }

  size_t storageSize() const {
    return storage_size_.get();
  }

  bool empty() const {
    return storage_size_.get() == 0;
  }

  SetIterator<T> begin() {
    return SetIterator(this);
  }

  SetIterator<T> end() {
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

private:
  std::vector<std::vector<cell_container_t>> container_;
  size_t n_rows_;
  size_t n_cols_;
  Scalar<size_t> storage_size_;
};

template <class T>
class SetSubIterator {
public:
  SetSubIterator(Set<T> *base, int row_begin, int row_end)
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

  __attribute__((always_inline)) SetSubIterator &operator++() {
    it_++;
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

  void produce_next_iterator_() {
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

  Set<T> *base_;
  size_t row_idx_;
  size_t col_idx_;
  size_t n_rows_;
  size_t n_cols_;
  std::vector<typename Set<T>::cell_container_t> *row_;
  typename Set<T>::cell_container_t::iterator it_;
  typename Set<T>::cell_container_t::iterator cell_end_;
  typename Set<T>::cell_container_t::iterator end_;
  typename Set<T>::cell_container_t anti_duplicates_;
  int row_begin_;
  int row_end_;
};

template <class T>
class SetIterator {
  friend int clause_set_op_plusplus<T>(SetIterator<T> *range);
  friend int clause_set_op_neq<T>(SetIterator<T> *range);
  friend int clause_set_op_star<T>(SetIterator<T> *range);

public:
  SetIterator(Set<T> *base) : base_(base) {
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
    auto _p =
        noelle_pragma_begin("ldtc", &k, 0, clause_set_op_neq<T>, this);
    auto result = limits_[k].first != limits_[k].second;
    noelle_pragma_end(_p);
    return result;
  }

  T operator*() {
    int k = 0;
    auto _p =
        noelle_pragma_begin("ldtc", &k, 0, clause_set_op_star<T>, this);
    auto result = *limits_[k].first;
    noelle_pragma_end(_p);
    return result;
  }

  __attribute__((always_inline)) void operator++() {
    int k = 0;
    auto _p =
        noelle_pragma_begin("ldtc", &k, 0, clause_set_op_plusplus<T>, this);
    ++limits_[k].first;
    noelle_pragma_end(_p);
  }

private:
  Set<T> *base_;
  std::vector<std::pair<SetSubIterator<T>, SetSubIterator<T>>> limits_;
};

} // namespace skynet
