#include <fcntl.h>
#include <stdio.h>

#include <cassert>
#include <string>

int main(int argc, char** argv) {
  unsigned int flags = 0;

  assert(argc == 4);

  const char* from = argv[1];
  const char* to = argv[2];
  std::string flag{argv[3]};
#define FLAG(f) \
  if (flag == #f) flags |= f;
  FLAG(RENAME_EXCHANGE);
  FLAG(RENAME_NOREPLACE);
  if (-1 == renameat2(AT_FDCWD, from, AT_FDCWD, to, flags)) {
    perror("renameat2");
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
