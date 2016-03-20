#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "file_copy.h"
#include "scoped_fd.h"

using std::string;

bool FileCopyInternal(int dirfd, int from_fd, const struct stat& st,
		      const string& target) {
  ScopedFd to_fd(openat(dirfd, target.c_str(), O_WRONLY | O_CREAT, st.st_mode));
  if (to_fd.get() == -1) {
    perror(("open " + target).c_str());
    return false;
  }

  if (-1 == posix_fallocate(to_fd.get(), 0, st.st_size)) {
    perror("posix_fallocate");
    return false;
  }

  char buf[1024 * 1024];
  ssize_t nread;
  while ((nread = read(from_fd, buf, sizeof buf)) != 0) {
    if (nread == -1) {
      perror("read");
      return false;
    }
    if (nread != write(to_fd.get(), buf, nread)) {
      perror("write");
      return false;
    }
  }
  return true;
}

bool FileCopy(int dirfd, const std::string& source, const std::string& target) {
  ScopedFd from_fd(openat(dirfd, source.c_str(), O_RDONLY, 0));
  struct stat st{};
  if (-1 == fstat(from_fd.get(), &st)) {
    // I wonder why fstat can fail here.
    perror("fstat");
    return false;
  }

  return FileCopyInternal(dirfd, from_fd.get(), st, target);
}
