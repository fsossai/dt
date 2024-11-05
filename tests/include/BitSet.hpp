#pragma once

#include <cstdint>
#include <vector>

template <class T>
class BitSet {
public:
  BitSet(int size) : buf_((size + 7) / 8), capacity_(buf_.size() * 8) {}

  BitSet() : BitSet(1) {}

  void insert(T value) {
    if (value >= capacity_) {
      buf_.resize(value / 8 + 1);
      capacity_ = buf_.size() * 8;
    }
    buf_[value / 8] |= 1 << (value % 8);
  }

  bool find(T value) const {
    if (value >= capacity_) {
      return false;
    }
    return buf_[value / 8] & (1 << (value % 8));
  }

  bool end() const {
    return false;
  }

private:
  std::size_t get_capacity() const {
    return buf_.size() * 8;
  }
  std::vector<uint8_t> buf_;
  std::size_t capacity_;
};
