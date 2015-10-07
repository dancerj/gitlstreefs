#include <condition_variable>
#include <mutex>

#include "concurrency_limit.h"

using std::unique_lock;
using std::mutex;
using std::condition_variable;

ScopedConcurrencyLimit::ScopedConcurrencyLimit() {
  unique_lock<mutex> l(m_);
  if (jobs_ > kLimit) {
    cv_.wait(l, []{
	return jobs_ <= kLimit;
      });
  }
  jobs_++;
}
ScopedConcurrencyLimit::~ScopedConcurrencyLimit() {
  {
    unique_lock<mutex> l(m_);
    jobs_--;
  }
  cv_.notify_one();
}

size_t ScopedConcurrencyLimit::jobs_{0};
mutex ScopedConcurrencyLimit::m_{};
condition_variable ScopedConcurrencyLimit::cv_{};
