#define FUSE_USE_VERSION 35

#include "git_adapter.h"
#include "git-githubfs.h"

#include <fuse.h>

#include <memory>

namespace git_adapter {
namespace {
// Global scope to make it accessible from callback.
auto fs = std::make_unique<directory_container::DirectoryContainer>();

static int fs_getattr(const char *path, struct stat *stbuf,
                      fuse_file_info *fi) {
  if (fi) {
    auto fe = reinterpret_cast<directory_container::File *>(fi->fh);
    return fe->Getattr(stbuf);
  } else {
    return fs->Getattr(path, stbuf);
  }
}

static int fs_opendir(const char *path, struct fuse_file_info *fi) {
  if (path == 0 || *path != '/') {
    return -ENOENT;
  }
  const auto d =
      dynamic_cast<directory_container::Directory *>(fs->mutable_get(path));
  if (!d) return -ENOENT;
  fi->fh = reinterpret_cast<uint64_t>(d);
  return 0;
}

static int fs_releasedir(const char *, struct fuse_file_info *fi) {
  if (fi->fh == 0) return -EBADF;
  return 0;
}

static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi,
                      fuse_readdir_flags) {
  filler(buf, ".", nullptr, 0, fuse_fill_dir_flags{});
  filler(buf, "..", nullptr, 0, fuse_fill_dir_flags{});
  auto *d = reinterpret_cast<const directory_container::Directory *>(fi->fh);
  if (!d) return -ENOENT;
  d->for_each(
      [&](const std::string &s, const directory_container::File *unused) {
        filler(buf, s.c_str(), nullptr, 0, fuse_fill_dir_flags{});
      });
  return 0;
}

static int fs_open(const char *path, struct fuse_file_info *fi) {
  if (path == 0 || *path != '/') {
    return -ENOENT;
  }

  auto f = fs->mutable_get(path);
  if (!f) return -ENOENT;
  fi->fh = reinterpret_cast<uint64_t>(f);

  f->Open();
  return 0;
}

static int fs_read(const char *path, char *buf, size_t size, off_t offset,
                   struct fuse_file_info *fi) {
  auto fe = reinterpret_cast<directory_container::File *>(fi->fh);
  if (!fe) {
    return -ENOENT;
  }
  return fe->Read(buf, size, offset);
}

static int fs_readlink(const char *path, char *buf, size_t size) {
  if (path == 0 || *path != '/') {
    return -ENOENT;
  }

  auto f = fs->mutable_get(path);
  return f->Readlink(buf, size);
}

static int fs_release(const char *path, struct fuse_file_info *fi) {
  auto fe = reinterpret_cast<directory_container::File *>(fi->fh);
  if (!fe) {
    return -ENOENT;
  }
  return fe->Release();
}

void *fs_init(fuse_conn_info *, fuse_config *config) {
  config->nullpath_ok = 1;
  return nullptr;
}

}  // namespace

struct fuse_operations GetFuseOperations() {
  struct fuse_operations o = {};
#define DEFINE_HANDLER(n) o.n = &fs_##n
  DEFINE_HANDLER(getattr);
  DEFINE_HANDLER(init);
  DEFINE_HANDLER(open);
  DEFINE_HANDLER(opendir);
  DEFINE_HANDLER(read);
  DEFINE_HANDLER(readdir);
  DEFINE_HANDLER(readlink);
  DEFINE_HANDLER(release);
  DEFINE_HANDLER(releasedir);
#undef DEFINE_HANDLER
  return o;
}

directory_container::DirectoryContainer *GetDirectoryContainer() {
  return fs.get();
}
}  // namespace git_adapter
