#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "base64decode.h"
#include "strutil.h"

int main(int ac, char** av) {
  // Do benchmark.
  if (ac != 3) {
    fprintf(stderr, "%s filename iteration\n", av[0]);
    return 0;
  }
  size_t iter = atoi(av[2]);
  std::string b = ReadFromFileOrDie(AT_FDCWD, av[1]);
  for (size_t i = 0; i < iter; ++i) {
    base64decode(b);
  }
  return 0;
}
