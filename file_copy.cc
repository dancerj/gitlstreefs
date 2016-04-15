#include <fcntl.h>
#include <string>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/types.h>

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

  ssize_t read_bytes = sendfile(to_fd.get(), from_fd, NULL, st.st_size);
  if (read_bytes == -1) {
    perror("sendfile");
    return false;
  }
  if(read_bytes != st.st_size) {
    return false;
  }
  if (-1 == fchmod(to_fd.get(), st.st_mode)) {
    perror("fchmod");
    return false;
  }
  if (-1 == futimens(to_fd.get(), &st.st_mtim)) {
    perror("futimens");
    return false;
  }
  if (-1 == fchown(to_fd.get(), st.st_uid, st.st_gid)) {
    perror("fchown");
    return false;
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
