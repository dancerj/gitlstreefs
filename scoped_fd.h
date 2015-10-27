#include <unistd.h>

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
    if (fd_ != -1) {
      close(fd_);
    }
  };

  int release() {
    int ret = fd_;
    fd_ = -1;
    return ret;
  }

  int get() const { return fd_; }

 private:
  // file descriptor, or -1 if not set.
  int fd_;
};
