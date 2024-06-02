#ifndef __SKYNET__
#define __SKYNET__

#include <vector>
#include <iostream>
#include <cassert>

#include "dti.h"

namespace skynet {

// Foward declarations

template<typename T>
class Sequence;

template<typename T>
int next_k(Sequence<T> *seq) {
  seq->k_++;
  seq->container_.emplace_back();
  return seq->k_;
}

template<class T>
class Sequence {
public:
  template<typename U>
  using ContainerType = std::vector<U>;

  friend int next_k<T>(Sequence<T> *base);

  class Iterator {
  public:
    Iterator(Sequence<T> *base, int idx)
      : idx(idx), base(base) {
    }

    T operator*() {
      auto coord = base->getCoordinate(idx);
      return base->container_[coord.first][coord.second];
    }

    Iterator& operator++() {
      idx++;
      return *this;
    }

    bool operator!=(const Iterator &other) const {
      return idx != other.idx;
    }

  private:
    int idx;
    Sequence<T> *base;
  };

  Sequence() {
    container_.emplace_back();
    k_ = 0;
  }

  __attribute__((always_inline))
  void append(T value) {
    int k = container_.size() - 1;
    PRAGMA_LDTC_BEGIN(k, next_k<T>, this);
    container_[k].push_back(value);
    PRAGMA_LDTC_END();
  }

  void append_as(int k, T value) {
    container_[k].push_back(value);
  }

  T& operator[](size_t idx) {
    auto coord = getCoordinate(idx);
    return container_[coord.first][coord.second];
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
    size_t sum = 0;
    for (size_t i = 0; i < container_.size(); i++) {
      sum += container_[i].size();
    }
    return sum;
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

  std::pair<size_t, size_t> getCoordinate(size_t idx) const {
    size_t c1 = 0;
    size_t csum = container_[0].size();
    while (csum <= idx) {
      c1++;
      assert(c1 != container_.size());
      csum += container_[c1].size();
    }

    size_t c2 = 0;
    if (c1 == 0) {
      c2 = idx;
    } else {
      c2 = idx - (csum - container_[c1].size());
    }
    
    std::pair<size_t, size_t> coord;
    coord.first = c1;
    coord.second = c2;

    return coord;
  }

  int k_;
};

template<typename T>
class Array;

template<typename T>
int array_next_k(Array<T> *array) {
  array->k_++;
  array->container_.emplace_back();
  return array->k_;
}

template<class T>
class Array {
public:
  template<typename U>
  using ContainerType = std::vector<U>;

  friend int array_next_k<T>(Array<T> *base);

  class Iterator {
  public:
    Iterator(Array<T> *base, int idx)
      : idx_(idx), base_(base) {
    }

    T operator*() {
      return base_[idx_];
    }

    Iterator& operator++() {
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
    PRAGMA_LDTC_BEGIN(k, array_next_k<T>, this);
    container_[k][idx] += value;
    PRAGMA_LDTC_END();
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

}

#endif // __SKYNET__
