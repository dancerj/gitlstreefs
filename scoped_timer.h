/**
 * Utility class to dump usecs.
 */
#include <chrono>
#include <string>
#include <iostream>

/*
  system_clock, monotonic_clock and high_resolution_clock are supposed to exist,
  we have steady_clock and system_lock.
 */

class ScopedTimer {
public:
  explicit ScopedTimer(const std::string name) : name_(name),
    begin_(std::chrono::steady_clock::now()) {}

  ~ScopedTimer() {
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    std::chrono::microseconds diff_usec(std::chrono::duration_cast<std::chrono::microseconds>(end - begin_));
    std::cout << name_ << " " << diff_usec.count() <<
      std::endl;
  }

  int get() const {
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(end - begin_).count();
  }
private:
  const std::string name_;

  std::chrono::steady_clock::time_point begin_;
  std::chrono::steady_clock::time_point end_;
};
