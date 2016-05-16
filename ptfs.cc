#define FUSE_USE_VERSION 26

#include "ptfs.h"

#include <attr/xattr.h>
#include <dirent.h>
#include <fcntl.h>
#include <memory>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <string>

#include "relative_path.h"
#include "scoped_fd.h"

using std::unique_ptr;
using std::string;

namespace ptfs {

// Directory before mount.
int PtfsHandler::premount_dirfd_ = -1;

PtfsHandler* GetContext() {
  fuse_context* context = fuse_get_context();
  return reinterpret_cast<PtfsHandler*>(context->private_data);
}

#define WRAP_ERRNO(f)				\
  if(-1 == f) {					\
    return -errno;				\
  } else{					\
    return 0;					\
  }

#define WRAP_ERRNO_OR_RESULT(T, f)				\
  T res = f;							\
  if(-1 == res) {						\
    return -errno;						\
  } else {							\
    return res;							\
  }

int PtfsHandler::GetAttr(const std::string& relative_path, struct stat* stbuf) {
  WRAP_ERRNO(fstatat(premount_dirfd_, relative_path.c_str(), stbuf, AT_SYMLINK_NOFOLLOW));
}

ssize_t PtfsHandler::Read(const FileHandle& fh, char* target, size_t size, off_t offset) {
  WRAP_ERRNO_OR_RESULT(ssize_t, pread(fh.fd_get(), target, size, offset));
}

ssize_t PtfsHandler::Write(const FileHandle& fh, const char* buf, size_t size, off_t offset) {
  WRAP_ERRNO_OR_RESULT(ssize_t, pwrite(fh.fd_get(), buf, size, offset));
}

int PtfsHandler::Open(const std::string& relative_path, int access_flags, unique_ptr<FileHandle>* fh) {
  int fd = openat(premount_dirfd_, relative_path.c_str(), access_flags);
  if (fd == -1)
    return -ENOENT;
  fh->reset(new FileHandle(fd));

  return 0;
}

int PtfsHandler::Create(const std::string& relative_path, int access_flags,
			mode_t mode, unique_ptr<FileHandle>* fh) {
  int fd = openat(premount_dirfd_, relative_path.c_str(), access_flags, mode);
  if (fd == -1)
    return -ENOENT;
  fh->reset(new FileHandle(fd));

  return 0;
}

int PtfsHandler::Release(int access_flags, unique_ptr<FileHandle>* upfh) {
  FileHandle& fh = **upfh;
  WRAP_ERRNO(close(fh.fd_release()));
}

int PtfsHandler::Unlink(const std::string& relative_path) {
  WRAP_ERRNO(unlinkat(premount_dirfd_, relative_path.c_str(), 0));
}

int PtfsHandler::ReadDir(const std::string& relative_path,
			 void *buf, fuse_fill_dir_t filler,
			 off_t offset) {
  // Directory would contain . and ..
  // filler(buf, ".", NULL, 0);
  // filler(buf, "..", NULL, 0);
  struct dirent **namelist{nullptr};
  int scandir_count = scandirat(premount_dirfd_,
				relative_path.c_str(),
				&namelist,
				nullptr,
				nullptr);
  if (scandir_count == -1) {
    return -ENOENT;
  }
  for(int i = 0; i < scandir_count; ++i) {
    filler(buf, namelist[i]->d_name, nullptr, 0);
    free(namelist[i]);
  }
  free(namelist);
  return 0;
}

static int fs_chmod(const char *path, mode_t mode)
{
  if (*path == 0)
    return -ENOENT;
  string relative_path(GetRelativePath(path));
  WRAP_ERRNO(fchmodat(GetContext()->premount_dirfd_,
   		      relative_path.c_str(), mode, 0));
}

static int fs_chown(const char *path, uid_t uid, gid_t gid)
{
  if (*path == 0)
    return -ENOENT;
  string relative_path(GetRelativePath(path));
  WRAP_ERRNO(fchownat(GetContext()->premount_dirfd_,
		      relative_path.c_str(), uid, gid, AT_SYMLINK_NOFOLLOW));
}

static int fs_truncate(const char *path, off_t size) {
  if (*path == 0)
    return -ENOENT;
  string relative_path(GetRelativePath(path));
  ScopedFd fd(openat(GetContext()->premount_dirfd_, relative_path.c_str(), O_WRONLY));
  if (fd.get() == -1) {
    return -errno;
  }
  WRAP_ERRNO(ftruncate(fd.get(), size));
}

static int fs_utimens(const char *path, const struct timespec ts[2]) {
  if (*path == 0)
    return -ENOENT;
  string relative_path(GetRelativePath(path));
  WRAP_ERRNO(utimensat(GetContext()->premount_dirfd_,
		       relative_path.c_str(), ts, AT_SYMLINK_NOFOLLOW));
}

static int fs_mknod(const char *path, mode_t mode, dev_t rdev) {
  if (*path == 0)
    return -ENOENT;
  string relative_path(GetRelativePath(path));
  WRAP_ERRNO(mknodat(GetContext()->premount_dirfd_, relative_path.c_str(), mode, rdev));
}

static int fs_link(const char *from, const char *to) {
  if (*from == 0)
    return -ENOENT;
  if (*to == 0)
    return -ENOENT;
  string from_s(GetRelativePath(from));
  string to_s(GetRelativePath(to));
  WRAP_ERRNO(linkat(GetContext()->premount_dirfd_, from_s.c_str(),
 		    GetContext()->premount_dirfd_, to_s.c_str(), 0));
}

static int fs_statfs(const char *path, struct statvfs *stbuf) {
  WRAP_ERRNO(fstatvfs(GetContext()->premount_dirfd_, stbuf));
}

static int fs_symlink(const char *from, const char *to) {
  if (*from == 0)
    return -ENOENT;
  if (*to == 0)
    return -ENOENT;
  string to_s(GetRelativePath(to));
  WRAP_ERRNO(symlinkat(from, GetContext()->premount_dirfd_, to_s.c_str()));
}

static int fs_getattr(const char *path, struct stat *stbuf) {
  memset(stbuf, 0, sizeof(struct stat));
  if (*path == 0)
    return -ENOENT;
  string relative_path(GetRelativePath(path));
  return GetContext()->GetAttr(relative_path, stbuf);
}

static int fs_opendir(const char* path, struct fuse_file_info* fi) {
  if (*path == 0)
    return -ENOENT;
  unique_ptr<string> relative_path(new string(GetRelativePath(path)));
  fi->fh = reinterpret_cast<uint64_t>(relative_path.release());
  return 0;
}

static int fs_releasedir(const char*, struct fuse_file_info* fi) {
  if (fi->fh == 0)
    return -EBADF;
  unique_ptr<string> auto_delete(reinterpret_cast<string*>(fi->fh));
  return 0;
}

static int fs_readdir(const char *unused, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi) {
  if (fi->fh == 0)
    return -ENOENT;
  string* relative_path(reinterpret_cast<string*>(fi->fh));
  return GetContext()->ReadDir(relative_path->c_str(), buf, filler, offset);
}

static int fs_open(const char *path, struct fuse_file_info *fi) {
  if (*path == 0)
    return -ENOENT;
  string relative_path(GetRelativePath(path));
  unique_ptr<FileHandle> fh(nullptr);
  int ret = GetContext()->Open(relative_path, fi->flags, &fh);
  if (ret == 0) {
    if (fh.get() == nullptr) return -EBADFD;
    fi->fh = reinterpret_cast<uint64_t>(fh.release());
  }
  return ret;
}

static int fs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
  if (*path == 0)
    return -ENOENT;
  string relative_path(GetRelativePath(path));
  unique_ptr<FileHandle> fh(nullptr);
  int ret = GetContext()->Create(relative_path, fi->flags, mode, &fh);
  if (ret == 0) {
    if (fh.get() == nullptr) return -EBADFD;
    fi->fh = reinterpret_cast<uint64_t>(fh.release());
  }
  return ret;
}

