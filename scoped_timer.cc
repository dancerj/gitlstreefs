#include "scoped_timer.h"

#include <cmath>

#include <algorithm>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>

namespace scoped_timer {
namespace {
// Global scoped variables for tracking metrics.
StatsHolder timing_stats{};
}

StatsHolder::StatsHolder() {}
StatsHolder::~StatsHolder() {}

void StatsHolder::Add(const std::string& name, DataType value) {
    std::unique_lock<std::mutex> l(m);
    stats[name][log2(value)]++;
  }

std::string StatsHolder::Dump() {
  std::stringstream ss;
  std::unique_lock<std::mutex> l(m);
  for (const auto& stat : stats) {
    const std::string& name = stat.first;
    const auto& histogram = stat.second;
    ss << name << std::endl;
    int max_value =
      std::max_element(histogram.begin(), histogram.end(),[](auto& a, auto& b){
	  return a.second < b.second;
	})->second;
    if (max_value == 0) continue;
    for (const auto& h : histogram) {
      const int item = 1 << h.first;
      const int count = h.second;
      ss << item << ":" << count << " " <<
	std::string(count * 50 / max_value, '*') << std::endl;
    }
  }
  return ss.str();
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
