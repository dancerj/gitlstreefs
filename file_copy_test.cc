#include <string>
#include <fcntl.h>

#include "file_copy.h"

int main(int argc, char** argv) {
  if (argc == 3) {
    return FileCopy(AT_FDCWD, argv[1], argv[2]) ? 0 : 1;
  }
}
