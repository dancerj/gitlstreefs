/**
 * Utility class to dump usecs.
 */
#include <chrono>
#include <string>

/*
  system_clock, monotonic_clock and high_resolution_clock are supposed to exist,
  we have steady_clock and system_lock.
 */

class ScopedTimer {
public:
  explicit ScopedTimer(const std::string name) : name_(name),
    begin_(std::chrono::steady_clock::now()) {}

  ~ScopedTimer();
  static std::string dump();
private:
  const std::string name_;

  std::chrono::steady_clock::time_point begin_;
};
