#pragma once

#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
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
    Iterator(Set<T> *base, int row_begin, int row_end) : base_(base) {
      flat_idx_ = row_begin * base->n_cols_;
      flat_end_ = row_end * base->n_cols_;
      end_ = base->container_[flat_end_ - 1].end();
      reset_cell_iterators_();
      produce_next_iterator_();
    }

    T operator*() {
      return *it_;
    }

    Iterator &operator++() {
      it_++;
      produce_next_iterator_();
      return *this;
    }

    bool operator!=(const Iterator &other) const {
      return it_ != other.end_;
    }

  private:
    bool reached_last_cell_() const {
      return flat_idx_ == (flat_end_ - 1);
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

    bool this_is_column_zero_() const {
      return flat_idx_ % base_->n_cols_ == 0;
    }

    void reset_seen_set_() {
      anti_duplicates_.clear();
    }

    void reset_cell_iterators_() {
      it_ = base_->container_[flat_idx_].begin();
      cell_end_ = base_->container_[flat_idx_].end();
    }

    void mark_current_element_as_seen_() {
      anti_duplicates_.insert(*it_);
    }

    void move_to_next_cell_() {
      flat_idx_++;
      reset_cell_iterators_();
    }

    void produce_next_iterator_() {
      bool moved;
      do {
        moved = false;

        // find the next non-empty cell
        while (reached_cell_end_() && !reached_last_cell_()) {
          // std::printf("[nc %zu/%zu] ", flat_idx_, flat_end_);
          move_to_next_cell_();
          if (this_is_column_zero_()) {
            reset_seen_set_();
          }
        }

        // find the next unseen element
        if (!reached_cell_end_()) {
          if (already_seen_()) {
            it_++;
            moved = true;
          } else {
          }
        }
      } while (moved);

      if (!reached_last_cell_() && !reached_end_()) {
        mark_current_element_as_seen_();
      }
    }

    Set<T> *base_;
    size_t flat_idx_;
    size_t flat_end_;
    typename cell_container_t::iterator it_;
    typename cell_container_t::iterator cell_end_;
    typename cell_container_t::iterator end_;
    cell_container_t anti_duplicates_;
  };

  Set() : n_rows_(1), n_cols_(1), storage_size_(0) {
    container_.resize(n_rows_ * n_cols_);
  }

  void insert(T value) {
    int i = value % n_rows_;
    int j = rand() % n_cols_;
    // int j = 0;
    auto pair = container_[i * n_cols_ + j].insert(value);
    if (pair.second) { // insertion took place
      storage_size_++;
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
    storage_size_ = 0;
  }

  size_t storageSize() const {
    return storage_size_;
  }

  bool empty() const {
    return storage_size_ == 0;
  }

  Iterator begin() {
    return Iterator(this, 0, n_rows_);
  }

  Iterator end() {
    return Iterator(this, 0, n_rows_);
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

  void printStats() {
    auto size_local = size();
    auto redundancy = (double)storageSize() / size_local;

    std::cout << "storageSize = " << storageSize() << "\n";
    std::cout << "size = " << size_local << "\n";
    std::cout << "redundancyFactor = " << redundancy << "\n";

    for (int i = 0; i < n_rows_; i++) {
      int count = 0;
      auto it = Iterator(this, i, i + 1);
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
        count += container_[i * n_cols_ + j].size();
      }
      std::cout << "row." << i << ".storageSize = " << count << " ";
      std::printf("(%.1f %%)\n", 100. * count / storageSize());
    }
  }

private:
  std::vector<cell_container_t> container_;
  size_t n_rows_;
  size_t n_cols_;
  size_t storage_size_;
};

} // namespace skynet
