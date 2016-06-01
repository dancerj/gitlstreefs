#define FUSE_USE_VERSION 26

#include "ptfs.h"

#include <fcntl.h>
#include <string.h>

#include <string>

#include "relative_path.h"
#include "scoped_fd.h"

using std::unique_ptr;
using std::string;

namespace ptfs {

// For path based functions.
#define DECLARE_RELATIVE(path, relative_path)	\
  if (*path == 0)				\
    return -ENOENT;				\
  string relative_path(GetRelativePath(path));	\

// For FileHandle based functions.
#define USE_FILEHANDLE(fh, fi)			\
  FileHandle* fh = GetFileHandle(fi);		\
  if (fh == nullptr || fh->fd_get() == -1)	\
    return -EBADF;				\

PtfsHandler* GetContext() {
  fuse_context* context = fuse_get_context();
  return reinterpret_cast<PtfsHandler*>(context->private_data);
}

static int fs_chmod(const char *path, mode_t mode) {
  DECLARE_RELATIVE(path, relative_path);
  return GetContext()->Chmod(relative_path, mode);
}

static int fs_chown(const char *path, uid_t uid, gid_t gid)
{
  DECLARE_RELATIVE(path, relative_path);
  return GetContext()->Chown(relative_path, uid, gid);
}

static int fs_truncate(const char *path, off_t size) {
  DECLARE_RELATIVE(path, relative_path);
  return GetContext()->Truncate(relative_path, size);
}

static int fs_utimens(const char *path, const struct timespec ts[2]) {
  DECLARE_RELATIVE(path, relative_path);
  return GetContext()->Utimens(relative_path, ts);
}

static int fs_mknod(const char *path, mode_t mode, dev_t rdev) {
  DECLARE_RELATIVE(path, relative_path);
  return GetContext()->Mknod(relative_path, mode, rdev);
}

static int fs_link(const char *from, const char *to) {
  DECLARE_RELATIVE(from, from_s);
  DECLARE_RELATIVE(to, to_s);
  return GetContext()->Link(from_s, to_s);
}


static int fs_statfs(const char *path, struct statvfs *stbuf) {
  return GetContext()->Statfs(stbuf);
}

static int fs_symlink(const char *from, const char *to) {
  DECLARE_RELATIVE(to, to_s);
  if (*from == 0)
    return -ENOENT;
  return GetContext()->Symlink(from, to_s);
}

static int fs_getattr(const char *path, struct stat *stbuf) {
  memset(stbuf, 0, sizeof(struct stat));
  DECLARE_RELATIVE(path, relative_path);
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
  DECLARE_RELATIVE(path, relative_path);
  unique_ptr<FileHandle> fh(nullptr);
  int ret = GetContext()->Open(relative_path, fi->flags, &fh);
  if (fh.get() == nullptr) return -EBADF;
  if (ret == 0) {
    fi->fh = reinterpret_cast<uint64_t>(fh.release());
  }
  return ret;
}

static int fs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
  DECLARE_RELATIVE(path, relative_path);
  unique_ptr<FileHandle> fh(nullptr);
  int ret = GetContext()->Create(relative_path, fi->flags, mode, &fh);
  if (fh.get() == nullptr) return -EBADF;
  if (ret == 0) {
    fi->fh = reinterpret_cast<uint64_t>(fh.release());
  }
  return ret;
}

static FileHandle* GetFileHandle(struct fuse_file_info* fi) {
  return reinterpret_cast<FileHandle*>(fi->fh);
}

static int fs_release(const char* unused, struct fuse_file_info *fi) {
  unique_ptr<FileHandle> fh(GetFileHandle(fi));
  return GetContext()->Release(fi->flags, fh.get());
}

static int fs_read(const char *unused, char *target, size_t size, off_t offset,
		   struct fuse_file_info *fi) {
  USE_FILEHANDLE(fh, fi);
  return GetContext()->Read(*fh, target, size, offset);
}

static int fs_write(const char *unused, const char *buf, size_t size,
		    off_t offset, struct fuse_file_info *fi) {
  USE_FILEHANDLE(fh, fi);
  return GetContext()->Write(*fh, buf, size, offset);
}

static int fs_readlink(const char *path, char *buf, size_t size) {
  DECLARE_RELATIVE(path, relative_path);
  return GetContext()->Readlink(relative_path, buf, size);
}

static int fs_mkdir(const char *path, mode_t mode) {
  DECLARE_RELATIVE(path, relative_path);
  return GetContext()->Mkdir(relative_path, mode);
}

static int fs_rmdir(const char *path) {
  DECLARE_RELATIVE(path, relative_path);
  return GetContext()->Rmdir(relative_path);
}

static int fs_unlink(const char *path) {
  DECLARE_RELATIVE(path, relative_path);
  return GetContext()->Unlink(relative_path);
}

static int fs_fsync(const char *unused, int isdatasync, struct fuse_file_info *fi) {
  USE_FILEHANDLE(fh, fi);
  return GetContext()->Fsync(fh, isdatasync);
}

static int fs_fallocate(const char *unused, int mode, off_t offset,
			off_t length, struct fuse_file_info *fi) {
  USE_FILEHANDLE(fh, fi);
  return GetContext()->Fallocate(fh, mode, offset, length);
}

static int fs_setxattr(const char *path, const char *name, const char *value,
		       size_t size, int flags) {
  DECLARE_RELATIVE(path, relative_path);
  return GetContext()->Setxattr(relative_path, name, value, size, flags);
}

static int fs_getxattr(const char *path, const char *name, char *value,
		       size_t size) {
  DECLARE_RELATIVE(path, relative_path);
  return GetContext()->Getxattr(relative_path, name, value, size);
}

static int fs_listxattr(const char *path, char *list, size_t size) {
  DECLARE_RELATIVE(path, relative_path);
  return GetContext()->Listxattr(relative_path, list, size);
}

static int fs_removexattr(const char *path, const char *name) {
  DECLARE_RELATIVE(path, relative_path);
  return GetContext()->Removexattr(relative_path, name);
}

static int fs_rename(const char *from, const char *to) {
  DECLARE_RELATIVE(from, from_s);
  DECLARE_RELATIVE(to, to_s);
  return GetContext()->Rename(from_s, to_s);
}

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

}  // namespace ptfs
