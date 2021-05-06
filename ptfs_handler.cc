#define FUSE_USE_VERSION 35

#include "ptfs.h"

#include <attr/xattr.h>
#include <dirent.h>
#include <fcntl.h>

#include <memory>
#include <string>

#include "scoped_fd.h"

using std::string;
using std::unique_ptr;

namespace ptfs {

#define WRAP_ERRNO(f) \
  if (-1 == f) {      \
    return -errno;    \
  } else {            \
    return 0;         \
  }

#define WRAP_ERRNO_OR_RESULT(f) \
  auto res = f;                 \
  if (-1 == res) {              \
    return -errno;              \
  } else {                      \
    return res;                 \
  }

#define WITH_FD(relative_path, mode)                                 \
  ScopedFd fd(openat(premount_dirfd_, relative_path.c_str(), mode)); \
  if (fd.get() == -1) {                                              \
    return -errno;                                                   \
  }

PtfsHandler::PtfsHandler() { assert(premount_dirfd_ != -1); }

PtfsHandler::~PtfsHandler() {}

int PtfsHandler::GetAttr(const std::string& relative_path, struct stat* stbuf) {
  WRAP_ERRNO(fstatat(premount_dirfd_, relative_path.c_str(), stbuf,
                     AT_SYMLINK_NOFOLLOW));
}

ssize_t PtfsHandler::Read(const FileHandle& fh, char* target, size_t size,
                          off_t offset) {
  WRAP_ERRNO_OR_RESULT(pread(fh.fd_get(), target, size, offset));
}

int PtfsHandler::ReadBuf(const FileHandle& fh, struct fuse_bufvec& buf,
                         size_t size, off_t offset) {
  // enum with | becomes int.
  buf.buf[0].flags =
      static_cast<fuse_buf_flags>(FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK);
  buf.buf[0].fd = fh.fd_get();
  buf.buf[0].pos = offset;

  return 0;
}

ssize_t PtfsHandler::Write(const FileHandle& fh, const char* buf, size_t size,
                           off_t offset) {
  WRAP_ERRNO_OR_RESULT(pwrite(fh.fd_get(), buf, size, offset));
}

int PtfsHandler::Open(const std::string& relative_path, int access_flags,
                      unique_ptr<FileHandle>* fh) {
  int fd = openat(premount_dirfd_, relative_path.c_str(), access_flags);
  if (fd == -1) return -ENOENT;
  fh->reset(new FileHandle(fd));

  return 0;
}

int PtfsHandler::Create(const std::string& relative_path, int access_flags,
                        mode_t mode, unique_ptr<FileHandle>* fh) {
  int fd = openat(premount_dirfd_, relative_path.c_str(), access_flags, mode);
  if (fd == -1) return -ENOENT;
  fh->reset(new FileHandle(fd));

  return 0;
}

int PtfsHandler::Release(int access_flags, FileHandle* fh) {
  WRAP_ERRNO(close(fh->fd_release()));
}

int PtfsHandler::Unlink(const std::string& relative_path) {
  WRAP_ERRNO(unlinkat(premount_dirfd_, relative_path.c_str(), 0));
}

int PtfsHandler::ReadDir(const std::string& relative_path, void* buf,
                         fuse_fill_dir_t filler, off_t offset) {
  // Host directory would already contain . and .. so just pass them through.
  struct dirent** namelist{nullptr};
  int scandir_count = scandirat(premount_dirfd_, relative_path.c_str(),
                                &namelist, nullptr, nullptr);
  if (scandir_count == -1) {
    return -ENOENT;
  }
  for (int i = 0; i < scandir_count; ++i) {
    filler(buf, namelist[i]->d_name, nullptr, 0, fuse_fill_dir_flags{});
    free(namelist[i]);
  }
  free(namelist);
  return 0;
}

int PtfsHandler::Chmod(const std::string& relative_path, mode_t mode) {
  WRAP_ERRNO(fchmodat(premount_dirfd_, relative_path.c_str(), mode, 0));
}

int PtfsHandler::Chown(const std::string& relative_path, uid_t uid, gid_t gid) {
  WRAP_ERRNO(fchownat(premount_dirfd_, relative_path.c_str(), uid, gid,
                      AT_SYMLINK_NOFOLLOW));
}

int PtfsHandler::Truncate(const std::string& relative_path, off_t size) {
  WITH_FD(relative_path, O_WRONLY);
  WRAP_ERRNO(ftruncate(fd.get(), size));
}

int PtfsHandler::Utimens(const std::string& relative_path,
                         const struct timespec ts[2]) {
  WRAP_ERRNO(utimensat(premount_dirfd_, relative_path.c_str(), ts,
                       AT_SYMLINK_NOFOLLOW));
}

int PtfsHandler::Mknod(const std::string& relative_path, mode_t mode,
                       dev_t rdev) {
  WRAP_ERRNO(mknodat(premount_dirfd_, relative_path.c_str(), mode, rdev));
}

int PtfsHandler::Link(const std::string& relative_path_from,
                      const std::string& relative_path_to) {
  WRAP_ERRNO(linkat(premount_dirfd_, relative_path_from.c_str(),
                    premount_dirfd_, relative_path_to.c_str(), 0));
}

int PtfsHandler::Statfs(struct statvfs* stbuf) {
  WRAP_ERRNO(fstatvfs(premount_dirfd_, stbuf));
}

int PtfsHandler::Symlink(const char* from, const string& to) {
  WRAP_ERRNO(symlinkat(from, premount_dirfd_, to.c_str()));
}

int PtfsHandler::Readlink(const string& relative_path, char* buf, size_t size) {
  int res;
  if ((res = readlinkat(premount_dirfd_, relative_path.c_str(), buf,
                        size - 1)) == -1) {
    return -errno;
  } else {
    buf[res] = '\0';
    return 0;
  }
}

int PtfsHandler::Mkdir(const string& relative_path, mode_t mode) {
  WRAP_ERRNO(mkdirat(premount_dirfd_, relative_path.c_str(), mode));
}

int PtfsHandler::Rmdir(const string& relative_path) {
  WRAP_ERRNO(unlinkat(premount_dirfd_, relative_path.c_str(), AT_REMOVEDIR));
}

int PtfsHandler::Fsync(FileHandle* fh, int isdatasync) {
  if (isdatasync) {
    WRAP_ERRNO(fdatasync(fh->fd_get()));
  } else {
    WRAP_ERRNO(fsync(fh->fd_get()));
  }
}

int PtfsHandler::Fallocate(FileHandle* fh, int mode, off_t offset,
                           off_t length) {
  if (mode) {
    return -EOPNOTSUPP;
  }
  return -posix_fallocate(fh->fd_get(), offset, length);
}

int PtfsHandler::Setxattr(const string& relative_path, const char* name,
                          const char* value, size_t size, int flags) {
  WITH_FD(relative_path, O_RDONLY);
  WRAP_ERRNO(fsetxattr(fd.get(), name, value, size, flags));
}

ssize_t PtfsHandler::Getxattr(const string& relative_path, const char* name,
                              char* value, size_t size) {
  WITH_FD(relative_path, O_RDONLY);
  WRAP_ERRNO_OR_RESULT(fgetxattr(fd.get(), name, value, size));
}

ssize_t PtfsHandler::Listxattr(const string& relative_path, char* list,
                               size_t size) {
  WITH_FD(relative_path, O_RDONLY);
  WRAP_ERRNO_OR_RESULT(flistxattr(fd.get(), list, size));
}

int PtfsHandler::Removexattr(const string& relative_path, const char* name) {
  WITH_FD(relative_path, O_RDONLY);
  WRAP_ERRNO(fremovexattr(fd.get(), name));
}

int PtfsHandler::Rename(const string& relative_path_from,
                        const string& relative_path_to,
                        unsigned int rename_flags) {
  WRAP_ERRNO(renameat2(premount_dirfd_, relative_path_from.c_str(),
                       premount_dirfd_, relative_path_to.c_str(),
                       rename_flags));
}

}  // namespace ptfs
