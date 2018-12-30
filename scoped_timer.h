/**
 * Utility class to dump usecs.
 */
#include <chrono>
#include <string>
#include <map>

#include "directory_container.h"

/*
  system_clock, monotonic_clock and high_resolution_clock are supposed to exist,
  we have steady_clock and system_lock.
 */
namespace scoped_timer {

class ScopedTimer {
public:
  explicit ScopedTimer(const std::string name) : name_(name),
    begin_(std::chrono::steady_clock::now()) {}

  ~ScopedTimer();
  static std::string dump();
private:
  const std::string name_;

  std::chrono::steady_clock::time_point begin_;
  DISALLOW_COPY_AND_ASSIGN(ScopedTimer);
};

class StatusHandler : public directory_container::File {
public:
  StatusHandler();
  virtual ~StatusHandler();

  virtual int Getattr(struct stat *stbuf) override;
  virtual ssize_t Read(char *buf, size_t size, off_t offset) override;
  virtual int Open() override;
  virtual int Release() override;
private:
  void RefreshMessage();
  std::string message_;
  DISALLOW_COPY_AND_ASSIGN(StatusHandler);
};

class StatsHolder {
public:
  StatsHolder();
  ~StatsHolder();
  typedef long DataType;
  void Add(const std::string& name, DataType value);
  std::string Dump();
private:
  std::mutex m{};
  std::unordered_map<std::string /* title */, std::map<int /* bucket */, size_t /* count */> > stats{};
};
} // scoped_timer
