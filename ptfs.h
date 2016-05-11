#ifndef PTFS_H_
#define PTFS_H_
// Pass-through file system.
#include <assert.h>
#include <fuse.h>
#include <unistd.h>

#include <memory>
#include <string>

#include "disallow.h"
#include "scoped_fd.h"

namespace ptfs{

class FileHandle {
public:
  explicit FileHandle(int fd) : fd_(fd) {}
  virtual ~FileHandle() {}
  int fd_get() const { return fd_.get(); }
  int fd_release() { return fd_.release(); }
private:
  ScopedFd fd_;
  DISALLOW_COPY_AND_ASSIGN(FileHandle);
};

class PtfsHandler {
public:
  PtfsHandler() {
    assert(premount_dirfd_ != -1);
  }
  virtual ~PtfsHandler() {}

  /**
   * @return >= 0 on success, -errno on fail.
   */
  virtual int GetAttr(const std::string& path, struct stat* stbuf);

  /**
   * @return >= 0 on success, -errno on fail.
   */
  virtual ssize_t Read(const FileHandle& fh, char* buf, size_t size, off_t offset);
  virtual ssize_t Write(const FileHandle& fh, const char* buf, size_t size, off_t offset);

  /**
   * @return >= 0 on success, -errno on fail.
   */
  virtual int Open(const std::string& relative_path,
		   int access_flags,
		   std::unique_ptr<FileHandle>* fh);
  /**
   * Should free the file handle and report back any errors.
   * @return >= 0 on success, -errno on fail.
   */
  virtual int Release(int access_flags, std::unique_ptr<FileHandle>* fh);

  /**
   * @return >= 0 on success, -errno on fail.
   */
  virtual int ReadDir(const std::string& path, void *buf, fuse_fill_dir_t filler,
		      off_t offset);

  virtual int Unlink(const std::string& relative_path);

  // TODO: Can this be not public and global?
  static int premount_dirfd_;

private:
  DISALLOW_COPY_AND_ASSIGN(PtfsHandler);
};

template<class T> void* fs_init(fuse_conn_info* unused) {
  return new T();
}

void FillFuseOperationsInternal(fuse_operations* o);
/**
   Initialization interface. Call this with your class of choice as
   template parameter.
 */
template<class T> void FillFuseOperations(fuse_operations* o) {
  FillFuseOperationsInternal(o);
  o->init = fs_init<T>;
}
};  // namspace ptfs
#endif
