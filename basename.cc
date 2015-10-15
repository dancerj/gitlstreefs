#include <string>

#include "basename.h"

const std::string BaseName(const std::string n) {
  size_t i = n.rfind("/");
  if (i == std::string::npos)
    return n;
  return n.substr(i + 1);
}

const std::string DirName(const std::string n) {
  size_t i = n.rfind("/");
  if (i == std::string::npos) return "";
  return n.substr(0, i);
}
