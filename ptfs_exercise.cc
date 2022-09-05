#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cassert>
#include <string>

// A simple program to play with ptfs.

int main() {
  int fd = open("out/ptfstmp/README.md", O_RDONLY);
  assert(fd != -1);
  char buf[512];
  ssize_t count = read(fd, buf, sizeof buf);
  assert(count > 400);
  std::string b{buf};
  assert(b.starts_with("# Test data directory"));
  close(fd);
}
