#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string>

#include "jsonparser.h"
#include "strutil.h"

int main(int ac, char** av) {
  if (ac != 3) {
    fprintf(stderr, "%s filename iteration\n", av[0]);
    return 1;
  }
  size_t iter = atoi(av[2]);
  std::string j = ReadFromFileOrDie(AT_FDCWD, av[1]);
  for (size_t i = 0; i < iter; ++i) {
    std::unique_ptr<jjson::Value> p = jjson::Parse(j);
    assert(p.get() != nullptr);
  }
  return 0;
}
