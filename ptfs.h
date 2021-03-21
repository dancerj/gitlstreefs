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
  PtfsHandler();
  virtual ~PtfsHandler();

  /**
   * @return >= 0 on success, -errno on fail.
   */
  virtual int GetAttr(const std::string& path, struct stat* stbuf);

  /**
   * @return read size >= 0 on success, -errno on fail.
   */
  virtual ssize_t Read(const FileHandle& fh, char* buf, size_t size, off_t offset);

  /**
   * @return 0 on success, -errno on fail.
   */
  virtual int ReadBuf(const FileHandle& fh, struct fuse_bufvec &buf, size_t size, off_t offset);

  /**
   * @return write size >= 0 on success, -errno on fail.
   */
  virtual ssize_t Write(const FileHandle& fh, const char* buf, size_t size, off_t offset);

  /**
   * Responsible for allocating the FileHandle to fh on successful invocation.
   * @return >= 0 on success, -errno on fail.
   */
  virtual int Open(const std::string& relative_path,
		   int access_flags,
		   std::unique_ptr<FileHandle>* fh);
  /**
   * Similar to Open with extra mode parameter.
   * Responsible for allocating the FileHandle to fh on successful invocation.
   * @return >= 0 on success, -errno on fail.
   */
  virtual int Create(const std::string& relative_path,
		     int access_flags,
		     mode_t mode,
		     std::unique_ptr<FileHandle>* fh);

  /**
   * Should deinitialize the file handle and report back any errors.
   * @return >= 0 on success, -errno on fail.
   */
  virtual int Release(int access_flags, FileHandle* fh);

  /**
   * @return >= 0 on success, -errno on fail.
   */
  virtual int ReadDir(const std::string& path, void *buf, fuse_fill_dir_t filler,
		      off_t offset);

  /**
   * @return >= 0 on success, -errno on fail.
   */
  virtual int Unlink(const std::string& relative_path);

  virtual int Chmod(const std::string& relative_path, mode_t mode);
  virtual int Chown(const std::string& relative_path, uid_t uid, gid_t gid);
  virtual int Truncate(const std::string& relative_path, off_t size);
  virtual int Utimens(const std::string& relative_path, const struct timespec ts[2]);
  virtual int Mknod(const std::string& relative_path, mode_t mode, dev_t rdev);
  virtual int Link(const std::string& relative_path_from, const std::string& relative_path_to);
  virtual int Statfs(struct statvfs *stbuf);

  virtual int Symlink(const char* from, const std::string& to);
  virtual int Readlink(const std::string& relative_path, char* buf, size_t size);
  virtual int Mkdir(const std::string& relative_path, mode_t mode);
  virtual int Rmdir(const std::string& relative_path);
  virtual int Fsync(FileHandle* fh, int isdatasync);
  virtual int Fallocate(FileHandle* fh, int mode, off_t offset, off_t length);
  virtual int Setxattr(const std::string& relative_path, const char *name, const char *value,
		       size_t size, int flags);
  virtual ssize_t Getxattr(const std::string& relative_path, const char *name,
			   char *value, size_t size);
  virtual ssize_t Listxattr(const std::string& relative_path,
			    char *list, size_t size);
  virtual int Removexattr(const std::string& relative_path, const char *name);
  virtual int Rename(const std::string& relative_path_from, const std::string& relative_path_to);

  /**
   * File descriptor where all operations happen relative to.
   */
  // TODO: Can this be not public and global?
  // Directory before mount.
  inline static int premount_dirfd_{-1};

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
}  // namspace ptfs
#endif
