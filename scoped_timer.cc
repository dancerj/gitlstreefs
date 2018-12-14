#include "scoped_timer.h"

#include <cmath>

#include <algorithm>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>

namespace {
// Global scoped variables for tracking metrics.
std::mutex m{};
std::unordered_map<std::string /* title */, std::map<int /* bucket */, int /* count */> > stats{};
}

ScopedTimer::~ScopedTimer() {
  std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
  std::chrono::microseconds diff_usec(std::chrono::duration_cast<std::chrono::microseconds>(end - begin_));
  std::cout << name_ << " " << diff_usec.count() <<
    std::endl;
  {
    std::unique_lock<std::mutex> l(m);
    stats[name_][log2(diff_usec.count())] ++;
  }
}

/*static*/ std::string ScopedTimer::dump() {
  std::stringstream ss;
  for (const auto& stat : stats) {
    const std::string& name = stat.first;
    const std::map<int, int>& histogram = stat.second;
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
	std::string(count * 50 / max_value, 'a') << std::endl;
    }
  }
  return ss.str();
}
