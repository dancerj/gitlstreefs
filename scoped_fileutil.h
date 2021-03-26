#ifndef SCOPED_FILEUTIL_H_
#define SCOPED_FILEUTIL_H_

#include <string>

#include "scoped_fd.h"

class ScopedTempFile {
 public:
  ScopedTempFile(int dirfd, const std::string& basename,
                 const std::string& opt);
  ~ScopedTempFile();

  // Lose ownership of the tmpfile, and no longer delete the file on
  // destructor.
  void clear() { name_.clear(); };

  const std::string& get() const { return name_; };
  const char* c_str() const { return name_.c_str(); };

 private:
  int dirfd_;
  std::string name_;
  DISALLOW_COPY_AND_ASSIGN(ScopedTempFile);
};

class ScopedFileLockWithDelete {
 public:
  ScopedFileLockWithDelete(int dirfd, const std::string& basename);
  ~ScopedFileLockWithDelete();

 private:
  int dirfd_;
  std::string name_;
  bool have_lock_ = false;
  DISALLOW_COPY_AND_ASSIGN(ScopedFileLockWithDelete);
};
#endif
