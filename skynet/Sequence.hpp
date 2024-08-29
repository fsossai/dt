#pragma once

#include <cassert>
#include <iostream>
#include <vector>

#include "arcana/noelle/core/Pragmas.hpp"

namespace skynet {

template <typename T>
class Sequence;

template <typename T>
int clause_sequence_append(Sequence<T> *seq) {
  seq->k_++;
  seq->container_.emplace_back();
  return seq->k_;
}

template <class T>
class Sequence {
public:
  template <typename U>
  using ContainerType = std::vector<U>;

  friend int clause_sequence_append<T>(Sequence<T> *base);

  class Iterator {
  public:
    Iterator(Sequence<T> *base, int idx) : idx(idx), base(base) {}

    T operator*() {
      auto coord = base->getCoordinate(idx);
      return base->container_[coord.first][coord.second];
    }

    Iterator &operator++() {
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

  __attribute__((always_inline)) void append(T value) {
    int k = container_.size() - 1;
    noelle_pragma_begin("ldtc", &k, 0, clause_sequence_append<T>, this);
    container_[k].push_back(value);
    noelle_pragma_end("ldtc");
  }

  void append_as(int k, T value) {
    container_[k].push_back(value);
  }

  T &operator[](size_t idx) {
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

}
