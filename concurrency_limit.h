#ifndef CONCURRENCY_LIMIT_H_
#define CONCURRENCY_LIMIT_H_

#include <condition_variable>
#include <mutex>
#include <set>

#include "disallow.h"

class ScopedConcurrencyLimit {
public:
  explicit ScopedConcurrencyLimit(const std::string& message);
  ~ScopedConcurrencyLimit();
private:
  void DumpStatus();
  static constexpr size_t kLimit = 6;
  std::string message_;
  static std::mutex m_;
  static std::condition_variable cv_;
  static std::set<const std::string*> messages_;
  DISALLOW_COPY_AND_ASSIGN(ScopedConcurrencyLimit);
};

#endif
