#include "scoped_fileutil.h"

#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <syslog.h>

#include <string>

using std::string;

ScopedTempFile::ScopedTempFile(int dirfd, const string& basename,
                               const string& opt)
    : dirfd_(dirfd),
      name_(basename + ".tmp" + opt + std::to_string(pthread_self())) {
  if (-1 == unlinkat(dirfd, name_.c_str(), 0) && errno != ENOENT) {
    syslog(LOG_ERR, "unlinkat ScopedTempFile %s %m", name_.c_str());
    abort();  // Probably a race condition?
  }
}

ScopedTempFile::~ScopedTempFile() {
  if (name_.size() > 0) {
    if (-1 == unlinkat(dirfd_, name_.c_str(), 0)) {
      syslog(LOG_ERR, "unlinkat ~ScopedTempFile %s %m", name_.c_str());
      abort();  // Logic error somewhere or a race condition.
    }
  }
}

ScopedFileLockWithDelete::ScopedFileLockWithDelete(int dirfd,
                                                   const string& basename)
    : dirfd_(dirfd), name_(basename + ".lock") {
  while (true) {
    int e = mkdirat(dirfd_, name_.c_str(), 0700);
    if (e == 0) {
      have_lock_ = true;
      return;
    }
    if (e == -1 && errno == EEXIST) {
      // sleep 1ms.
      usleep(1000);
      continue;
    }
    syslog(LOG_ERR, "mkdirat %s %m", name_.c_str());
  }
  have_lock_ = true;
}

ScopedFileLockWithDelete::~ScopedFileLockWithDelete() {
  if (!have_lock_) return;
  if (-1 == unlinkat(dirfd_, name_.c_str(), AT_REMOVEDIR)) {
    syslog(LOG_ERR, "unlinkat FileLock %s %m", name_.c_str());
  }
}
