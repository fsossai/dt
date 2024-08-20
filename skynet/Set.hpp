#pragma once

#include <iostream>
#include <vector>
#include <unordered_set>

namespace skynet {

template <class T>
class Set;

template <class T>
int set_clause_insert(Set<T> *set) {
  set->n_rows_++;
  set->n_cols_++;
  set->container_.resize(set->n_rows_ * set->n_cols_);
  return set->n_rows_ - 1;
}

template <class T>
class Set {
public:
  using cell_container_t = std::unordered_set<T>;
  using container_t = std::vector<cell_container_t>;

  friend int set_clause_insert<T>(Set<T> *set);

  class Iterator {
  public:
    Iterator(Set<T> *base, int row_begin, int row_end)
      : base_(base) {
      flat_idx_ = row_begin * base->n_cols_;
      flat_end_ = row_end * base->n_cols_;
      it_ = base->container_[flat_idx_].begin();
      cell_end_ = base_->container_[flat_idx_].end();
      end_ = base->container_[flat_end_ - 1].end();
      skip_empty_();
    }

    T operator*() {
      return *it_;
    }

    Iterator &operator++() {
      it_++;
      skip_empty_();
      return *this;
    }

    bool operator!=(const Iterator &other) const {
      return it_ != other.end_;
    }

  private:
    void skip_empty_() {
      while (it_ == cell_end_ && flat_idx_ != flat_end_) {
        flat_idx_++;
        it_ = base_->container_[flat_idx_].begin();
        cell_end_ = base_->container_[flat_idx_].end();
      }
    }

    Set<T> *base_;
    size_t flat_idx_;
    size_t flat_end_;
    typename cell_container_t::iterator it_;
    typename cell_container_t::iterator cell_end_;
    typename cell_container_t::iterator end_;
  };

  Set() : n_rows_(1), n_cols_(1) {
    container_.resize(n_rows_ * n_cols_);
  }

  void insert(T value) {
    int i = value % n_rows_;
    int j = rand() % n_cols_;
    // int j = 0;
    auto pair = container_[i * n_cols_ + j].insert(value);
    if (pair.second) { // insertion took place
      size_++;
    }
  }

  bool contains(T value) {
    int i = value % n_rows_;
    for (int j = 0; j < n_cols_; j++) {
      auto &s = container_[i * n_cols_ + j];
      if (s.find(value) != s.end()) {
        return true;
      }
    }
    return false;
  }

  void clear() {
    for (auto &s : container_) {
      s.clear();
    }
    size_ = 0;
  }

  size_t size() const {
    return size_;
  }

  bool empty() const {
    return size_ == 0;
  }

  Iterator begin() {
    return Iterator(this, 0, n_rows_);
  }

  Iterator end() {
    return Iterator(this, 0, n_rows_);
  }

  void printInternals() const {
    for (int i = 0; i < n_rows_; i++) {
      for (int j = 0; j < n_cols_; j++) {
        std::cout << "{ ";
        for (auto &x : container_[i * n_cols_ + j]) {
          std::cout << x << " ";
        }
        std::cout << "}\t";
      }
      std::cout << "\n";
    }
  }

private:
  std::vector<cell_container_t> container_;
  size_t n_rows_;
  size_t n_cols_;
  size_t size_;
};

} // namespace skynet