static FileHandle* GetFileHandle(struct fuse_file_info* fi) {
  return reinterpret_cast<FileHandle*>(fi->fh);
}

static int fs_release(const char* unused, struct fuse_file_info *fi) {
  unique_ptr<FileHandle> fh(GetFileHandle(fi));
  return GetContext()->Release(fi->flags, &fh);
}

static int fs_read(const char *unused, char *target, size_t size, off_t offset,
		   struct fuse_file_info *fi) {
  FileHandle* fh = GetFileHandle(fi);
  if (fh == nullptr)
    return -ENOENT;
  return GetContext()->Read(*fh, target, size, offset);
}

static int fs_write(const char *unused, const char *buf, size_t size,
		    off_t offset, struct fuse_file_info *fi) {
  FileHandle* fh = GetFileHandle(fi);
  if (fh->fd_get() == -1)
    return -ENOENT;

  return GetContext()->Write(*fh, buf, size, offset);
}

static int fs_readlink(const char *path, char *buf, size_t size) {
  if (*path == 0)
    return -ENOENT;
  string relative_path(GetRelativePath(path));
  int res;
  if ((res = readlinkat(GetContext()->premount_dirfd_,
			relative_path.c_str(), buf, size - 1)) == -1) {
    return -errno;
  } else {
    buf[res] = '\0';
    return 0;
  }
}

static int fs_mkdir(const char *path, mode_t mode) {
  if (*path == 0)
    return -ENOENT;
  string relative_path(GetRelativePath(path));
  WRAP_ERRNO(mkdirat(GetContext()->premount_dirfd_, relative_path.c_str(), mode));
}


static int fs_rmdir(const char *path) {
  if (*path == 0)
    return -ENOENT;
  string relative_path(GetRelativePath(path));
  WRAP_ERRNO(unlinkat(GetContext()->premount_dirfd_, relative_path.c_str(), AT_REMOVEDIR));
}

