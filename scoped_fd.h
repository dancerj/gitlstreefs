#ifndef SCOPED_FD_H__
#define SCOPED_FD_H__

#include <unistd.h>

#include "disallow.h"

class ScopedFd {
public:
  explicit ScopedFd(int fd) : fd_(fd) {}

  // move constructor
  ScopedFd(ScopedFd&& target) : fd_(target.release()) {}

  // move assignment.
  ScopedFd& operator=(ScopedFd&& target) {
    fd_ = target.release();
    return *this;
  }

  ~ScopedFd() {
    MaybeClose();
  };

  int release() {
    int ret = fd_;
    fd_ = -1;
    return ret;
  }

  void reset(int fd) {
    MaybeClose();
    fd_ = fd;
  }

  void clear() {
    reset(-1);
  }

  int get() const { return fd_; }

 private:
  // file descriptor, or -1 if not set.
  int fd_;

  void MaybeClose() {
    if (fd_ != -1) {
      close(fd_);
    }
    fd_ = -1;
  }

  DISALLOW_COPY_AND_ASSIGN(ScopedFd);
};
#endif
