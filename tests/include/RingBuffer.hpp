#pragma once

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string.h>
#include <vector>
#include <deque>

template <class T>
class RingBuffer {
private:
  int next_power_of_2_(int x) {
    return 0x8000'0000 >> (__builtin_clz(x - 1) - 1);
  }

public:
  static constexpr int RESIZE_FACTOR = 2;

  RingBuffer(int capacity)
    : idx_begin_(0),
      idx_end_(0),
      capacity_(next_power_of_2_(capacity + 1)) {
    data_ = (T *)malloc(sizeof(T) * capacity_);
    mask_ = capacity_ - 1;
#ifdef STATS
    std::cout << "RingBuffer initial capacity = " << capacity_ << "\n";
#endif
  }

  RingBuffer() : RingBuffer(1) {}

  void push(T element) {
    if (full()) {
      size_t new_capacity = capacity_ * RESIZE_FACTOR;
      T *new_data_ = (T *)malloc(sizeof(T) * new_capacity);

      // smart copy
      if (idx_begin_ == 0) { // this implies idx_end_ == capacity_ - 1
        memcpy(new_data_, data_, sizeof(T) * idx_end_);
      } else {
        size_t tail_size = capacity_ - idx_begin_;
        memcpy(&new_data_[tail_size], data_, sizeof(T) * (idx_begin_ - 1));
        memcpy(new_data_, &data_[idx_begin_], sizeof(T) * tail_size);
      }
      free(data_);
      data_ = new_data_;
      idx_end_ = capacity_ - 1;
      idx_begin_ = 0;
      capacity_ = new_capacity;
      mask_ = capacity_ - 1;
    }
    data_[idx_end_] = element;
    idx_end_ = (idx_end_ + 1) & mask_;
  }

  bool empty() const {
    return idx_begin_ == idx_end_;
  }

  bool full() const {
    return ((idx_begin_ - 1) & mask_) == idx_end_;
  }

  const T &front() const {
#ifdef SAFE
    if (empty()) {
      fprintf(stderr, "ERROR: front() on empty RingBuffer\n");
      abort();
    }
#endif
    return data_[idx_begin_];
  }

  T &front() {
#ifdef SAFE
    if (empty()) {
      fprintf(stderr, "ERROR: front() on empty RingBuffer\n");
      abort();
    }
#endif
    return data_[idx_begin_];
  }

  void pop() {
#ifdef SAFE
    if (empty()) {
      fprintf(stderr, "ERROR: pop() on empty RingBuffer\n");
      abort();
    }
#endif
    idx_begin_ = (idx_begin_ + 1) & mask_;
  }

  size_t size() const {
    return (idx_end_ - idx_begin_ + capacity_) & mask_;
  }

  size_t capacity() const {
    return capacity_;
  }

private:
  int default_size_() {
    int d = 512 / sizeof(T);
    if (d >= 1) {
      return d;
    } else {
      return 1;
    }
  }

  size_t size_;
  T *data_;
  int64_t idx_begin_;
  int64_t idx_end_;
  size_t capacity_;
  int64_t mask_;
};
