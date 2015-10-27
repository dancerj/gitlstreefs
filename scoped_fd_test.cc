#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

#include <iostream>

#include "scoped_fd.h"

using namespace std;

void TestMove() {
  // assume a file that is larger than 10 bytes.
  ScopedFd fd(open("scoped_fd_test.cc", O_RDONLY));
  ScopedFd fd2 = move(fd);
  assert(fd.get() == -1);
  assert(fd2.get() != -1);
  char buf[11];
  size_t s = read(fd2.get(), buf, 10);
  assert(s == 10);
}

void TestOpenFailAndReset() {
  ScopedFd fd(-1);
  assert(fd.get() == -1);
  fd.reset(open("scoped_fd_test.cc", O_RDONLY));
  assert(fd.get() != -1);
}

int main(int argc, char** argv) {
  TestMove();
  TestOpenFailAndReset();
  return 0;
}