static int fs_unlink(const char *path) {
  if (*path == 0)
    return -ENOENT;
  string relative_path(GetRelativePath(path));
  return GetContext()->Unlink(relative_path);
}

static int fs_fsync(const char *unused, int isdatasync, struct fuse_file_info *fi) {
  FileHandle* fh = GetFileHandle(fi);
  if (isdatasync) {
    WRAP_ERRNO(fdatasync(fh->fd_get()));
  } else {
    WRAP_ERRNO(fsync(fh->fd_get()));
  }
}

static int fs_fallocate(const char *unused, int mode, off_t offset,
			off_t length, struct fuse_file_info *fi) {
  if (mode) {
    return -EOPNOTSUPP;
  }
  FileHandle* fh = GetFileHandle(fi);
  return -posix_fallocate(fh->fd_get(), offset, length);
}

static int fs_setxattr(const char *path, const char *name, const char *value,
		       size_t size, int flags) {
  if (*path == 0)
    return -ENOENT;
  string relative_path(GetRelativePath(path));
  ScopedFd fd(openat(GetContext()->premount_dirfd_, relative_path.c_str(), O_RDONLY));
  if (fd.get() == -1) {
    return -errno;
  }
  WRAP_ERRNO(fsetxattr(fd.get(), name, value, size, flags));
}

static int fs_getxattr(const char *path, const char *name, char *value,
		       size_t size) {
  if (*path == 0)
    return -ENOENT;
  string relative_path(GetRelativePath(path));
  ScopedFd fd(openat(GetContext()->premount_dirfd_, relative_path.c_str(), O_RDONLY));
  if (fd.get() == -1) {
    return -errno;
  }
  WRAP_ERRNO_OR_RESULT(ssize_t, fgetxattr(fd.get(), name, value, size));
}

static int fs_listxattr(const char *path, char *list, size_t size) {
  if (*path == 0)
    return -ENOENT;
  string relative_path(GetRelativePath(path));
  ScopedFd fd(openat(GetContext()->premount_dirfd_, relative_path.c_str(), O_RDONLY));
  if (fd.get() == -1) {
    return -errno;
  }
  WRAP_ERRNO_OR_RESULT(ssize_t, flistxattr(fd.get(), list, size));
}

static int fs_removexattr(const char *path, const char *name) {
  if (*path == 0)
    return -ENOENT;
  string relative_path(GetRelativePath(path));
  ScopedFd fd(openat(GetContext()->premount_dirfd_, relative_path.c_str(), O_RDONLY));
  if (fd.get() == -1) {
    return -errno;
  }
  WRAP_ERRNO(fremovexattr(fd.get(), name));
}

static int fs_rename(const char *from, const char *to) {
  if (*from == 0)
    return -ENOENT;
  if (*to == 0)
    return -ENOENT;
  string from_s(GetRelativePath(from));
  string to_s(GetRelativePath(to));
  WRAP_ERRNO(renameat(GetContext()->premount_dirfd_, from_s.c_str(),
		      GetContext()->premount_dirfd_, to_s.c_str()));
}

// Up to the caller to initialize this file system.
// static void* fs_init(fuse_conn_info* unused) {
//   return new PtfsHandler();
// }

static void fs_destroy(void* private_data) {
  delete reinterpret_cast<PtfsHandler*>(private_data);
}

void FillFuseOperationsInternal(fuse_operations* o) {
#define DEFINE_HANDLER(n) o->n = &fs_##n
  DEFINE_HANDLER(chmod);
  DEFINE_HANDLER(chown);
  DEFINE_HANDLER(create);
  DEFINE_HANDLER(destroy);
  DEFINE_HANDLER(fallocate);
  DEFINE_HANDLER(fsync);
  DEFINE_HANDLER(getattr);
  DEFINE_HANDLER(getxattr);
  DEFINE_HANDLER(link);
  DEFINE_HANDLER(listxattr);
  DEFINE_HANDLER(mkdir);
  DEFINE_HANDLER(mknod);
  DEFINE_HANDLER(open);
  DEFINE_HANDLER(opendir);
  DEFINE_HANDLER(read);
  DEFINE_HANDLER(readdir);
  DEFINE_HANDLER(readlink);
  DEFINE_HANDLER(release);
  DEFINE_HANDLER(releasedir);
  DEFINE_HANDLER(removexattr);
  DEFINE_HANDLER(rename);
  DEFINE_HANDLER(rmdir);
  DEFINE_HANDLER(setxattr);
  DEFINE_HANDLER(statfs);
  DEFINE_HANDLER(symlink);
  DEFINE_HANDLER(truncate);
  DEFINE_HANDLER(unlink);
  DEFINE_HANDLER(utimens);
  DEFINE_HANDLER(write);
#undef DEFINE_HANDLER
  o->flag_nopath = true;
}

};
