#ifndef STATS_HOLDER_H_
#define STATS_HOLDER_H_
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>

#include "disallow.h"

namespace stats_holder {
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
  DISALLOW_COPY_AND_ASSIGN(StatsHolder);
};

}  // stats_holder
#endif
