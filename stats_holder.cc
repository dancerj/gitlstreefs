#include "stats_holder.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace stats_holder {

StatsHolder::StatsHolder() {}
StatsHolder::~StatsHolder() {}

void StatsHolder::Add(const std::string& name, DataType value) {
  std::lock_guard<std::mutex> l(m);
  stats[name][log2(value)]++;
}

std::string StatsHolder::Dump() {
  std::stringstream ss;
  std::lock_guard<std::mutex> l(m);
  constexpr int kColumns = 50;
  for (const auto& stat : stats) {
    const std::string& name = stat.first;
    const auto& histogram = stat.second;
    ss << name << std::endl;
    int max_value =
        std::max_element(histogram.begin(), histogram.end(),
                         [](auto& a, auto& b) { return a.second < b.second; })
            ->second;
    if (max_value == 0) continue;
    for (const auto& h : histogram) {
      const int item = 1 << h.first;
      const int count = h.second;
      ss << item << ":" << count << " "
         << std::string(count * kColumns / max_value, '*') << std::endl;
    }
  }
  return ss.str();
}
}  // namespace stats_holder
