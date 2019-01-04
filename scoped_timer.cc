#include "stats_holder.h"
#include "scoped_timer.h"

#include <unistd.h>
#include <string.h>

#include <cmath>

#include <iostream>
#include <string>

namespace scoped_timer {
namespace {
// Global scoped variables for tracking metrics.
stats_holder::StatsHolder timing_stats{};
}

ScopedTimer::~ScopedTimer() {
  std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
  std::chrono::microseconds diff_usec(std::chrono::duration_cast<std::chrono::microseconds>(end - begin_));
  std::cout << name_ << " " << diff_usec.count() <<
    std::endl;
  {
    timing_stats.Add(name_, diff_usec.count());
  }
}

/*static*/ std::string ScopedTimer::dump() {
  return timing_stats.Dump();
}

StatusHandler::StatusHandler() : message_() {}

StatusHandler::~StatusHandler() {}

int StatusHandler::Getattr(struct stat *stbuf) {
  RefreshMessage();
  stbuf->st_uid = getuid();
  stbuf->st_gid = getgid();
  stbuf->st_mode = S_IFREG | 0400;
  stbuf->st_size = message_.size();
  stbuf->st_nlink = 1;
  return 0;
}

ssize_t StatusHandler::Read(char *buf, size_t size, off_t offset) {
  if (offset < static_cast<off_t>(message_.size())) {
    if (offset + size > message_.size())
      size = message_.size() - offset;
    memcpy(buf, message_.data() + offset, size);
  } else
    size = 0;
  return size;
}

int StatusHandler::Open() {
  RefreshMessage();
  return 0;
}

int StatusHandler::Release() {
  return 0;
}

void StatusHandler::RefreshMessage() {
  message_ = ScopedTimer::dump();
}
}  // namespace scoped_timer
