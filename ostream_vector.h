#ifndef OSTREAM_VECTOR_H_
#define OSTREAM_VECTOR_H_
namespace {
std::ostream& operator<<(std::ostream& os, const std::vector<std::string>& v) {
  bool first = true;
  for (const auto& i : v) {
    if (!first) {
      os << ", ";
    }
    first = false;
    os << "\"" << i << "\"";
  }
  os << std::endl;
  return os;
}
} // namespace
#endif
