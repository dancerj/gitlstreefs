#define FUSE_USE_VERSION 32

#include "roptfs.h"

#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <memory>

#include <string>

#include "../relative_path.h"

using std::string;
using std::unique_ptr;

namespace roptfs {

// Directory before mount.
int RoptfsHandler::premount_dirfd_ = -1;

RoptfsHandler* GetContext() {
  fuse_context* context = fuse_get_context();
  return reinterpret_cast<RoptfsHandler*>(context->private_data);
}

int RoptfsHandler::GetAttr(const std::string& relative_path,
                           struct stat* stbuf) {
  if (-1 == fstatat(premount_dirfd_, relative_path.c_str(), stbuf,
                    AT_SYMLINK_NOFOLLOW)) {
    return -errno;
  } else {
    return 0;
  }
}

ssize_t RoptfsHandler::Read(const FileHandle& fh, char* target, size_t size,
                            off_t offset) {
  ssize_t n = pread(fh.get(), target, size, offset);
  if (n == -1) return -errno;
  return n;
}

int RoptfsHandler::Open(const std::string& relative_path,
                        unique_ptr<FileHandle>* fh) {
  int fd = openat(premount_dirfd_, relative_path.c_str(), O_RDONLY);
  if (fd == -1) return -ENOENT;
  fh->reset(new FileHandle(fd));

  return 0;
}

int RoptfsHandler::ReadDir(const std::string& relative_path, void* buf,
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

// For path based functions.
#define DECLARE_RELATIVE(path, relative_path) \
  if (*path == 0) return -ENOENT;             \
  string relative_path(GetRelativePath(path));

static int fs_getattr(const char* path, struct stat* stbuf, fuse_file_info*) {
  memset(stbuf, 0, sizeof(struct stat));
  DECLARE_RELATIVE(path, relative_path);
  return GetContext()->GetAttr(relative_path, stbuf);
}

static int fs_opendir(const char* path, struct fuse_file_info* fi) {
  if (*path == 0) return -ENOENT;
  unique_ptr<string> relative_path(new string(GetRelativePath(path)));
  fi->fh = reinterpret_cast<uint64_t>(relative_path.release());
  return 0;
}

static int fs_releasedir(const char*, struct fuse_file_info* fi) {
  if (fi->fh == 0) return -EBADF;
  unique_ptr<string> auto_delete(reinterpret_cast<string*>(fi->fh));
  return 0;
}

static int fs_readdir(const char* unused, void* buf, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info* fi,
                      fuse_readdir_flags) {
  if (fi->fh == 0) return -ENOENT;
  string* relative_path(reinterpret_cast<string*>(fi->fh));
  return GetContext()->ReadDir(relative_path->c_str(), buf, filler, offset);
}

static int fs_open(const char* path, struct fuse_file_info* fi) {
  DECLARE_RELATIVE(path, relative_path);
  unique_ptr<FileHandle> fh(nullptr);
  int ret = GetContext()->Open(relative_path, &fh);
  if (ret == 0) {
    if (fh.get() == nullptr) return -EBADFD;
    fi->fh = reinterpret_cast<uint64_t>(fh.release());
  }
  return ret;
}

static int fs_release(const char* unused, struct fuse_file_info* fi) {
  unique_ptr<FileHandle> auto_deleter(reinterpret_cast<FileHandle*>(fi->fh));
  return 0;
}

static int fs_read(const char* unused, char* target, size_t size, off_t offset,
                   struct fuse_file_info* fi) {
  FileHandle* fh = reinterpret_cast<FileHandle*>(fi->fh);

  if (fh == nullptr) return -ENOENT;
  return GetContext()->Read(*fh, target, size, offset);
}

// Up to the caller to initialize this file system.
// static void* fs_init(fuse_conn_info* unused) {
//   return new RoptfsHandler();
// }

static void fs_destroy(void* private_data) {
  delete reinterpret_cast<RoptfsHandler*>(private_data);
}

void FillFuseOperationsInternal(fuse_operations* o) {
#define DEFINE_HANDLER(n) o->n = &fs_##n
  DEFINE_HANDLER(destroy);
  DEFINE_HANDLER(getattr);
  DEFINE_HANDLER(open);
  DEFINE_HANDLER(opendir);
  DEFINE_HANDLER(read);
  DEFINE_HANDLER(readdir);
  DEFINE_HANDLER(release);
  DEFINE_HANDLER(releasedir);
#undef DEFINE_HANDLER
}

};  // namespace roptfs
