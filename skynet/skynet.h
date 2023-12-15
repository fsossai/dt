#ifndef __SKYNET__
#define __SKYNET__

#include <vector>
#include <iostream>
#include <cassert>

#include "dti.h"

namespace skynet {

template<class T>
class Sequence {
public:
  template<typename U>
  using ContainerType = std::vector<U>;

  class Iterator {
  public:
    Iterator(Sequence<T> *base, int idx)
      : idx(idx), base(base) {
    }

    T operator*() {
      return (*base)[idx];
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

  void append(T value) {
    int k = container_.size() - 1;
    PRAGMA_LDTC_BEGIN(value, Sequence<T>::next_k, this);
    container_[k].push_back(value);
    PRAGMA_LDTC_END();
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
      for (auto x : c) {
        std::cout << x << " ";
      }
      std::cout << "\n";
    }
  }


private:
  std::vector<std::vector<T>> container_;

  int next_k() {
    k_++;
    container_.emplace_back();
    return k_;
  }
 
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

//template<typename T>
//class Sequence {
//public:
//  template<typename U>
//  using ContainerType = std::vector<U>;
//
//  Sequence() = default;
//
//  void append(T value) {
//    int k = container_.size()-1;
//    PRAGMA_LDTC_BEGIN(value, Sequence<T>::next_k, this);
//    container_[k].push_back(value);
//    PRAGMA_LDTC_END();
//  }
//
//  size_t size() const {
//    return container_.size();
//  }
//
//  T& operator[](size_t idx) {
//    return container_[idx];
//  }
//  
//  const T& operator[](size_t idx) const {
//    return container_[idx];
//  }
//
//  typename ContainerType<T>::iterator begin() {
//    return container_.begin();
//  }
//
//  typename ContainerType<T>::iterator end() {
//    return container_.end();
//  }
//
//private:
//  ContainerType<T> container_;
//  int k_ = 0;
//};

}

#endif // __SKYNET__
