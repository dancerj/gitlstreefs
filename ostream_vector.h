#ifndef OSTREAM_VECTOR_H_
#define OSTREAM_VECTOR_H_
#include <iterator>

namespace {
std::ostream& operator<<(std::ostream& os, const std::vector<std::string>& v) {
  os << "\"";
  copy(v.begin(), v.end(), std::ostream_iterator<std::string>(std::cout, "\", \""));
  os << "\"" << std::endl;
  return os;
}
}  // namespace
#endif
