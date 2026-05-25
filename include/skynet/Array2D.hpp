#pragma once

#include <cstddef>
#include <utility>
#include <vector>
#include <omp.h>

#include "Common.hpp"
#include "arcana/noelle/core/Pragma.h"

namespace skynet {

template <typename T>
class Array2D;

template <typename T>
void clause_array2d_add(int N, Array2D<T> *array) {
  skynet_assert(N >= 1);
  const size_t old_N = array->container_.size();
  array->container_.resize(N);

  for (size_t t = old_N; t < static_cast<size_t>(N); ++t) {
    auto &lane = array->container_[t];
    lane.resize(array->elements());
    array->fill_lane(lane, array->default_row_);
  }
}

template <typename T>
class Array2D {
public:
  friend void clause_array2d_add<T>(int N, Array2D<T> *array);

  explicit Array2D(size_t rows, size_t cols)
    : Array2D(rows, cols, T{}) {}

  Array2D(size_t rows, size_t cols, T default_value)
    : Array2D(rows, std::vector<T>(cols, std::move(default_value))) {}

  Array2D(size_t rows, std::vector<T> default_row)
    : rows_(rows),
      cols_(default_row.size()),
      default_row_(std::move(default_row)) {
    container_.resize(1);
    container_[0].resize(elements());
    fill_lane(container_[0], default_row_);
  }

  INLINE void __add(size_t t, size_t row, size_t col, const T &value) {
    skynet_assert(t < lanes());
    skynet_assert(row < rows_);
    skynet_assert(col < cols_);
    container_[t][offset(row, col)] += value;
  }

  INLINE void __add(size_t t, size_t row, const std::vector<T> &value) {
    skynet_assert(t < lanes());
    skynet_assert(row < rows_);
    skynet_assert(value.size() == cols_);

    auto *dst = row_data(container_[t], row);
    for (size_t col = 0; col < cols_; ++col) {
      dst[col] += value[col];
    }
  }

  INLINE void add(size_t row, size_t col, const T &value) {
    int t = 0;
    auto p = noelle_pragma_begin("ldtc", &t, 0, clause_array2d_add<T>, this);
    __add(t, row, col, value);
    noelle_pragma_end(p);
  }

  INLINE void add(size_t row, const std::vector<T> &value) {
    int t = 0;
    auto p = noelle_pragma_begin("ldtc", &t, 0, clause_array2d_add<T>, this);
    __add(t, row, value);
    noelle_pragma_end(p);
  }

  INLINE T operator()(size_t row, size_t col) const {
    skynet_assert(row < rows_);
    skynet_assert(col < cols_);

    T v = default_row_[col];
    const size_t idx = offset(row, col);
    for (const auto &lane : container_) {
      v += lane[idx];
    }
    return v;
  }

  INLINE std::vector<T> operator[](size_t row) const {
    skynet_assert(row < rows_);

    std::vector<T> v = default_row_;
    const size_t begin = row * cols_;
    for (const auto &lane : container_) {
      for (size_t col = 0; col < cols_; ++col) {
        v[col] += lane[begin + col];
      }
    }
    return v;
  }

  void fill(const T &value) {
    for (auto &lane : container_) {
      std::fill(lane.begin(), lane.end(), value);
    }
  }

  void fill(const std::vector<T> &row_value) {
    skynet_assert(row_value.size() == cols_);
    for (auto &lane : container_) {
      fill_lane(lane, row_value);
    }
  }

  void reduce() {
    for (size_t idx = 0; idx < elements(); ++idx) {
      T v = default_row_[idx % cols_];
      for (const auto &lane : container_) {
        v += lane[idx];
      }
      container_[0][idx] = v;
    }
    container_.resize(1);
  }

  void reduce_par() {
#pragma omp parallel for
    for (size_t idx = 0; idx < elements(); ++idx) {
      T v = default_row_[idx % cols_];
      for (const auto &lane : container_) {
        v += lane[idx];
      }
      container_[0][idx] = v;
    }
    container_.resize(1);
  }

  void reduce_rd(bool parallel = true) {
    const int nthreads = omp_get_max_threads() ? parallel : 1;
    int P = container_.size();
    int N = elements();

    for (int step = 1; step < P; step <<= 1) {
#pragma omp parallel for num_threads(nthreads)
      for (int i = 0; i < P; i += 2 * step) {
        int j = i + step;
        auto &lhs = container_[i];
        auto &rhs = container_[j];
        if (j < P) {
          for (int k = 0; k < N; ++k) {
            lhs[k] += rhs[k];
          }
        }
      }
    }
    container_.resize(1);
  }

  size_t size() const {
    return rows_;
  }

  size_t rows() const {
    return rows_;
  }

  size_t cols() const {
    return cols_;
  }

  std::vector<T> &lane(size_t t) {
    skynet_assert(t < lanes());
    return container_[t];
  }

  const std::vector<T> &lane(size_t t) const {
    skynet_assert(t < lanes());
    return container_[t];
  }

private:
  static void fill_lane(std::vector<T> &lane, const std::vector<T> &row_value) {
    const size_t cols = row_value.size();
    if (cols == 0) {
      return;
    }

    const size_t rows = lane.size() / cols;
    for (size_t row = 0; row < rows; ++row) {
      std::copy(row_value.begin(),
                row_value.end(),
                lane.begin() + static_cast<std::ptrdiff_t>(row * cols));
    }
  }

  size_t lanes() const {
    return container_.size();
  }

  size_t elements() const {
    return rows_ * cols_;
  }

  size_t offset(size_t row, size_t col) const {
    return row * cols_ + col;
  }

  T *row_data(std::vector<T> &lane, size_t row) {
    return lane.data() + static_cast<std::ptrdiff_t>(row * cols_);
  }

  std::vector<std::vector<T>> container_;
  size_t rows_;
  size_t cols_;
  std::vector<T> default_row_;
};

} // namespace skynet
