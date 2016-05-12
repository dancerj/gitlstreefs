#include "concurrency_limit.h"

#include <condition_variable>
#include <iostream>
#include <mutex>

using std::unique_lock;
using std::mutex;
using std::condition_variable;
using std::string;
using std::set;
using std::cout;

void ScopedConcurrencyLimit::DumpStatus() {
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
    DumpStatus();
    cv_.wait(l, []{
	return messages_.size() <= kLimit;
      });
  }
  messages_.insert(&message_);
}

ScopedConcurrencyLimit::~ScopedConcurrencyLimit() {
  {
    unique_lock<mutex> l(m_);
    messages_.erase(&message_);
    DumpStatus();
  }
  cv_.notify_one();
}

mutex ScopedConcurrencyLimit::m_{};
condition_variable ScopedConcurrencyLimit::cv_{};
set<const string*> ScopedConcurrencyLimit::messages_{};
