#pragma once

#include <atomic>
#include <iostream>
#include <omp.h>
#include <type_traits>
#include <vector>

#include "HSequence.hpp"
#include "Interface.hpp"
#include "Common.hpp"

#include "arcana/noelle/core/Pragma.h"

namespace skynet {

template <typename T>
class MArray;

template <typename T>
void clause_array_add(int N, MArray<T> *array) {
  int K = (N + array->M_ - 1) / array->M_;
  auto currentK = array->container_.size();
  if (currentK == K) {
#ifdef DEBUG
    printf("%s: no resize\n", __func__);
#endif
    return;
  }
#ifdef DEBUG
  printf("%s: resize\n", __func__);
#endif

  assert(currentK < K);
  constexpr auto ceil_div = [](size_t a, size_t b) { return (a + b - 1) / b; };

  array->container_.resize(K);
  auto L = array->size_;
  for (size_t i = currentK; i < K; i++) {
    auto &new_row = array->container_[i];
    constexpr auto padding_elements =
        ceil_div(MArray<T>::TAIL_PADDING, sizeof(T));
    new_row.reserve(L + padding_elements);
    new_row.resize(L, array->default_value_);
  }
}

template <typename T>
int clause_array_add_nested(int N, MArray<T> *array, int M, int offset = 0) {
  auto p = noelle_pragma_begin("ldtc",
                               &offset,
                               clause_array_add_nested<T>,
                               array,
                               N * M);
  if (N * M > array->container_.size()) {
    array->container_.resize(N * M);
    for (auto &row : array->container_) {
      row.resize(array->size_);
    }
  }
  int retVal = (offset + tc_chain_id()) * N;
  noelle_pragma_end(p);
  return retVal;
}

template <typename T>
std::vector<T> &operator+=(std::vector<T> &lhs, const std::vector<T> &rhs) {
  const auto N = lhs.size();
  for (size_t i = 0; i < N; ++i) {
    lhs[i] += rhs[i];
  }
  return lhs;
}

template <class T>
class MArray {
public:
  friend void clause_array_add<T>(int N, MArray<T> *base);
  friend int clause_array_add_nested<T>(int N,
                                        MArray<T> *base,
                                        int M,
                                        int offset);

  static constexpr size_t TAIL_PADDING = 1000; // bytes

  class Iterator {
  public:
    Iterator(MArray<T> *base, int idx) : idx_(idx), base_(base) {}

    T operator*() {
      return (*base_)[idx_];
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
    MArray<T> *base_;
  };

  MArray(size_t size, T default_value)
    : size_(size),
      M_(1),
      default_value_(default_value) {
    container_.emplace_back();
    container_[0].resize(size);
    fill(default_value);
  }

  MArray(size_t size) : MArray(size, T{}) {}

  void setSharing(int M) {
    assert(M >= 1);
    M_ = M;
  }

  void __set(int t, size_t idx, T value) {
    container_[t][idx] = value;
  }

  INLINE void set(size_t idx, T value) {
    int t = 0;
    auto p = noelle_pragma_begin("ldtc", &t, 0);
    __set(t, idx, value);
    noelle_pragma_end(p);
  }

  void fill(T value) {
#pragma omp parallel for
    for (auto &subc : container_) {
      for (auto &x : subc) {
        x = value;
      }
    }
  }

  void add_nested(size_t idx, T value, int offset = 0) {
    auto p = noelle_pragma_begin("ldtc",
                                 &offset,
                                 clause_array_add_nested<T>,
                                 this,
                                 1);
    container_[offset + tc_chain_id()][idx] += value;
    noelle_pragma_end(p);
  }

  INLINE void __add(int t, size_t idx, const T &value) {
    // #pragma omp atomic
    // container_[t / M_][idx] += value;
    container_[t][idx] += value;
  }

  INLINE void add(size_t idx, const T &value) {
    int t = 0;
    auto p = noelle_pragma_begin("ldtc", &t, 0, clause_array_add<T>, this);
    __add(t, idx, value);
    noelle_pragma_end(p);
  }

  // This function should be a method of `ApatheticArray` that, at the moment is
  // not implemented
  INLINE bool replace_if_negative(size_t idx, T new_val) {
    auto _p = noelle_pragma_begin("ldtc");
    auto *addr = &container_[0][idx];
    auto val = *addr;
    if (val < 0) {
      if (__sync_bool_compare_and_swap(addr, val, new_val)) {
        return true;
      }
    }
    noelle_pragma_end(_p);
    return false;
  }

  T operator[](size_t idx) const {
    T v = container_[0][idx];
    for (size_t i = 1; i < container_.size(); i++) {
      v += container_[i][idx];
    }
    return v;
  }

  void reduce_par() {
#pragma omp parallel for
    for (size_t i = 0; i < size_; i++) {
      container_[0][i] = operator[](i);
    }
    container_.resize(1);
  }

  void reduce_seq() {
    for (size_t i = 0; i < size_; i++) {
      container_[0][i] = operator[](i);
    }
    container_.resize(1);
  }

  // recursive doubling
  void reduce_rd(bool parallel = true) {
    const int nthreads = omp_get_max_threads() ? parallel : 1;
    int P = container_.size();

    if (P == 0)
      return;

    int N = container_[0].size();

    // Recursive doubling
    for (int step = 1; step < P; step <<= 1) {
#pragma omp parallel for num_threads(nthreads)
      for (int i = 0; i < P; i += 2 * step) {
        int j = i + step;
        auto &lhs = container_[i];
        auto &rhs = container_[j];
        if (j < P) {
          for (int k = 0; k < N; ++k)
            lhs[k] += rhs[k];
        }
      }
    }
    container_.resize(1);
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
    for (auto &c : container_) {
      std::cout << "> ";
      for (auto x : c) {
        std::cout << x << " ";
      }
      std::cout << "\n";
    }
  }

  void printShape() const {
    std::cout << container_.size() << " x " << container_[0].size() << "\n";
  }

  // private:
  std::vector<std::vector<T>> container_;
  size_t size_;
  int M_;
  T default_value_;
};

} // namespace skynet
