#pragma once

#include <iostream>
#include <vector>

#include "arcana/noelle/core/Pragma.h"

namespace skynet {

template <typename T>
class Array;

template <typename T>
int clause_array_add(Array<T> *array) {
  array->k_++;
  array->container_.emplace_back();
  array->container_[array->k_].resize(array->size_);
  return array->k_;
}

template <class T>
class Array {
public:
  template <typename U>
  using ContainerType = std::vector<U>;

  friend int clause_array_add<T>(Array<T> *base);

  class Iterator {
  public:
    Iterator(Array<T> *base, int idx) : idx_(idx), base_(base) {}

    T operator*() {
      return base_[idx_];
    }

    Iterator &operator++() {
      idx_++;
      return *this;
    }

    bool operator!=(const Iterator &other) const {
      return idx_ != other.idx_;
    }

  private:
    int idx_;
    Array<T> *base_;
  };

  Array(size_t size) : size_(size) {
    container_.emplace_back();
    container_[0].resize(size);
    k_ = 0;
  }

  void set(size_t idx, T value) {
    container_[k_][idx] = value;
  }

  void add(size_t idx, T value) {
    int k = container_.size() - 1;
    auto _p = noelle_pragma_begin("ldtc", &k, 0, clause_array_add<T>, this);
    container_[k][idx] += value;
    noelle_pragma_end(_p);
  }

  T operator[](size_t idx) {
    T v = container_[0][idx];
    for (int i = 1; i <= this->k_; i++) {
      v += container_[i][idx];
    }
    return v;
  }

  Iterator begin() {
    return Iterator(this, 0);
  }

  Iterator end() {
    return Iterator(this, size());
  }

  bool empty() const {
    return size() == 0;
  }

  size_t size() const {
    return size_;
  }

  void printInternals() const {
    for (auto c : container_) {
      std::cout << "> ";
      for (auto x : c) {
        std::cout << x << " ";
      }
      std::cout << "\n";
    }
  }

private:
  std::vector<std::vector<T>> container_;
  int k_;
  size_t size_;
};

} // namespace skynet
