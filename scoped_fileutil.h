#ifndef SCOPED_LOCKFILE_H
#define SCOPED_LOCKFILE_H
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <syslog.h>

#include <string>

#include "scoped_fd.h"

class ScopedTempFile {
public:
  ScopedTempFile(int dirfd, const std::string& basename, const std::string& opt) :
    dirfd_(dirfd),
    name_(basename + ".tmp" + opt + std::to_string(pthread_self())) {
    if (-1 == unlinkat(dirfd, name_.c_str(), 0) && errno != ENOENT) {
      syslog(LOG_ERR, "unlinkat %s %m", name_.c_str());
      abort();  // Probably a race condition?
    }
  }
  ~ScopedTempFile() {
    if (name_.size() > 0) {
      if (-1 == unlinkat(dirfd_, name_.c_str(), 0)) {
	syslog(LOG_ERR, "unlinkat tmpfile %s %m", name_.c_str());
	abort();   // Logic error somewhere or a race condition.
      }
    }
  }
  void clear() {
    name_.clear();
  };
  const std::string& get() const { return name_; };
  const char* c_str() const { return name_.c_str(); };
private:
  int dirfd_;
  std::string name_;
  DISALLOW_COPY_AND_ASSIGN(ScopedTempFile);
};

class ScopedFileLockWithDelete {
 public:
  ScopedFileLockWithDelete(int dirfd, const std::string& basename) :
    dirfd_(dirfd),
    name_(basename + ".lock"),
    have_lock_(false) {
    while (true) {
      int e = mkdirat(dirfd_, name_.c_str(), 0700);
      if (e == 0) {
	have_lock_ = true;
	return;
      }
      if (e == -1 && errno == EEXIST) {
	// sleep 1 second?
	sleep(1);
	continue;
      }
      syslog(LOG_ERR, "mkdirat %s %m", name_.c_str());
    }
    have_lock_ = true;
  }
  ~ScopedFileLockWithDelete() {
    if (!have_lock_) return;
    if (-1 == unlinkat(dirfd_, name_.c_str(), AT_REMOVEDIR)) {
      syslog(LOG_ERR, "unlinkat %s %m", name_.c_str());
    }
  }
 private:
  int dirfd_;
  std::string name_;
  bool have_lock_;
  DISALLOW_COPY_AND_ASSIGN(ScopedFileLockWithDelete);
};
#endif
