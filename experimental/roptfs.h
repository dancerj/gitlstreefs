#ifndef ROPTFS_H_
#define ROPTFS_H_
// Read-only pass-through file system.
#include <assert.h>
#include <fuse.h>
#include <unistd.h>

#include <memory>
#include <string>

#include "../disallow.h"

namespace roptfs {

class FileHandle {
 public:
  explicit FileHandle(int fd) : fd_(fd) {}
  virtual ~FileHandle() {
    if (fd_ == -1) return;
    assert(-1 != close(fd_));
  }
  int get() const { return fd_; }

 private:
  int fd_;
  DISALLOW_COPY_AND_ASSIGN(FileHandle);
};

class RoptfsHandler {
 public:
  RoptfsHandler() { assert(premount_dirfd_ != -1); }
  virtual ~RoptfsHandler() {}

  /**
   * @return >= 0 on success, -errno on fail.
   */
  virtual int GetAttr(const std::string& path, struct stat* stbuf);

  /**
   * @return >= 0 on success, -errno on fail.
   */
  virtual ssize_t Read(const FileHandle& fh, char* buf, size_t size,
                       off_t offset);

  /**
   * @return >= 0 on success, -errno on fail.
   */
  virtual int Open(const std::string& relative_path,
                   std::unique_ptr<FileHandle>* fh);

  /**
   * @return >= 0 on success, -errno on fail.
   */
  virtual int ReadDir(const std::string& path, void* buf,
                      fuse_fill_dir_t filler, off_t offset);

  // TODO: Can this be not public and global?
  static int premount_dirfd_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RoptfsHandler);
};

template <class T>
void* fs_init(fuse_conn_info* unused) {
  return new T();
}

void FillFuseOperationsInternal(fuse_operations* o);
/**
   Initialization interface. Call this with your class of choice as
   template parameter.
 */
template <class T>
void FillFuseOperations(fuse_operations* o) {
  FillFuseOperationsInternal(o);
  o->init = fs_init<T>;
}
};  // namespace roptfs
#endif
