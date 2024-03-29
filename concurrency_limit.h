#ifndef CONCURRENCY_LIMIT_H_
#define CONCURRENCY_LIMIT_H_

#include <condition_variable>
#include <mutex>
#include <set>
#include <string>

#include "disallow.h"

class ScopedConcurrencyLimit {
 public:
  explicit ScopedConcurrencyLimit(const std::string& message);
  ~ScopedConcurrencyLimit();

 private:
  void DumpStatusLocked() const;

  static constexpr size_t kLimit = 6;
  inline static std::mutex m_{};
  inline static std::condition_variable cv_{};
  inline static std::set<const std::string*> messages_{};

  const std::string message_;
  DISALLOW_COPY_AND_ASSIGN(ScopedConcurrencyLimit);
};

#endif
