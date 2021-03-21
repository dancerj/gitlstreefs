#include "concurrency_limit.h"

#include <condition_variable>
#include <iostream>
#include <mutex>

using std::condition_variable;
using std::cout;
using std::lock_guard;
using std::unique_lock;
using std::mutex;
using std::set;
using std::string;

void ScopedConcurrencyLimit::DumpStatusLocked() const {
  // TODO: assert lock is held.
  string output;
  for (const string* message : messages_) {
    output += *message + " ";
  }
  output += "                                      ";
  cout << output.substr(0, 79) << "\r" << std::flush;
}

ScopedConcurrencyLimit::ScopedConcurrencyLimit(const string& message) :
  message_(message) {
  unique_lock<mutex> l(m_);
  if (messages_.size() > kLimit) {
    DumpStatusLocked();
    cv_.wait(l, []{
	return messages_.size() <= kLimit;
      });
  }
  messages_.insert(&message_);
}

ScopedConcurrencyLimit::~ScopedConcurrencyLimit() {
  {
    lock_guard<mutex> l(m_);
    messages_.erase(&message_);
    DumpStatusLocked();
  }
  cv_.notify_one();
}
