#pragma once

#include <iostream>
#include <vector>
#include <unordered_set>

namespace skynet {

template<class T>
class Set;

template<class T>
int set_clause_insert(Set<T>* set) {
  set->n_rows_++;
  set->n_cols_++;
  set->container_.resize(set->n_rows_ * set->n_cols_);
  return set->n_rows_ - 1;
}

template<class T>
class Set {
public:
  using set_container_t = std::unordered_set<T>;

  friend int set_clause_insert<T>(Set<T> *set);

  class Iterator {
  public:
    Iterator(Set<T> *base, typename set_container_t::iterator it)
      : base_(base) {
    }

    T operator*() {
      return *it_;
    }

    Iterator& operator++() {
      it_++;
      if (it_ == current_set_->end()) {
        if (col_ == base_->n_cols_ - 1) {
          // end of the row, go to the next
          row_++;
          col_ = 0;
        } else {
          col_++;
        }
        current_set_ = &base_->container_[row_ * base_->n_cols_];
        it_ = current_set_->begin();
      }
      current_set_++;
      return *this;
    }

    bool operator!=(const Iterator &other) const {
      return it_ != other.it_;
    }

  private:
    Set<T> *base_;
    size_t row_;
    size_t col_;
    set_container_t *current_set_;
    typename set_container_t::iterator it_;
  };

  Set() : n_rows_(1), n_cols_(1) {
    container_.resize(n_rows_ * n_cols_);
  }

  void insert(T value) {
    int i = value % n_rows_;
    int j = rand() % n_cols_;
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
    size_= 0;
  }

  size_t size() const {
    return size_;
  }

  bool empty() const {
    return size_ == 0;
  }

  Iterator begin() {
    return Iterator(this, container_[0].begin());
  }

  Iterator end() {
    return Iterator(this, container_[container_.size() - 1].end());
  }

  void printInternals() const {
    for (int i = 0; i < n_rows_; i++) {
      for (int j = 0; j < n_cols_; j++) {
        std::cout << "{ ";
        for (auto &x : container_[i*n_cols_ + j]) {
          std::cout << x << " ";
        }
        std::cout << "}\t";
      }
      std::cout << "\n";
    }
  }

private:
  std::vector<set_container_t> container_;
  size_t n_rows_;
  size_t n_cols_;
  size_t size_;
};

}
