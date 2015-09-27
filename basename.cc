#include <string>

#include "basename.h"

const std::string BaseName(const std::string n) {
  size_t i = n.rfind("/");
  if (i == std::string::npos)
    return n;
  return n.substr(i + 1);
}
