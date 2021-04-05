#ifndef DPE_DPE_UTIL_H_
#define DPE_DPE_UTIL_H_

#include <cstdint>
#include <cassert>
#include <utility>
typedef int64_t int64;

namespace dpe_util {
struct RangeBasedTaskGenerator {
  struct Task {
    int64 id;
    int operator==(const Task& other) const { return id == other.id; }
    int operator!=(const Task& other) const { return !(*this == other); }
    Task& operator*() { return *this; }
    Task& operator++() {
      ++id;
      return *this;
    }
  };

  RangeBasedTaskGenerator(int64 first = 0, int64 last = 0, int64 block_size = 1)
      : first_(first), last_(last), block_size_(block_size) {
    assert(block_size_ >= 1);
    const int64 cnt = last_ - first_ + 1;
    first_task_ = 1;
    last_task_ = (cnt + block_size - 1) / block_size;
  }

  ~RangeBasedTaskGenerator() {}

  RangeBasedTaskGenerator& setRange(int64 first = 0, int64 last = 0,
                                    int64 block_size = 1) {
    first_ = first;
    last_ = last;
    block_size_ = block_size;
    assert(block_size_ >= 1);
    const int64 cnt = last_ - first_ + 1;
    first_task_ = 1;
    last_task_ = (cnt + block_size - 1) / block_size;
    return *this;
  }

  Task firstTask() const { return {first_task_}; }

  Task nextTask(Task curr) const {
    curr.id++;
    return curr;
  }

  Task lastTask() const { return {last_task_}; }

  Task begin() const { return {first_task_}; }

  Task end() const { return {last_task_ + 1}; }

  int64 count() const { return last_task_ - first_task_ + 1; }

  std::pair<int64, int64> toRange(int64 id) const {
    assert(id >= first_task_ && id <= last_task_);

    int64 u = (id - 1) * block_size_ + first_;
    int64 v = u + block_size_ - 1;
    if (v > last_) v = last_;

    return {u, v};
  }
  int64 block_size_;
  int64 first_;
  int64 last_;
  int64 first_task_;
  int64 last_task_;
};
typedef RangeBasedTaskGenerator RBTG;
}  // namespace dpe_util
namespace du = dpe_util;

#endif