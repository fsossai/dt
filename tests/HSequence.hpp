#pragma once

#include <algorithm>
#include <cassert>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace skynet {

template <typename T>
class HSequence {
public:
  HSequence(bool leaf = true) : leaf(leaf) {}

  ~HSequence() {
    for (auto hs : this->level) {
      hs->~HSequence();
    }
  }

  void __split(int t, int nsplits) {
    if (nsplits == 1) {
      return;
    }
    auto hs = this->findNthLeaf(t);
    assert(hs != nullptr);
    assert(hs->leaf);
    hs->leaf = false;
    hs->level.clear();
    for (int i = 0; i < nsplits; i++) {
      hs->level.push_back(new HSequence(true));
    }
    hs->level[0]->data = std::move(hs->data);
  }

  void split(int nsplits) {
    __split(0, nsplits);
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

  void append(T value) {
    if (this->leaf) {
      this->data.push_back(value);
    } else {
      this->level[this->level.size() - 1]->append(value);
    }
  }

  void __append(int t, T value) {
    auto holder = findNthLeaf(t);
    holder->append(value);
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
    std::cout << "{\n";
    visitPreorder([&](int l, auto *hs) {
      if (hs->leaf) {
        std::cout << "|" << std::string(l, '-') << " ";
        for (auto value : hs->data) {
          std::cout << value << separator;
        }
        std::cout << " (" << hs->data.size() << ")\n";
      }
      return false;
    });
    std::cout << "}\n";
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
