#pragma once

#include <algorithm>
#include <cassert>
#include <functional>
#include <iostream>
#include <string>
#include <omp.h>
#include <vector>

#include "arcana/noelle/core/Pragma.h"
#include "Interface.hpp"

namespace skynet {

template <typename T>
class HSequence;

template <typename T>
__attribute__((always_inline)) HSequence<T> *clause_split(int N, HSequence<T> *hs) {
  auto p = noelle_pragma_begin("ldtc", &hs, clause_split<T>, hs);
  if (hs->leaf) {
    hs->bloom(N);
  } else {
    assert(hs->level.size() != 0);
    int offset = hs->level.size() - tc_chain_length();
    int k = tc_chain_id();
    hs = hs->level[offset + k];
    hs->bloom(N);
  }
  noelle_pragma_end(p);
  return hs;
}

template <typename T>
class HSequence {
public:
  HSequence(bool leaf) : leaf(leaf) {
    if (!leaf) {
      this->level.push_back(new HSequence(true));
    }
  }

  HSequence() : HSequence(false) {}

  ~HSequence() {
    for (auto hs : this->level) {
      hs->~HSequence();
    }
  }

  void split(int nsplits) {
    clause_split(nsplits, this);
  }

  HSequence &operator[](size_t idx) {
    assert(!leaf);
    return *level[idx];
  }

  std::vector<T> &elements() {
    assert(this->leaf);
    return this->data;
  }

  template <typename F>
  bool visitLeaves(F f, int l = 0) {
    if (this->leaf) {
      if (f(l, this->data)) {
        return true;
      }
    }
    for (auto hs : level) {
      if (hs->visitLeaves(f, l + 1)) {
        return true;
      }
    }
    return false;
  }

  bool forEach(std::function<bool(T *)> f) {
    if (this->leaf) {
      for (auto &x : this->data) {
        if (f(&x)) {
          return true;
        }
      }
    }
    for (auto hs : level) {
      if (hs->forEach(f)) {
        return true;
      }
    }
    return false;
  }

  __attribute__((always_inline)) void append(T value) {
    auto hs = this;
    auto p = noelle_pragma_begin("ldtc", &hs, clause_split<T>, hs);
    if (hs->leaf) {
      hs->data.push_back(value);
    } else {
      int offset = hs->level.size() - tc_chain_length();
      int k = tc_chain_id();
      // this->level[offset + k]->append(value);
      hs->level[offset + k]->data.push_back(value);
    }
    noelle_pragma_end(p);
  }

  void bloom(int N) {
    if (this->leaf) {
      assert(this->level.size() == 0);
      this->leaf = false;
      for (int i = 0; i < N; i++) {
        this->level.push_back(new HSequence<T>(true));
      }
      this->level[0]->data = std::move(this->data);
    } else {
      assert(this->level.size() != 0);
      for (int i = 0; i < N - 1; i++) {
        this->level.push_back(new HSequence<T>(true));
      }
    }
  }

  void print() {
    forEach([](auto *value) {
      std::cout << *value << " ";
      return false;
    });
    std::cout << "\n";
  }

  size_t size() {
    size_t counter = 0;
    forEach([&](auto) {
      counter++;
      return false;
    });
    return counter;
  }

  T &at(size_t idx) {
    size_t position = 0;
    T *element = nullptr;
    forEach([&](auto value) -> bool {
      if (position == idx) {
        element = value;
        return true;
      }
      position++;
      return false;
    });
    assert(element != nullptr);
    return *element;
  }

  template <typename F>
  bool visitPreorder(F f, int l = 0) {
    if (f(l, this)) {
      return true;
    }
    for (auto hs : this->level) {
      if (hs->visitPreorder(f, l + 1)) {
        return true;
      }
    }
    return false;
  }

  template <typename F>
  bool visitPostorder(F f, int l = 0) {
    for (auto hs : this->level) {
      if (hs->visitPreorder(f, l + 1)) {
        return true;
      }
    }
    if (f(l, this)) {
      return true;
    }
    return false;
  }

  void printInternals(std::string separator = " ") {
    printInternals(separator, 0);
  }

  void printInternals(std::string separator, int l) {
    if (l != 0) {
      std::cout << std::string(2 * (l - 1), ' ') << "| ";
    }
    if (this->leaf) {
      for (auto value : this->data) {
        std::cout << value << separator;
      }
      std::cout << " (" << this->data.size() << ")\n";
    } else {
      std::cout << "+\n";
    }
    for (auto hs : this->level) {
      hs->printInternals(separator, l + 1);
    }
  }

  void clear() {
    visitLeaves([](auto, auto &data) {
      data.clear();
      return false;
    });
  }

  bool empty() {
    if (this->leaf) {
      return this->data.size() == 0;
    }
    if (this->level.size() == 0) {
      return true;
    }
    for (auto hs : this->level) {
      if (!hs->empty()) {
        return false;
      }
    }
    return true;
  }

  bool shrink() {
    if (this->leaf) {
      return this->data.size() == 0;
    }

    std::vector<HSequence *> alive;
    for (auto hs : this->level) {
      if (!hs->shrink()) {
        alive.push_back(hs);
      }
    }

    if (alive.size() == 0) {
      this->level.clear();
      this->data.clear();
      this->leaf = true;
      return true;
    } else {
      this->level = std::move(alive);
      return false;
    }
  }

  // private:

  HSequence *findNthLeaf(int idx) {
    int position = 0;
    HSequence *leaf = nullptr;
    visitPreorder([&](auto, auto hs) {
      if (hs->leaf) {
        if (position == idx) {
          leaf = hs;
          return true;
        }
        position++;
      }
      return false;
    });

    return leaf;
  }

  bool leaf;
  HSequence *parent = nullptr;
  std::vector<HSequence *> level;
  std::vector<T> data;
};

} // namespace skynet
